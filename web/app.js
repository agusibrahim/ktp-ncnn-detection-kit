import { KtpDetector } from "./ktp-detector.js";

const statusEl = document.getElementById("status");
const resultEl = document.getElementById("result");
const video = document.getElementById("video");
const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");
const cameraBtn = document.getElementById("cameraBtn");
const stopBtn = document.getElementById("stopBtn");
const fileInput = document.getElementById("fileInput");

const state = {
  detector: null,
  ready: false,
  stream: null,
  image: null,
  tracking: false,
  detecting: false,
  detections: [],
  lastDetectAt: 0,
};

function setStatus(text) {
  statusEl.textContent = text;
}

function sourceSize(source) {
  return {
    width: source.videoWidth || source.naturalWidth || source.width,
    height: source.videoHeight || source.naturalHeight || source.height,
  };
}

function activeSource() {
  if (state.tracking && video.readyState >= 2) return video;
  return state.image;
}

async function loadModel() {
  state.detector = new KtpDetector({ confidence: 0.35, iou: 0.45 });
  await state.detector.init();
  state.ready = true;
  setStatus("Model ready");
  draw();
}

function resizeCanvas() {
  const rect = canvas.getBoundingClientRect();
  const dpr = devicePixelRatio || 1;
  canvas.width = Math.round(rect.width * dpr);
  canvas.height = Math.round(rect.height * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  draw();
}

async function detect(source) {
  if (!state.ready || state.detecting || !source) return;
  state.detecting = true;
  const start = performance.now();
  try {
    const { width, height } = sourceSize(source);
    const scratch = document.createElement("canvas");
    scratch.width = width;
    scratch.height = height;
    const sctx = scratch.getContext("2d", { willReadFrequently: true });
    sctx.drawImage(source, 0, 0, width, height);
    state.detections = state.detector.detectImageData(sctx.getImageData(0, 0, width, height));
    resultEl.textContent = `${state.detections.length} detection(s) - ${Math.round(performance.now() - start)}ms`;
    draw();
  } finally {
    state.detecting = false;
  }
}

function draw() {
  const w = canvas.clientWidth;
  const h = canvas.clientHeight;
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, w, h);

  const source = activeSource();
  if (!source) {
    ctx.strokeStyle = "rgba(255,255,255,.35)";
    ctx.lineWidth = 2;
    ctx.strokeRect(w * 0.09, h * 0.33, w * 0.82, w * 0.52);
    return;
  }

  const size = sourceSize(source);
  const scale = Math.min(w / size.width, h / size.height);
  const dw = size.width * scale;
  const dh = size.height * scale;
  const ox = (w - dw) / 2;
  const oy = (h - dh) / 2;
  ctx.drawImage(source, ox, oy, dw, dh);

  ctx.fillStyle = "rgba(0,0,0,.10)";
  ctx.fillRect(0, 0, w, h);

  for (const det of state.detections) {
    const x = ox + det.x1 * scale;
    const y = oy + det.y1 * scale;
    const bw = (det.x2 - det.x1) * scale;
    const bh = (det.y2 - det.y1) * scale;
    ctx.strokeStyle = "#21c786";
    ctx.lineWidth = 4;
    ctx.strokeRect(x, y, bw, bh);
    ctx.fillStyle = "#21c786";
    ctx.fillText(`KTP ${Math.round(det.score * 100)}%`, x + 8, Math.max(20, y - 8));
  }
}

async function startCamera() {
  const stream = await navigator.mediaDevices.getUserMedia({
    video: { facingMode: { ideal: "environment" }, width: { ideal: 640 }, height: { ideal: 480 } },
    audio: false,
  });
  state.stream = stream;
  video.srcObject = stream;
  await video.play();
  state.image = null;
  state.tracking = true;
  cameraBtn.disabled = true;
  stopBtn.disabled = false;
  requestAnimationFrame(loop);
}

function stopCamera() {
  state.tracking = false;
  if (state.stream) for (const track of state.stream.getTracks()) track.stop();
  state.stream = null;
  video.srcObject = null;
  cameraBtn.disabled = false;
  stopBtn.disabled = true;
}

function loop(now) {
  if (!state.tracking) return;
  draw();
  if (now - state.lastDetectAt > 90) {
    state.lastDetectAt = now;
    detect(video);
  }
  requestAnimationFrame(loop);
}

fileInput.addEventListener("change", async () => {
  const file = fileInput.files && fileInput.files[0];
  if (!file) return;
  stopCamera();
  const image = new Image();
  image.onload = async () => {
    state.image = image;
    await detect(image);
  };
  image.src = URL.createObjectURL(file);
});

cameraBtn.addEventListener("click", startCamera);
stopBtn.addEventListener("click", stopCamera);
window.addEventListener("resize", resizeCanvas);
resizeCanvas();
loadModel().catch((err) => {
  console.error(err);
  setStatus("Model failed");
  resultEl.textContent = err.message;
});
