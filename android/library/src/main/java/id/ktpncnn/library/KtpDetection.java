package id.ktpncnn.library;

public final class KtpDetection {
    public final float score;
    public final float x1;
    public final float y1;
    public final float x2;
    public final float y2;

    public KtpDetection(float score, float x1, float y1, float x2, float y2) {
        this.score = score;
        this.x1 = x1;
        this.y1 = y1;
        this.x2 = x2;
        this.y2 = y2;
    }
}
