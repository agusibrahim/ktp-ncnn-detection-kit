#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#if __has_include(<ncnn/net.h>)
#include <ncnn/net.h>
#else
#include <net.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace fs = std::filesystem;

constexpr int kInputSize = 160;

struct Image {
    int w = 0;
    int h = 0;
    std::vector<unsigned char> rgb;
};

struct Detection {
    float score = 0.f;
    float x1 = 0.f;
    float y1 = 0.f;
    float x2 = 0.f;
    float y2 = 0.f;
};

static float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

static Image load_rgb(const std::string& path) {
    int w = 0, h = 0, c = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &c, 3);
    if (!data) throw std::runtime_error("failed to load image");
    Image img;
    img.w = w;
    img.h = h;
    img.rgb.assign(data, data + static_cast<size_t>(w) * h * 3);
    stbi_image_free(data);
    return img;
}

static void resize_rgb(const Image& src, std::vector<unsigned char>& dst, int dst_w, int dst_h) {
    dst.resize(static_cast<size_t>(dst_w) * dst_h * 3);
    for (int y = 0; y < dst_h; ++y) {
        int sy = std::max(0, std::min(static_cast<int>((y + 0.5f) * src.h / dst_h - 0.5f), src.h - 1));
        for (int x = 0; x < dst_w; ++x) {
            int sx = std::max(0, std::min(static_cast<int>((x + 0.5f) * src.w / dst_w - 0.5f), src.w - 1));
            for (int c = 0; c < 3; ++c) {
                dst[(static_cast<size_t>(y) * dst_w + x) * 3 + c] = src.rgb[(static_cast<size_t>(sy) * src.w + sx) * 3 + c];
            }
        }
    }
}

static ncnn::Mat make_input(const Image& img, float& ratio, int& pad_x, int& pad_y) {
    ratio = std::min(kInputSize / static_cast<float>(img.w), kInputSize / static_cast<float>(img.h));
    int rw = static_cast<int>(std::round(img.w * ratio));
    int rh = static_cast<int>(std::round(img.h * ratio));
    pad_x = (kInputSize - rw) / 2;
    pad_y = (kInputSize - rh) / 2;

    std::vector<unsigned char> resized;
    resize_rgb(img, resized, rw, rh);
    std::vector<unsigned char> canvas(static_cast<size_t>(kInputSize) * kInputSize * 3, 114);
    for (int y = 0; y < rh; ++y) {
        for (int x = 0; x < rw; ++x) {
            size_t dst = (static_cast<size_t>(pad_y + y) * kInputSize + (pad_x + x)) * 3;
            size_t src = (static_cast<size_t>(y) * rw + x) * 3;
            canvas[dst] = resized[src];
            canvas[dst + 1] = resized[src + 1];
            canvas[dst + 2] = resized[src + 2];
        }
    }

    ncnn::Mat in = ncnn::Mat::from_pixels(canvas.data(), ncnn::Mat::PIXEL_RGB, kInputSize, kInputSize);
    const float norm[3] = {1.f / 255.f, 1.f / 255.f, 1.f / 255.f};
    in.substract_mean_normalize(nullptr, norm);
    return in;
}

static float iou(const Detection& a, const Detection& b) {
    float x1 = std::max(a.x1, b.x1), y1 = std::max(a.y1, b.y1);
    float x2 = std::min(a.x2, b.x2), y2 = std::min(a.y2, b.y2);
    float inter = std::max(0.f, x2 - x1) * std::max(0.f, y2 - y1);
    float aa = std::max(0.f, a.x2 - a.x1) * std::max(0.f, a.y2 - a.y1);
    float ab = std::max(0.f, b.x2 - b.x1) * std::max(0.f, b.y2 - b.y1);
    return inter / (aa + ab - inter + 1e-6f);
}

static std::vector<Detection> nms(std::vector<Detection> dets, float threshold) {
    std::sort(dets.begin(), dets.end(), [](const Detection& a, const Detection& b) { return a.score > b.score; });
    std::vector<Detection> out;
    for (const auto& d : dets) {
        bool keep = true;
        for (const auto& k : out) {
            if (iou(d, k) > threshold) {
                keep = false;
                break;
            }
        }
        if (keep) out.push_back(d);
    }
    return out;
}

static std::vector<Detection> parse(const ncnn::Mat& raw, const Image& img, float ratio, int pad_x, int pad_y, float conf) {
    bool channels_first = raw.h == 5;
    int rows = channels_first ? raw.w : raw.h;
    std::vector<Detection> dets;
    for (int i = 0; i < rows; ++i) {
        float cx, cy, bw, bh, score;
        if (channels_first) {
            cx = raw.row(0)[i]; cy = raw.row(1)[i]; bw = raw.row(2)[i]; bh = raw.row(3)[i]; score = raw.row(4)[i];
        } else {
            const float* r = raw.row(i);
            cx = r[0]; cy = r[1]; bw = r[2]; bh = r[3]; score = r[4];
        }
        if (score < conf) continue;
        dets.push_back({
            score,
            clampf((cx - bw * 0.5f - pad_x) / ratio, 0.f, static_cast<float>(img.w)),
            clampf((cy - bh * 0.5f - pad_y) / ratio, 0.f, static_cast<float>(img.h)),
            clampf((cx + bw * 0.5f - pad_x) / ratio, 0.f, static_cast<float>(img.w)),
            clampf((cy + bh * 0.5f - pad_y) / ratio, 0.f, static_cast<float>(img.h)),
        });
    }
    return nms(std::move(dets), 0.45f);
}

static void draw_rect(Image& img, const Detection& d) {
    int x1 = static_cast<int>(d.x1), y1 = static_cast<int>(d.y1);
    int x2 = static_cast<int>(d.x2), y2 = static_cast<int>(d.y2);
    auto set = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= img.w || y >= img.h) return;
        size_t p = (static_cast<size_t>(y) * img.w + x) * 3;
        img.rgb[p] = 20; img.rgb[p + 1] = 220; img.rgb[p + 2] = 140;
    };
    for (int t = 0; t < 4; ++t) {
        for (int x = x1; x <= x2; ++x) { set(x, y1 + t); set(x, y2 - t); }
        for (int y = y1; y <= y2; ++y) { set(x1 + t, y); set(x2 - t, y); }
    }
}

static void crop_write(const Image& img, const Detection& d, const std::string& path) {
    int x1 = static_cast<int>(std::floor(d.x1));
    int y1 = static_cast<int>(std::floor(d.y1));
    int x2 = static_cast<int>(std::ceil(d.x2));
    int y2 = static_cast<int>(std::ceil(d.y2));
    x1 = std::max(0, std::min(x1, img.w - 1));
    y1 = std::max(0, std::min(y1, img.h - 1));
    x2 = std::max(x1 + 1, std::min(x2, img.w));
    y2 = std::max(y1 + 1, std::min(y2, img.h));
    int cw = x2 - x1, ch = y2 - y1;
    std::vector<unsigned char> crop(static_cast<size_t>(cw) * ch * 3);
    for (int y = 0; y < ch; ++y) {
        std::copy_n(&img.rgb[(static_cast<size_t>(y1 + y) * img.w + x1) * 3], cw * 3, &crop[static_cast<size_t>(y) * cw * 3]);
    }
    stbi_write_png(path.c_str(), cw, ch, 3, crop.data(), cw * 3);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: ktp_detect <image> <model_dir> [--mark out.png] [--crop crop.png] [--conf 0.35]\n";
        return 2;
    }

    std::string image_path = argv[1];
    fs::path model_dir = argv[2];
    std::string mark_path;
    std::string crop_path;
    float conf = 0.35f;
    for (int i = 3; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--mark" && i + 1 < argc) mark_path = argv[++i];
        else if (a == "--crop" && i + 1 < argc) crop_path = argv[++i];
        else if (a == "--conf" && i + 1 < argc) conf = std::stof(argv[++i]);
    }

    Image img = load_rgb(image_path);
    ncnn::Net net;
    net.opt.use_vulkan_compute = false;
    net.opt.num_threads = 2;
    net.opt.use_fp16_storage = true;
    net.load_param((model_dir / "ktp_fp16.ncnn.param").string().c_str());
    net.load_model((model_dir / "ktp_fp16.ncnn.bin").string().c_str());

    float ratio = 1.f;
    int pad_x = 0, pad_y = 0;
    ncnn::Mat input = make_input(img, ratio, pad_x, pad_y);
    ncnn::Extractor ex = net.create_extractor();
    ex.input("in0", input);
    ncnn::Mat raw;
    ex.extract("out0", raw);
    auto dets = parse(raw, img, ratio, pad_x, pad_y, conf);

    std::cout << "detections=" << dets.size() << "\n";
    for (const auto& d : dets) {
        std::cout << "ktp " << d.score << " " << d.x1 << " " << d.y1 << " " << d.x2 << " " << d.y2 << "\n";
    }

    if (!dets.empty()) {
        if (!crop_path.empty()) crop_write(img, dets[0], crop_path);
        if (!mark_path.empty()) {
            for (const auto& d : dets) draw_rect(img, d);
            stbi_write_png(mark_path.c_str(), img.w, img.h, 3, img.rgb.data(), img.w * 3);
        }
    }
    return 0;
}
