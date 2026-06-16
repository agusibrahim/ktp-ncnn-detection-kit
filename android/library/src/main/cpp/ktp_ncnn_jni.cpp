#include <android/log.h>
#include <jni.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <net.h>

#define LOG_TAG "KTP_NCNN"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr int kInputSize = 160;

struct Detection {
    float score = 0.f;
    float x1 = 0.f;
    float y1 = 0.f;
    float x2 = 0.f;
    float y2 = 0.f;
};

std::unique_ptr<ncnn::Net> g_net;

float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

float iou(const Detection& a, const Detection& b) {
    const float x1 = std::max(a.x1, b.x1);
    const float y1 = std::max(a.y1, b.y1);
    const float x2 = std::min(a.x2, b.x2);
    const float y2 = std::min(a.y2, b.y2);
    const float inter = std::max(0.f, x2 - x1) * std::max(0.f, y2 - y1);
    const float area_a = std::max(0.f, a.x2 - a.x1) * std::max(0.f, a.y2 - a.y1);
    const float area_b = std::max(0.f, b.x2 - b.x1) * std::max(0.f, b.y2 - b.y1);
    return inter / (area_a + area_b - inter + 1e-6f);
}

std::vector<Detection> nms(std::vector<Detection> detections, float threshold) {
    std::sort(detections.begin(), detections.end(), [](const Detection& a, const Detection& b) {
        return a.score > b.score;
    });

    std::vector<Detection> kept;
    std::vector<char> removed(detections.size(), 0);
    for (size_t i = 0; i < detections.size(); ++i) {
        if (removed[i]) continue;
        kept.push_back(detections[i]);
        for (size_t j = i + 1; j < detections.size(); ++j) {
            if (!removed[j] && iou(detections[i], detections[j]) > threshold) removed[j] = 1;
        }
    }
    return kept;
}

ncnn::Mat make_input(const int* argb, int width, int height, float& ratio, int& pad_x, int& pad_y) {
    ratio = std::min(kInputSize / static_cast<float>(width), kInputSize / static_cast<float>(height));
    const int resized_w = static_cast<int>(std::round(width * ratio));
    const int resized_h = static_cast<int>(std::round(height * ratio));
    pad_x = (kInputSize - resized_w) / 2;
    pad_y = (kInputSize - resized_h) / 2;

    std::vector<unsigned char> canvas(static_cast<size_t>(kInputSize) * kInputSize * 3, 114);
    for (int y = 0; y < resized_h; ++y) {
        int sy = static_cast<int>((y + 0.5f) / ratio - 0.5f);
        sy = std::max(0, std::min(sy, height - 1));
        for (int x = 0; x < resized_w; ++x) {
            int sx = static_cast<int>((x + 0.5f) / ratio - 0.5f);
            sx = std::max(0, std::min(sx, width - 1));
            const int pixel = argb[static_cast<size_t>(sy) * width + sx];
            const size_t dst = (static_cast<size_t>(pad_y + y) * kInputSize + (pad_x + x)) * 3;
            canvas[dst] = static_cast<unsigned char>((pixel >> 16) & 0xff);
            canvas[dst + 1] = static_cast<unsigned char>((pixel >> 8) & 0xff);
            canvas[dst + 2] = static_cast<unsigned char>(pixel & 0xff);
        }
    }

    ncnn::Mat input = ncnn::Mat::from_pixels(canvas.data(), ncnn::Mat::PIXEL_RGB, kInputSize, kInputSize);
    const float norm[3] = {1.f / 255.f, 1.f / 255.f, 1.f / 255.f};
    input.substract_mean_normalize(nullptr, norm);
    return input;
}

std::vector<Detection> parse_output(const ncnn::Mat& out, float ratio, int pad_x, int pad_y, int width, int height, float conf) {
    const bool channels_first = out.h == 5;
    const int rows = channels_first ? out.w : out.h;
    std::vector<Detection> detections;

    for (int i = 0; i < rows; ++i) {
        float cx, cy, bw, bh, score;
        if (channels_first) {
            cx = out.row(0)[i];
            cy = out.row(1)[i];
            bw = out.row(2)[i];
            bh = out.row(3)[i];
            score = out.row(4)[i];
        } else {
            const float* row = out.row(i);
            cx = row[0];
            cy = row[1];
            bw = row[2];
            bh = row[3];
            score = row[4];
        }
        if (score < conf) continue;

        Detection det;
        det.score = score;
        det.x1 = clampf((cx - bw * 0.5f - pad_x) / ratio, 0.f, static_cast<float>(width));
        det.y1 = clampf((cy - bh * 0.5f - pad_y) / ratio, 0.f, static_cast<float>(height));
        det.x2 = clampf((cx + bw * 0.5f - pad_x) / ratio, 0.f, static_cast<float>(width));
        det.y2 = clampf((cy + bh * 0.5f - pad_y) / ratio, 0.f, static_cast<float>(height));
        detections.push_back(det);
    }

    return nms(std::move(detections), 0.45f);
}

} // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_id_ktpncnn_library_KtpNcnn_init(JNIEnv* env, jclass, jstring param_path, jstring bin_path) {
    const char* param = env->GetStringUTFChars(param_path, nullptr);
    const char* bin = env->GetStringUTFChars(bin_path, nullptr);

    g_net = std::make_unique<ncnn::Net>();
    g_net->opt.use_vulkan_compute = false;
    g_net->opt.num_threads = 2;
    g_net->opt.use_fp16_storage = true;
    g_net->opt.use_fp16_arithmetic = true;

    const int p = g_net->load_param(param);
    const int b = p == 0 ? g_net->load_model(bin) : -1;
    env->ReleaseStringUTFChars(param_path, param);
    env->ReleaseStringUTFChars(bin_path, bin);

    if (p != 0 || b != 0) {
        LOGE("load model failed param=%d bin=%d", p, b);
        g_net.reset();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_id_ktpncnn_library_KtpNcnn_detect(JNIEnv* env, jclass, jintArray pixels, jint width, jint height, jfloat conf) {
    if (!g_net || !pixels || width <= 0 || height <= 0) return env->NewFloatArray(0);

    jint* data = env->GetIntArrayElements(pixels, nullptr);
    float ratio = 1.f;
    int pad_x = 0;
    int pad_y = 0;
    ncnn::Mat input = make_input(data, width, height, ratio, pad_x, pad_y);
    env->ReleaseIntArrayElements(pixels, data, JNI_ABORT);

    ncnn::Extractor ex = g_net->create_extractor();
    if (ex.input("in0", input) != 0) return env->NewFloatArray(0);

    ncnn::Mat raw;
    if (ex.extract("out0", raw) != 0) return env->NewFloatArray(0);

    std::vector<Detection> detections = parse_output(raw, ratio, pad_x, pad_y, width, height, conf);
    const int count = std::min(static_cast<int>(detections.size()), 16);
    jfloatArray result = env->NewFloatArray(count * 5);
    if (count == 0) return result;

    std::vector<float> values(static_cast<size_t>(count) * 5);
    for (int i = 0; i < count; ++i) {
        values[i * 5] = detections[i].score;
        values[i * 5 + 1] = detections[i].x1;
        values[i * 5 + 2] = detections[i].y1;
        values[i * 5 + 3] = detections[i].x2;
        values[i * 5 + 4] = detections[i].y2;
    }
    env->SetFloatArrayRegion(result, 0, count * 5, values.data());
    return result;
}
