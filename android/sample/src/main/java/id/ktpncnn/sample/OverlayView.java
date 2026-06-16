package id.ktpncnn.sample;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.View;

public class OverlayView extends View {
    private final Paint dimPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint boxPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF box = new RectF();
    private String status = "Loading model";
    private float score = 0f;
    private boolean hasBox = false;

    public OverlayView(Context context) {
        super(context);
        dimPaint.setColor(Color.argb(26, 0, 0, 0));
        boxPaint.setStyle(Paint.Style.STROKE);
        boxPaint.setStrokeWidth(5f);
        boxPaint.setColor(Color.rgb(28, 190, 130));
        textPaint.setColor(Color.WHITE);
        textPaint.setTextSize(34f);
        textPaint.setFakeBoldText(true);
    }

    public void setStatus(String value) {
        status = value;
        invalidate();
    }

    public void setDetection(float x1, float y1, float x2, float y2, float confidence) {
        box.set(x1, y1, x2, y2);
        score = confidence;
        hasBox = true;
        invalidate();
    }

    public void clearDetection() {
        hasBox = false;
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        canvas.drawRect(0, 0, getWidth(), getHeight(), dimPaint);

        if (hasBox) {
            canvas.drawRoundRect(box, 22f, 22f, boxPaint);
            canvas.drawText("KTP " + Math.round(score * 100f) + "%", box.left + 12f, Math.max(42f, box.top - 16f), textPaint);
        } else {
            Paint guidePaint = new Paint(boxPaint);
            guidePaint.setAlpha(150);
            float w = getWidth() * 0.82f;
            float h = w / 1.586f;
            float x = (getWidth() - w) * 0.5f;
            float y = (getHeight() - h) * 0.5f;
            canvas.drawRoundRect(new RectF(x, y, x + w, y + h), 26f, 26f, guidePaint);
        }

        canvas.drawText(status, 28f, getHeight() - 42f, textPaint);
    }
}
