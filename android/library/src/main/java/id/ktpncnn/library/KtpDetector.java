package id.ktpncnn.library;

import android.content.Context;
import android.graphics.Bitmap;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

public final class KtpDetector {
    private final float confidence;

    public KtpDetector(Context context) throws IOException {
        this(context, 0.35f);
    }

    public KtpDetector(Context context, float confidence) throws IOException {
        this.confidence = confidence;
        File param = copyAsset(context, "ktp.param");
        File bin = copyAsset(context, "ktp.bin");
        if (!KtpNcnn.init(param.getAbsolutePath(), bin.getAbsolutePath())) {
            throw new IOException("Failed to initialize KTP NCNN model");
        }
    }

    public List<KtpDetection> detect(Bitmap bitmap) {
        Bitmap src = bitmap.getConfig() == Bitmap.Config.ARGB_8888
                ? bitmap
                : bitmap.copy(Bitmap.Config.ARGB_8888, false);
        int width = src.getWidth();
        int height = src.getHeight();
        int[] pixels = new int[width * height];
        src.getPixels(pixels, 0, width, 0, 0, width, height);
        if (src != bitmap) src.recycle();

        float[] raw = KtpNcnn.detect(pixels, width, height, confidence);
        List<KtpDetection> out = new ArrayList<>();
        for (int i = 0; i + 4 < raw.length; i += 5) {
            out.add(new KtpDetection(raw[i], raw[i + 1], raw[i + 2], raw[i + 3], raw[i + 4]));
        }
        return out;
    }

    private static File copyAsset(Context context, String name) throws IOException {
        File out = new File(context.getFilesDir(), name);
        if (out.exists() && out.length() > 0) return out;
        try (InputStream in = context.getAssets().open(name);
             FileOutputStream fos = new FileOutputStream(out)) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) > 0) fos.write(buffer, 0, read);
        }
        return out;
    }
}
