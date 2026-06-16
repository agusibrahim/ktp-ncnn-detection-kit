#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/ncnn-android"
URL="https://github.com/Tencent/ncnn/releases/download/20260526/ncnn-20260526-android-vulkan.zip"
ZIP="$ROOT/ncnn-20260526-android-vulkan.zip"

if [ -f "$OUT/ncnn_version_20260526.txt" ]; then
  echo "[+] NCNN Android SDK already exists: $OUT"
  exit 0
fi

rm -rf "$OUT" "$ROOT/ncnn-20260526-android-vulkan"
echo "[+] Downloading NCNN Android SDK"
curl -L "$URL" -o "$ZIP"
echo "[+] Extracting NCNN Android SDK"
unzip -q "$ZIP" -d "$ROOT"
mv "$ROOT/ncnn-20260526-android-vulkan" "$OUT"
touch "$OUT/ncnn_version_20260526.txt"
rm -f "$ZIP"
echo "[✓] NCNN Android SDK ready: $OUT"

