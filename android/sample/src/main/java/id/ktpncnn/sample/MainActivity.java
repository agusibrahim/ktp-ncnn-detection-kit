package id.ktpncnn.sample;

import id.ktpncnn.library.KtpDetection;
import id.ktpncnn.library.KtpDetector;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.Gravity;
import android.view.Surface;
import android.view.TextureView;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import java.io.IOException;
import java.util.Collections;
import java.util.List;

public class MainActivity extends Activity {
    private static final int REQ_CAMERA = 7;
    private static final int DETECT_W = 240;
    private static final int DETECT_H = 320;

    private TextureView preview;
    private OverlayView overlay;
    private HandlerThread cameraThread;
    private Handler cameraHandler;
    private CameraDevice camera;
    private CameraCaptureSession session;
    private KtpDetector detector;
    private volatile boolean detecting = false;
    private long lastDetectMs = 0L;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        buildUi();
        startThread();
        prepareModel();

        if (checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.CAMERA}, REQ_CAMERA);
        } else {
            setupTexture();
        }
    }

    private void buildUi() {
        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(Color.BLACK);
        AspectFrameLayout cameraBox = new AspectFrameLayout(this);
        preview = new TextureView(this);
        overlay = new OverlayView(this);
        cameraBox.addView(preview, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
                Gravity.CENTER));
        cameraBox.addView(overlay, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
                Gravity.CENTER));
        root.addView(cameraBox, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER));
        setContentView(root);
    }

    private void startThread() {
        cameraThread = new HandlerThread("ktp-camera");
        cameraThread.start();
        cameraHandler = new Handler(cameraThread.getLooper());
    }

    private void prepareModel() {
        try {
            detector = new KtpDetector(this, 0.35f);
            overlay.setStatus("NCNN FP16 ready");
        } catch (IOException e) {
            overlay.setStatus("Asset error: " + e.getMessage());
        }
    }

    private void setupTexture() {
        preview.setSurfaceTextureListener(new TextureView.SurfaceTextureListener() {
            @Override public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
                configurePreviewTransform(width, height);
                openCamera();
            }
            @Override public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
                configurePreviewTransform(width, height);
            }
            @Override public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) { return true; }
            @Override public void onSurfaceTextureUpdated(SurfaceTexture surface) { maybeDetect(); }
        });
        if (preview.isAvailable()) openCamera();
    }

    private void openCamera() {
        CameraManager manager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);
        try {
            String selected = null;
            for (String id : manager.getCameraIdList()) {
                Integer facing = manager.getCameraCharacteristics(id).get(CameraCharacteristics.LENS_FACING);
                if (facing != null && facing == CameraCharacteristics.LENS_FACING_BACK) {
                    selected = id;
                    break;
                }
            }
            if (selected == null && manager.getCameraIdList().length > 0) selected = manager.getCameraIdList()[0];
            if (selected == null) {
                overlay.setStatus("No camera");
                return;
            }
            if (checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) return;
            manager.openCamera(selected, new CameraDevice.StateCallback() {
                @Override public void onOpened(CameraDevice cameraDevice) {
                    camera = cameraDevice;
                    startPreview();
                }
                @Override public void onDisconnected(CameraDevice cameraDevice) { cameraDevice.close(); }
                @Override public void onError(CameraDevice cameraDevice, int error) {
                    overlay.setStatus("Camera error " + error);
                    cameraDevice.close();
                }
            }, cameraHandler);
        } catch (CameraAccessException e) {
            overlay.setStatus("Camera access error");
        }
    }

    private void startPreview() {
        try {
            SurfaceTexture texture = preview.getSurfaceTexture();
            if (texture == null || camera == null) return;
            texture.setDefaultBufferSize(480, 640);
            configurePreviewTransform(preview.getWidth(), preview.getHeight());
            Surface surface = new Surface(texture);
            CaptureRequest.Builder builder = camera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            builder.addTarget(surface);
            camera.createCaptureSession(Collections.singletonList(surface), new CameraCaptureSession.StateCallback() {
                @Override public void onConfigured(CameraCaptureSession cameraSession) {
                    session = cameraSession;
                    try {
                        builder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);
                        session.setRepeatingRequest(builder.build(), null, cameraHandler);
                        overlay.setStatus("Point camera at KTP");
                    } catch (CameraAccessException e) {
                        overlay.setStatus("Preview failed");
                    }
                }
                @Override public void onConfigureFailed(CameraCaptureSession cameraSession) {
                    overlay.setStatus("Preview config failed");
                }
            }, cameraHandler);
        } catch (CameraAccessException e) {
            overlay.setStatus("Preview error");
        }
    }

    private void configurePreviewTransform(int viewWidth, int viewHeight) {
        if (viewWidth <= 0 || viewHeight <= 0) return;
        Matrix matrix = new Matrix();
        preview.setTransform(matrix);
    }

    private void maybeDetect() {
        long now = System.currentTimeMillis();
        if (detecting || now - lastDetectMs < 60L) return;
        lastDetectMs = now;
        detecting = true;
        cameraHandler.post(new Runnable() {
            @Override
            public void run() {
            long start = System.nanoTime();
            Bitmap bitmap = preview.getBitmap(DETECT_W, DETECT_H);
            if (bitmap == null) {
                detecting = false;
                return;
            }
            List<KtpDetection> detections = detector != null
                    ? detector.detect(bitmap)
                    : Collections.<KtpDetection>emptyList();
            bitmap.recycle();
            final long ms = (System.nanoTime() - start) / 1_000_000L;
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    applyDetection(detections, ms);
                }
            });
            detecting = false;
            }
        });
    }

    private void applyDetection(List<KtpDetection> detections, long ms) {
        if (!detections.isEmpty()) {
            KtpDetection det = detections.get(0);
            float score = det.score;
            float sx = preview.getWidth() / (float) DETECT_W;
            float sy = preview.getHeight() / (float) DETECT_H;
            overlay.setDetection(det.x1 * sx, det.y1 * sy, det.x2 * sx, det.y2 * sy, score);
            overlay.setStatus("KTP " + Math.round(score * 100f) + "% - " + ms + "ms");
        } else {
            overlay.clearDetection();
            overlay.setStatus("Searching - " + ms + "ms");
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQ_CAMERA && grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            setupTexture();
        } else {
            overlay.setStatus("Camera permission denied");
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        try {
            if (session != null) session.close();
            if (camera != null) camera.close();
        } finally {
            if (cameraThread != null) cameraThread.quitSafely();
        }
    }
}
