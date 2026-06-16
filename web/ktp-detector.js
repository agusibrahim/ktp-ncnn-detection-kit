export class KtpDetector {
  constructor(options = {}) {
    this.modelSize = 160;
    this.confidence = options.confidence ?? 0.35;
    this.iou = options.iou ?? 0.45;
    this.ncnnPath = options.ncnnPath ?? "./ncnn";
    this.module = null;
  }

  async init() {
    const factory = globalThis.createKtpNcnnFp16;
    if (typeof factory !== "function") {
      throw new Error("ktp_ncnn_fp16.js must be loaded before ktp-detector.js");
    }
    this.module = await factory({
      locateFile: (path) => `${this.ncnnPath}/${path}`,
    });
    const ret = this.module.ccall(
      "init_model_with_size",
      "number",
      ["string", "string", "number", "number"],
      ["/model/ncnn/model.ncnn.param", "/model/ncnn/model.ncnn.bin", this.modelSize, 1],
    );
    if (ret !== 0) throw new Error(`NCNN init failed ${ret}`);
  }

  async detect(source) {
    if (!this.module) throw new Error("Call init() first");
    const width = source.videoWidth || source.naturalWidth || source.width;
    const height = source.videoHeight || source.naturalHeight || source.height;
    const canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    const ctx = canvas.getContext("2d", { willReadFrequently: true });
    ctx.drawImage(source, 0, 0, width, height);
    return this.detectImageData(ctx.getImageData(0, 0, width, height));
  }

  detectImageData(imageData) {
    if (!this.module) throw new Error("Call init() first");
    const maxDetections = 32;
    const imagePtr = this.module._malloc(imageData.data.byteLength);
    const outPtr = this.module._malloc(maxDetections * 6 * 4);
    try {
      this.module.HEAPU8.set(imageData.data, imagePtr);
      const count = this.module.ccall(
        "detect_rgba",
        "number",
        ["number", "number", "number", "number", "number", "number", "number"],
        [imagePtr, imageData.width, imageData.height, this.confidence, this.iou, outPtr, maxDetections],
      );
      const raw = new Float32Array(this.module.HEAPF32.buffer, outPtr, Math.max(0, count) * 6);
      const detections = [];
      for (let i = 0; i < count; i += 1) {
        const b = i * 6;
        detections.push({
          label: "ktp",
          score: raw[b + 1],
          x1: raw[b + 2],
          y1: raw[b + 3],
          x2: raw[b + 4],
          y2: raw[b + 5],
        });
      }
      return detections;
    } finally {
      this.module._free(imagePtr);
      this.module._free(outPtr);
    }
  }
}
