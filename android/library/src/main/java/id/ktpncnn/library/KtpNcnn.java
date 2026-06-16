package id.ktpncnn.library;

public final class KtpNcnn {
    static {
        System.loadLibrary("ktp_ncnn");
    }

    private KtpNcnn() {}

    public static native boolean init(String paramPath, String binPath);

    public static native float[] detect(int[] pixels, int width, int height, float conf);
}
