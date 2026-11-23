#!/usr/bin/env python3
"""
============================================================================
EDGE MODEL EXPORTER FOR QNX NEUTRINO RTOS & EMBEDDED RASPBERRY PI 4
============================================================================
Converts PyTorch .pt -> ONNX (.onnx) -> TFLite (.tflite)
============================================================================
"""

import os
import sys
import shutil

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODELS_DIR = os.path.join(BASE_DIR, "models")
PT_PATH = os.path.join(MODELS_DIR, "best.pt")

def main():
    print("=" * 65)
    print("🚗 QNX SDV Model Exporter (det10 - 10 Traffic Sign Classes)")
    print("=" * 65)

    if not os.path.exists(PT_PATH):
        print(f"❌ Error: Model not found at {PT_PATH}")
        sys.exit(1)

    try:
        from ultralytics import YOLO
    except ImportError:
        print("❌ Error: ultralytics is required.")
        sys.exit(1)

    model = YOLO(PT_PATH)
    print(f"Loaded model with {len(model.names)} classes:")
    for idx, name in model.names.items():
        print(f"  {idx}: {name}")

    # 1. Export to ONNX (Optimized for OpenCV DNN and QNX CPU runtime)
    print("\n[1/2] Exporting to ONNX format (320x320 with onnxslim)...")
    onnx_file = model.export(format="onnx", imgsz=320, simplify=True)
    dest_onnx = os.path.join(MODELS_DIR, "best.onnx")
    if onnx_file and os.path.exists(onnx_file) and onnx_file != dest_onnx:
        shutil.copy(onnx_file, dest_onnx)
    print(f"✅ ONNX Model saved at: {dest_onnx} ({os.path.getsize(dest_onnx)/(1024*1024):.2f} MB)")

    # 2. Export to TFLite (Optimized for ARM Cortex-A72 LiteRT on QNX)
    print("\n[2/2] Exporting to TFLite format (320x320 for embedded RTOS)...")
    try:
        tflite_file = model.export(format="tflite", imgsz=320)
        dest_tflite = os.path.join(MODELS_DIR, "best.tflite")
        if tflite_file and os.path.exists(tflite_file) and tflite_file != dest_tflite:
            shutil.copy(tflite_file, dest_tflite)
        print(f"✅ TFLite Model saved at: {dest_tflite} ({os.path.getsize(dest_tflite)/(1024*1024):.2f} MB)")
    except Exception as e:
        print(f"⚠️  TFLite export note: {e}")

    # 3. Synchronize labels.txt
    labels_file = os.path.join(MODELS_DIR, "labels.txt")
    with open(labels_file, "w") as f:
        for idx in range(len(model.names)):
            f.write(f"{model.names[idx]}\n")
    print(f"✅ Updated labels list at: {labels_file}")
    print("\n🎯 All edge models successfully synchronized with QNX vehicle workspace!")
    print("=" * 65)

if __name__ == "__main__":
    main()
