#!/usr/bin/env python3
"""
============================================================================
HIGH-FPS ASYNCHRONOUS WEBCAM DETECTOR & ADAS ACTION SIMULATOR
QNX Global E-Hack 2026 - Ultra-Smooth 60 FPS Real-Time Vision Engine
============================================================================
Key Features:
 1. Threaded Asynchronous Camera Capture (eliminates USB hardware wait)
 2. Decoupled Asynchronous Inference Worker (smooth 60 FPS rendering)
 3. Dual-Backend: ONNX Runtime (High-Speed CPU) with PyTorch fallback
 4. Interactive Live Confidence Tuning (Press +/- to adjust sensitivity)
 5. Complete det10 10-Class Action Mapping with Live ADAS HUD
============================================================================
"""

import sys
import os
import time
import threading
import cv2
import numpy as np

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ONNX_PATH = os.path.join(BASE_DIR, "models", "best.onnx")
PT_PATH   = os.path.join(BASE_DIR, "models", "best.pt")

# Action mappings for det10
ACTIONS = {
    0: ("STOP SIGN", "EMERGENCY BRAKE (0% PWM)", (0, 0, 255)),
    1: ("NO ENTRY", "WRONG WAY ALERT (HALT)", (0, 0, 255)),
    2: ("SPEED LIMIT 30", "THROTTLE CAP (30% PWM)", (0, 255, 0)),
    3: ("SCHOOL AHEAD", "CAUTION ZONE (25% PWM)", (255, 255, 0)),
    4: ("PEDESTRIAN CROSSING", "YIELD CRAWL (20% PWM)", (255, 255, 0)),
    5: ("SLIPPERY ROAD", "TRACTION CONTROL (20% PWM)", (255, 200, 0)),
    6: ("RIGHT HAND CURVE", "DIFFERENTIAL STEER RIGHT", (255, 0, 255)),
    7: ("GIVE WAY", "INTERSECTION YIELD (15% PWM)", (0, 165, 255)),
    8: ("NO PARKING", "CRUISE MAINTAIN (50% PWM)", (0, 255, 0)),
    9: ("NARROW ROAD AHEAD", "LANE CENTER (25% PWM)", (255, 255, 0)),
}

CLASS_NAMES = [
    "stop", "no_entry", "speed_limit_30", "school_ahead",
    "pedestrian_crossing", "slippery_road", "right_hand_curve",
    "give_way", "no_parking", "narrow_road_ahead"
]

class ThreadedWebcam:
    """Non-blocking background video capture thread"""
    def __init__(self, src=0, width=640, height=480):
        self.cap = cv2.VideoCapture(src)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        self.cap.set(cv2.CAP_PROP_FPS, 60)
        self.cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        self.grabbed, self.frame = self.cap.read()
        self.running = True
        self.lock = threading.Lock()

        self.thread = threading.Thread(target=self.update, daemon=True)
        self.thread.start()

    def update(self):
        while self.running:
            grabbed, frame = self.cap.read()
            if grabbed:
                with self.lock:
                    self.grabbed = grabbed
                    self.frame = frame
            else:
                time.sleep(0.01)

    def read(self):
        with self.lock:
            return self.grabbed, (self.frame.copy() if self.frame is not None else None)

    def stop(self):
        self.running = False
        self.thread.join(timeout=1.0)
        self.cap.release()

class AsyncYOLOInference:
    """Asynchronous background inference worker"""
    def __init__(self, model_path, conf_thresh=0.25):
        self.model_path = model_path
        self.conf = conf_thresh
        self.running = True
        self.lock = threading.Lock()

        self.latest_frame = None
        self.latest_boxes = []
        self.model_fps = 0.0
        self.inference_latency_ms = 0.0

        # Try loading ONNX Runtime session first for high CPU throughput
        self.ort_session = None
        if os.path.exists(ONNX_PATH):
            try:
                import onnxruntime as ort
                opts = ort.SessionOptions()
                opts.intra_op_num_threads = 4
                opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
                self.ort_session = ort.InferenceSession(ONNX_PATH, opts, providers=['CPUExecutionProvider'])
                print(f"🚀 Loaded high-speed ONNX Runtime backend: {ONNX_PATH}")
            except Exception as e:
                print(f"ONNX load fallback ({e}), using PyTorch YOLO.")
                self.ort_session = None

        if self.ort_session is None:
            from ultralytics import YOLO
            self.model = YOLO(model_path)
            print(f"📦 Loaded PyTorch YOLO backend: {model_path}")

        self.thread = threading.Thread(target=self._worker_loop, daemon=True)
        self.thread.start()

    def submit_frame(self, frame):
        with self.lock:
            self.latest_frame = frame

    def get_results(self):
        with self.lock:
            return self.latest_boxes, self.model_fps, self.inference_latency_ms

    def _worker_loop(self):
        while self.running:
            frame_to_process = None
            with self.lock:
                if self.latest_frame is not None:
                    frame_to_process = self.latest_frame
                    self.latest_frame = None

            if frame_to_process is None:
                time.sleep(0.005)
                continue

            t0 = time.time()
            boxes_found = []

            # 1. High-Speed ONNX Runtime Execution
            if self.ort_session is not None:
                h_orig, w_orig = frame_to_process.shape[:2]
                blob = cv2.resize(frame_to_process, (320, 320))
                blob = cv2.cvtColor(blob, cv2.COLOR_BGR2RGB)
                blob = blob.astype(np.float32) / 255.0
                blob = np.transpose(blob, (2, 0, 1))
                blob = np.expand_dims(blob, axis=0)

                outputs = self.ort_session.run(None, {'images': blob})[0] # [1, 14, 2100]
                out = outputs[0] # [14, 2100]

                # Extract boxes and classes
                num_classes = 10
                cls_scores = out[4:4+num_classes, :] # [10, 2100]
                max_scores = np.max(cls_scores, axis=0)
                class_ids  = np.argmax(cls_scores, axis=0)

                mask = max_scores >= self.conf
                if np.any(mask):
                    valid_scores = max_scores[mask]
                    valid_classes = class_ids[mask]
                    valid_boxes   = out[0:4, mask] # [cx, cy, w, h] (normalized 0-320)

                    cx = valid_boxes[0] * (w_orig / 320.0)
                    cy = valid_boxes[1] * (h_orig / 320.0)
                    bw = valid_boxes[2] * (w_orig / 320.0)
                    bh = valid_boxes[3] * (h_orig / 320.0)

                    x1 = cx - bw/2.0
                    y1 = cy - bh/2.0

                    cv_boxes = [[int(x1[k]), int(y1[k]), int(bw[k]), int(bh[k])] for k in range(len(valid_scores))]
                    indices = cv2.dnn.NMSBoxes(cv_boxes, valid_scores.tolist(), float(self.conf), 0.45)

                    if len(indices) > 0:
                        for idx in indices.flatten():
                            cid = int(valid_classes[idx])
                            score = float(valid_scores[idx])
                            bx, by, bw_b, bh_b = cv_boxes[idx]
                            boxes_found.append((cid, score, (bx, by, bx + bw_b, by + bh_b)))

            # 2. PyTorch Fallback
            else:
                res = self.model.predict(frame_to_process, imgsz=320, conf=self.conf, device="cpu", verbose=False)[0]
                for b in res.boxes:
                    cid = int(b.cls[0])
                    conf = float(b.conf[0])
                    x1, y1, x2, y2 = map(int, b.xyxy[0])
                    boxes_found.append((cid, conf, (x1, y1, x2, y2)))

            dt = time.time() - t0
            with self.lock:
                self.latest_boxes = boxes_found
                self.inference_latency_ms = dt * 1000.0
                self.model_fps = 1.0 / (dt + 1e-6)

    def stop(self):
        self.running = False
        self.thread.join(timeout=1.0)

def main():
    camera_id = int(sys.argv[1]) if len(sys.argv) > 1 else 0

    print("=" * 70)
    print("🚗 QNX SDV ULTRA-SMOOTH LIVE WEBCAM DETECTOR (60 FPS HUD)")
    print(f"Opening Camera Device: {camera_id}")
    print("Keyboard Controls:")
    print("  [+] Increase Confidence Threshold")
    print("  [-] Decrease Confidence Threshold (Detect Fainter/Angled Signs)")
    print("  [s] Save Detection Snapshot")
    print("  [q] Quit")
    print("=" * 70)

    # Initialize Threaded Camera
    cam = ThreadedWebcam(src=camera_id, width=640, height=480)
    time.sleep(0.5)

    grabbed, test_frame = cam.read()
    if not grabbed or test_frame is None:
        print(f"❌ Failed to connect to camera {camera_id}.")
        cam.stop()
        return

    # Initialize Asynchronous Detector
    detector = AsyncYOLOInference(PT_PATH, conf_thresh=0.25)

    display_prev_time = time.time()
    display_fps = 60.0

    while True:
        grabbed, frame = cam.read()
        if not grabbed or frame is None:
            time.sleep(0.005)
            continue

        # Submit latest frame to inference worker
        detector.submit_frame(frame)

        # Retrieve latest detected boxes
        boxes, model_fps, latency_ms = detector.get_results()

        # Compute display FPS
        now = time.time()
        display_fps = 0.9 * display_fps + 0.1 * (1.0 / (now - display_prev_time + 1e-6))
        display_prev_time = now

        current_action = "PATH CLEAR - NORMAL CRUISE (50% PWM)"
        action_col = (0, 255, 0)

        # Draw detected bounding boxes
        for cid, conf, (x1, y1, x2, y2) in boxes:
            sign_name, act_str, col = ACTIONS.get(cid, (CLASS_NAMES[cid], "CUSTOM", (255, 255, 255)))
            current_action = act_str
            action_col = col

            # Draw sleek HUD bounding box
            cv2.rectangle(frame, (x1, y1), (x2, y2), col, 2)
            cv2.rectangle(frame, (x1, max(0, y1 - 24)), (x1 + 180, y1), col, -1)
            cv2.putText(frame, f"{sign_name} {conf:.1%}", (x1 + 4, max(16, y1 - 6)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.50, (255, 255, 255), 2)

        # Top HUD Status Bar
        cv2.rectangle(frame, (0, 0), (frame.shape[1], 44), (18, 18, 18), -1)
        cv2.putText(frame, f"QNX SDV | DISP: {display_fps:.0f} FPS | MODEL: {model_fps:.0f} FPS ({latency_ms:.1f}ms) | CONF: {detector.conf:.2f}",
                    (8, 18), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (220, 220, 220), 1)
        cv2.putText(frame, f"ACTION: {current_action}",
                    (8, 36), cv2.FONT_HERSHEY_SIMPLEX, 0.50, action_col, 2)

        cv2.imshow("QNX SDV - Ultra-Smooth Traffic Sign Recognition", frame)

        key = cv2.waitKey(1) & 0xFF
        if key == ord('q') or key == 27:
            break
        elif key == ord('+') or key == ord('='):
            detector.conf = min(0.90, detector.conf + 0.05)
            print(f"[HUD] Increased confidence threshold to: {detector.conf:.2f}")
        elif key == ord('-') or key == ord('_'):
            detector.conf = max(0.10, detector.conf - 0.05)
            print(f"[HUD] Decreased confidence threshold to: {detector.conf:.2f}")
        elif key == ord('s'):
            cv2.imwrite("detection_snapshot.jpg", frame)
            print("📸 Saved snapshot to detection_snapshot.jpg")

    detector.stop()
    cam.stop()
    cv2.destroyAllWindows()
    print("System shutdown cleanly.")

if __name__ == "__main__":
    main()
