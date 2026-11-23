#!/usr/bin/env python3
"""
============================================================================
CLI PRODUCTION TRAINING ENGINE FOR det10 TRAFFIC SIGN MODEL
============================================================================
"""

import os
import sys
import argparse
from pathlib import Path

try:
    from ultralytics import YOLO
except ImportError:
    print("❌ Error: ultralytics is required. Run: pip install ultralytics")
    sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Production Training Pipeline for QNX SDV det10 Model")
    parser.add_argument("--data", type=str, default="ml_pipeline/det10_dataset.yaml", help="Path to det10_dataset.yaml")
    parser.add_argument("--weights", type=str, default="yolov8s.pt", help="Base model weights")
    parser.add_argument("--epochs", type=int, default=50, help="Number of training epochs")
    parser.add_argument("--batch", type=int, default=32, help="Batch size")
    parser.add_argument("--imgsz", type=int, default=384, help="Input image size")
    parser.add_argument("--device", type=str, default="0", help="GPU device ID or 'cpu'")
    parser.add_argument("--output-dir", type=str, default="runs_det10", help="Output directory")
    args = parser.parse_args()

    print("=" * 70)
    print("🚀 Commencing Production Training for 10-Class Road Sign Model (det10)")
    print(f"Data Config : {args.data}")
    print(f"Base Weights: {args.weights}")
    print(f"Epochs      : {args.epochs} | Batch: {args.batch} | Image Size: {args.imgsz}")
    print(f"Device      : {args.device}")
    print("=" * 70)

    model = YOLO(args.weights)
    results = model.train(
        data=args.data,
        epochs=args.epochs,
        batch=args.batch,
        imgsz=args.imgsz,
        device=args.device,
        optimizer="AdamW",
        lr0=0.001,
        lrf=0.01,
        cos_lr=True,
        weight_decay=0.0005,
        warmup_epochs=3,
        cls=2.5,        # High classification loss weight
        box=7.5,        # Bounding box loss
        dfl=1.5,
        augment=True,
        fliplr=0.0,     # Strict directional sign semantics
        flipud=0.0,
        degrees=10.0,
        scale=0.25,
        mosaic=0.7,
        close_mosaic=10,
        project=args.output_dir,
        name="det10_production",
        exist_ok=True,
        verbose=True
    )

    best_pt = os.path.join(args.output_dir, "det10_production/weights/best.pt")
    if os.path.exists(best_pt):
        print(f"\n✅ Training complete! Best weights saved to: {best_pt}")
        dest_model = "models/best.pt"
        import shutil
        shutil.copy(best_pt, dest_model)
        print(f"📦 Synchronized weights to vehicle models dir: {dest_model}")

if __name__ == "__main__":
    main()
