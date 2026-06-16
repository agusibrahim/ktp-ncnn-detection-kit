# KTP NCNN Detector

KTP detector using NCNN. Isi repo:

- `android/library`: Android AAR library with JNI + NCNN.
- `android/sample`: simple live camera Android sample.
- `web`: browser sample for camera/upload.
- `cli`: native command line detector for marking and cropping detections.
- `model`: NCNN model files.

The bundled model detects one class: `ktp`. Input size is `160x160`.

## Android Library

Build the AAR and sample APK:

```bash
cd android
./scripts/setup_ncnn_android.sh
./gradlew :library:assembleRelease :sample:assembleRelease
```

Outputs:

- AAR: `android/library/build/outputs/aar/library-release.aar`
- APK: `android/sample/build/outputs/apk/release/sample-release.apk`

Use the AAR in another Android project:

```gradle
dependencies {
    implementation files("libs/ktp-ncnn-library.aar")
}
```

Minimal Java usage:

```java
import id.ktpncnn.library.KtpDetection;
import id.ktpncnn.library.KtpDetector;

KtpDetector detector = new KtpDetector(context, 0.35f);
List<KtpDetection> detections = detector.detect(bitmap);

for (KtpDetection det : detections) {
    float x1 = det.x1;
    float y1 = det.y1;
    float x2 = det.x2;
    float y2 = det.y2;
    float score = det.score;
}
```

The AAR already contains `ktp.param`, `ktp.bin`, and the native `libktp_ncnn.so`. Apps only need camera permission if they use live camera input.

## Web

Serve the `web` folder from any static server:

```bash
cd web
python3 -m http.server 8000
```

Open `http://127.0.0.1:8000`. The sample supports camera and image upload.

Use it in another browser project:

```html
<script src="/ncnn/ktp_ncnn_fp16.js"></script>
<script type="module">
  import { KtpDetector } from "/ktp-detector.js";

  const detector = new KtpDetector({
    confidence: 0.35,
    iou: 0.45,
    ncnnPath: "/ncnn",
  });
  await detector.init();

  const detections = await detector.detect(videoOrImageElement);
  console.log(detections);
</script>
```

Files for another web project:

- `web/ktp-detector.js`
- `web/ncnn/ktp_ncnn_fp16.js`
- `web/ncnn/ktp_ncnn_fp16.wasm`
- `web/ncnn/ktp_ncnn_fp16.data`

Camera access requires HTTPS or `localhost`.

## CLI

Install dependencies on macOS:

```bash
brew install cmake ncnn libomp
```

Build:

```bash
cmake -S cli -B cli/build -DCMAKE_BUILD_TYPE=Release
cmake --build cli/build --config Release
```

Run detection, draw a marked image, and crop the best KTP:

```bash
./cli/build/ktp_detect image.jpg model \
  --conf 0.35 \
  --mark marked.jpg \
  --crop ktp-crop.jpg
```

The second argument is the model directory containing the NCNN param and bin files.

## Build Otomatis

Repo ini juga menyiapkan build otomatis:

- builds Android AAR and sample APK;
- zips the web sample;
- uploads build artifacts;
- publishes `web/` to GitHub Pages on `main`;
- attaches AAR/APK/Web ZIP to Releases when pushing a tag like `v1.0.0`.

Pages source should use workflow mode.

## License Note

The model metadata reports an Ultralytics AGPL-3.0 license. Check license compatibility before embedding this in a closed-source product.
