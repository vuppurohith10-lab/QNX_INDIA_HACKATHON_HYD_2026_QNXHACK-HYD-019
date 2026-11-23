#!/usr/bin/env python3
"""
============================================================================
HIGH-FIDELITY SYNTHETIC & AUGMENTED DATASET GENERATOR FOR det10 SIGNS
QNX Global E-Hack 2026 - Autonomous SDV System
============================================================================
Generates thousands of realistic, photometrically varied road scenes with
accurate ground-truth bounding boxes for all 10 det10 classes:
  0: stop
  1: no_entry
  2: speed_limit_30
  3: school_ahead
  4: pedestrian_crossing
  5: slippery_road
  6: right_hand_curve
  7: give_way
  8: no_parking
  9: narrow_road_ahead
============================================================================
"""

import os
import sys
import random
import math
import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageEnhance
from pathlib import Path

# The 10 Target Classes
CLASSES = [
    "stop",
    "no_entry",
    "speed_limit_30",
    "school_ahead",
    "pedestrian_crossing",
    "slippery_road",
    "right_hand_curve",
    "give_way",
    "no_parking",
    "narrow_road_ahead"
]

def draw_sign_icon(class_id, size=200):
    """Render an authentic road sign image of specified size (RGBA)"""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    center = size // 2
    r = size // 2 - 4

    # 0: STOP (Red Octagon)
    if class_id == 0:
        points = []
        for i in range(8):
            angle = math.radians(22.5 + i * 45)
            x = center + r * math.cos(angle)
            y = center + r * math.sin(angle)
            points.append((x, y))
        draw.polygon(points, fill=(210, 20, 20, 255), outline=(255, 255, 255, 255), width=max(2, size // 30))
        # White inner text or border
        draw.rectangle([center - r*0.5, center - r*0.18, center + r*0.5, center + r*0.18], fill=(255, 255, 255, 255))
        draw.text((center - r*0.35, center - r*0.15), "STOP", fill=(210, 20, 20, 255))

    # 1: NO ENTRY (Red Circle with White Horizontal Bar)
    elif class_id == 1:
        draw.ellipse([center - r, center - r, center + r, center + r],
                     fill=(210, 20, 20, 255), outline=(255, 255, 255, 255), width=max(2, size // 30))
        bar_w = int(r * 1.5)
        bar_h = int(r * 0.35)
        draw.rectangle([center - bar_w // 2, center - bar_h // 2,
                        center + bar_w // 2, center + bar_h // 2], fill=(255, 255, 255, 255))

    # 2: SPEED LIMIT 30 (White Circle with Red Border and "30")
    elif class_id == 2:
        draw.ellipse([center - r, center - r, center + r, center + r],
                     fill=(245, 245, 245, 255), outline=(210, 20, 20, 255), width=max(4, size // 10))
        # Draw "30" text
        draw.text((center - r*0.35, center - r*0.25), "30", fill=(20, 20, 20, 255))

    # 3: SCHOOL AHEAD (Yellow Triangle with Children)
    elif class_id == 3:
        p1 = (center, center - r)
        p2 = (center - int(r * 0.866), center + int(r * 0.6))
        p3 = (center + int(r * 0.866), center + int(r * 0.6))
        draw.polygon([p1, p2, p3], fill=(250, 200, 20, 255), outline=(210, 20, 20, 255), width=max(3, size // 16))
        # Silhouette
        draw.ellipse([center - r*0.15, center - r*0.2, center + r*0.15, center + r*0.1], fill=(20, 20, 20, 255))
        draw.polygon([(center, center), (center - r*0.25, center + r*0.4), (center + r*0.25, center + r*0.4)], fill=(20, 20, 20, 255))

    # 4: PEDESTRIAN CROSSING (Blue Square with White Triangle & Walking Figure)
    elif class_id == 4:
        draw.rectangle([center - r, center - r, center + r, center + r],
                       fill=(20, 80, 210, 255), outline=(255, 255, 255, 255), width=max(2, size // 25))
        p1 = (center, center - int(r * 0.8))
        p2 = (center - int(r * 0.75), center + int(r * 0.75))
        p3 = (center + int(r * 0.75), center + int(r * 0.75))
        draw.polygon([p1, p2, p3], fill=(255, 255, 255, 255))
        # Pedestrian silhouette
        draw.ellipse([center - r*0.1, center - r*0.4, center + r*0.1, center - r*0.2], fill=(20, 20, 20, 255))
        draw.line([center, center - r*0.2, center, center + r*0.2], fill=(20, 20, 20, 255), width=max(2, size // 20))
        draw.line([center, center + r*0.2, center - r*0.2, center + r*0.5], fill=(20, 20, 20, 255), width=max(2, size // 20))
        draw.line([center, center + r*0.2, center + r*0.2, center + r*0.5], fill=(20, 20, 20, 255), width=max(2, size // 20))

    # 5: SLIPPERY ROAD (Yellow/White Triangle with Skidding Car Tracks)
    elif class_id == 5:
        p1 = (center, center - r)
        p2 = (center - int(r * 0.866), center + int(r * 0.6))
        p3 = (center + int(r * 0.866), center + int(r * 0.6))
        draw.polygon([p1, p2, p3], fill=(255, 255, 255, 255), outline=(210, 20, 20, 255), width=max(3, size // 14))
        # Car & tracks
        draw.rectangle([center - r*0.3, center - r*0.1, center + r*0.3, center + r*0.15], fill=(20, 20, 20, 255))
        # Skidding track arcs
        draw.arc([center - r*0.4, center + r*0.1, center, center + r*0.45], 0, 180, fill=(20, 20, 20, 255), width=2)
        draw.arc([center, center + r*0.1, center + r*0.4, center + r*0.45], 180, 360, fill=(20, 20, 20, 255), width=2)

    # 6: RIGHT HAND CURVE (Yellow/White Triangle with Sharp Arrow Bending Right)
    elif class_id == 6:
        p1 = (center, center - r)
        p2 = (center - int(r * 0.866), center + int(r * 0.6))
        p3 = (center + int(r * 0.866), center + int(r * 0.6))
        draw.polygon([p1, p2, p3], fill=(255, 255, 255, 255), outline=(210, 20, 20, 255), width=max(3, size // 14))
        # Right curve arrow
        draw.line([center - r*0.15, center + r*0.35, center - r*0.15, center, center + r*0.2, center - r*0.1],
                  fill=(20, 20, 20, 255), width=max(3, size // 15), joint="curve")
        draw.polygon([(center + r*0.2, center - r*0.25), (center + r*0.35, center - r*0.05), (center + r*0.05, center - r*0.05)],
                     fill=(20, 20, 20, 255))

    # 7: GIVE WAY / YIELD (Inverted White Triangle with Red Border)
    elif class_id == 7:
        p1 = (center, center + r)
        p2 = (center - int(r * 0.866), center - int(r * 0.6))
        p3 = (center + int(r * 0.866), center - int(r * 0.6))
        draw.polygon([p1, p2, p3], fill=(255, 255, 255, 255), outline=(210, 20, 20, 255), width=max(4, size // 12))
        draw.text((center - r*0.35, center - r*0.25), "YIELD", fill=(210, 20, 20, 255))

    # 8: NO PARKING (Blue Circle with Red Border & Diagonal Slash)
    elif class_id == 8:
        draw.ellipse([center - r, center - r, center + r, center + r],
                     fill=(20, 80, 210, 255), outline=(210, 20, 20, 255), width=max(4, size // 10))
        # Red diagonal slash
        draw.line([center - int(r * 0.707), center - int(r * 0.707),
                   center + int(r * 0.707), center + int(r * 0.707)],
                  fill=(210, 20, 20, 255), width=max(3, size // 14))
        draw.text((center - r*0.2, center - r*0.3), "P", fill=(255, 255, 255, 255))

    # 9: NARROW ROAD AHEAD (Yellow/White Triangle with Road Narrowing)
    elif class_id == 9:
        p1 = (center, center - r)
        p2 = (center - int(r * 0.866), center + int(r * 0.6))
        p3 = (center + int(r * 0.866), center + int(r * 0.6))
        draw.polygon([p1, p2, p3], fill=(255, 255, 255, 255), outline=(210, 20, 20, 255), width=max(3, size // 14))
        # Left and right constriction bars
        draw.line([center - r*0.35, center + r*0.4, center - r*0.35, center + r*0.15, center - r*0.15, center - r*0.2],
                  fill=(20, 20, 20, 255), width=max(3, size // 16))
        draw.line([center + r*0.35, center + r*0.4, center + r*0.35, center + r*0.15, center + r*0.15, center - r*0.2],
                  fill=(20, 20, 20, 255), width=max(3, size // 16))

    return img

def generate_background_scene(width=640, height=480):
    """Generate realistic road, sky, and horizon textures"""
    img = np.zeros((height, width, 3), dtype=np.uint8)

    # Sky color variation (sunny blue, cloudy grey, sunset, or dusk)
    sky_type = random.choice(["day", "cloudy", "dusk"])
    horizon_y = random.randint(int(height * 0.35), int(height * 0.50))

    if sky_type == "day":
        sky_color = [random.randint(180, 240), random.randint(150, 200), random.randint(100, 160)] # BGR
    elif sky_type == "cloudy":
        sky_color = [random.randint(140, 180), random.randint(140, 180), random.randint(140, 180)]
    else:
        sky_color = [random.randint(100, 150), random.randint(100, 130), random.randint(160, 220)]

    img[0:horizon_y, :] = sky_color

    # Road surface (asphalt / concrete with noise)
    road_base = random.randint(45, 80)
    road_color = np.array([road_base, road_base, road_base], dtype=np.uint8)
    img[horizon_y:, :] = road_color

    # Add realistic grain / asphalt texture
    noise = np.random.normal(0, 10, (height - horizon_y, width, 3)).astype(np.int16)
    road_region = img[horizon_y:, :].astype(np.int16) + noise
    img[horizon_y:, :] = np.clip(road_region, 0, 255).astype(np.uint8)

    # Road perspective lane markings
    lane_color = (230, 230, 230)
    vp_x = random.randint(int(width * 0.4), int(width * 0.6))
    cv2.line(img, (vp_x, horizon_y), (random.randint(int(width * 0.05), int(width * 0.2)), height), (60, 160, 60), 12) # Verge
    cv2.line(img, (vp_x, horizon_y), (random.randint(int(width * 0.8), int(width * 0.95)), height), (60, 160, 60), 12)
    # Center dashed lane
    for y_step in range(horizon_y + 15, height, 40):
        t1 = (y_step - horizon_y) / (height - horizon_y)
        t2 = (y_step + 20 - horizon_y) / (height - horizon_y)
        x1 = int(vp_x + (width // 2 - vp_x) * t1)
        x2 = int(vp_x + (width // 2 - vp_x) * t2)
        cv2.line(img, (x1, y_step), (x2, y_step + 20), lane_color, max(2, int(6 * t1)))

    return Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))

def generate_sample(class_id, output_img_size=(640, 480)):
    """Composite sign icon onto background scene with 3D perspective, scale, lighting"""
    bg = generate_background_scene(output_img_size[0], output_img_size[1])
    W, H = output_img_size

    # Sign size (scale from distant 40px to close 160px)
    sign_size = random.randint(50, 150)
    sign_img = draw_sign_icon(class_id, size=sign_size)

    # Random slight rotation (-12 to +12 degrees)
    rot_angle = random.uniform(-12, 12)
    sign_img = sign_img.rotate(rot_angle, expand=True, resample=Image.BICUBIC)

    # Random placement on roadside (usually right or left side, above road)
    sw, sh = sign_img.size
    min_x = int(W * 0.55) if random.random() > 0.3 else int(W * 0.08)
    max_x = min(W - sw - 10, min_x + int(W * 0.35))
    if max_x <= min_x: max_x = min_x + 10

    min_y = int(H * 0.15)
    max_y = int(H * 0.65) - sh
    if max_y <= min_y: max_y = min_y + 10

    pos_x = random.randint(min_x, max_x)
    pos_y = random.randint(min_y, max_y)

    # Add mounting pole
    draw_bg = ImageDraw.Draw(bg)
    pole_x = pos_x + sw // 2
    pole_y1 = pos_y + sh // 2
    pole_y2 = pos_y + sh + random.randint(40, 120)
    draw_bg.line([(pole_x, pole_y1), (pole_x, pole_y2)], fill=(120, 120, 120), width=max(2, sw // 20))

    # Paste sign with transparency mask
    bg.paste(sign_img, (pos_x, pos_y), sign_img)

    # Calculate YOLO normalized bounding box: [cx, cy, w, h]
    # Crop to non-transparent bbox
    bbox = sign_img.getbbox()
    if bbox:
        bx1 = pos_x + bbox[0]
        by1 = pos_y + bbox[1]
        bx2 = pos_x + bbox[2]
        by2 = pos_y + bbox[3]
    else:
        bx1, by1, bx2, by2 = pos_x, pos_y, pos_x + sw, pos_y + sh

    cx = ((bx1 + bx2) / 2.0) / W
    cy = ((by1 + by2) / 2.0) / H
    bw = (bx2 - bx1) / float(W)
    bh = (by2 - by1) / float(H)

    # Photometric augmentation (lighting, contrast, blur)
    enh_bri = ImageEnhance.Brightness(bg)
    bg = enh_bri.enhance(random.uniform(0.7, 1.3))
    enh_con = ImageEnhance.Contrast(bg)
    bg = enh_con.enhance(random.uniform(0.8, 1.2))

    if random.random() < 0.25:
        bg = bg.filter(ImageFilter.GaussianBlur(radius=random.uniform(0.5, 1.5)))

    return bg, (class_id, cx, cy, bw, bh)

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Generate det10 synthetic road sign dataset")
    parser.add_argument("--output-dir", type=str, default="/home/rohith/dataset_det10")
    parser.add_argument("--train-samples", type=int, default=1500, help="Number of training samples")
    parser.add_argument("--val-samples", type=int, default=300, help="Number of validation samples")
    args = parser.parse_args()

    out_root = Path(args.output_dir)
    for split in ["train", "val"]:
        (out_root / "images" / split).mkdir(parents=True, exist_ok=True)
        (out_root / "labels" / split).mkdir(parents=True, exist_ok=True)

    print("=" * 70)
    print("🎨 GENERATING det10 DATASET (10 CLASSES)")
    print(f"Destination : {out_root.resolve()}")
    print(f"Train Count : {args.train_samples} images")
    print(f"Val Count   : {args.val_samples} images")
    print(f"Classes     : {CLASSES}")
    print("=" * 70)

    # Generate Train Split
    print("\n▶ Generating Training Set...")
    for i in range(args.train_samples):
        class_id = i % len(CLASSES)
        img, (cid, cx, cy, bw, bh) = generate_sample(class_id)
        img_filename = f"det10_train_{i:05d}.jpg"
        lbl_filename = f"det10_train_{i:05d}.txt"

        img.save(out_root / "images" / "train" / img_filename, quality=92)
        with open(out_root / "labels" / "train" / lbl_filename, "w") as f:
            f.write(f"{cid} {cx:.6f} {cy:.6f} {bw:.6f} {bh:.6f}\n")

        if (i + 1) % 250 == 0 or (i + 1) == args.train_samples:
            print(f"  [Train] Generated {i+1}/{args.train_samples} samples...")

    # Generate Validation Split
    print("\n▶ Generating Validation Set...")
    for i in range(args.val_samples):
        class_id = i % len(CLASSES)
        img, (cid, cx, cy, bw, bh) = generate_sample(class_id)
        img_filename = f"det10_val_{i:05d}.jpg"
        lbl_filename = f"det10_val_{i:05d}.txt"

        img.save(out_root / "images" / "val" / img_filename, quality=92)
        with open(out_root / "labels" / "val" / lbl_filename, "w") as f:
            f.write(f"{cid} {cx:.6f} {cy:.6f} {bw:.6f} {bh:.6f}\n")

        if (i + 1) % 100 == 0 or (i + 1) == args.val_samples:
            print(f"  [Val] Generated {i+1}/{args.val_samples} samples...")

    # Verify generated dataset
    n_train = len(list((out_root / "images" / "train").glob("*.jpg")))
    n_val = len(list((out_root / "images" / "val").glob("*.jpg")))
    print("\n" + "=" * 70)
    print(f"✅ Dataset Generation Complete!")
    print(f"   Train Images: {n_train}")
    print(f"   Val Images  : {n_val}")
    print(f"   Total Size  : ~{n_train + n_val} fully annotated bounding box frames")
    print("=" * 70)

if __name__ == "__main__":
    main()
