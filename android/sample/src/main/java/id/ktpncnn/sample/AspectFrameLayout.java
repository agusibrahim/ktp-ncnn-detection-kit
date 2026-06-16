package id.ktpncnn.sample;

import android.content.Context;
import android.widget.FrameLayout;

public class AspectFrameLayout extends FrameLayout {
    private static final float ASPECT = 3f / 4f;

    public AspectFrameLayout(Context context) {
        super(context);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = Math.round(width / ASPECT);
        int exactW = MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY);
        int exactH = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY);
        super.onMeasure(exactW, exactH);
    }
}
