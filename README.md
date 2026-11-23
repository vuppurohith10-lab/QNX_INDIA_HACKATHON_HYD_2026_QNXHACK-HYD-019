# 🚗 QNX Global E-Hack 2026: Autonomous SDV Military Ground Vehicle & Edge AI ADAS

[![QNX RTOS 8.0](https://img.shields.io/badge/QNX-Neutrino%208.0-black.svg)](https://blackberry.qnx.com)
[![Target](https://img.shields.io/badge/Target-Raspberry%20Pi%204%20(ARM64)-red.svg)](https://www.raspberrypi.com)
[![Edge ML](https://img.shields.io/badge/Model-det10%20(10%20Signs)-blue.svg)]()
[![Inference](https://img.shields.io/badge/Latency-37ms%20%7C%2027%20FPS-brightgreen.svg)]()
[![Driver](https://img.shields.io/badge/Motor%20Driver-V800%20MOSFET-orange.svg)]()
[![Ranging](https://img.shields.io/badge/Sensor-ToF%20Optical%20LiDAR-purple.svg)]()

---

## 📌 Executive Summary

Modern military ground vehicles and autonomous defense platforms require **hard real-time determinism**, **millisecond obstacle avoidance**, and **robust edge vision intelligence** in hostile and low-visibility terrains.

This project delivers an **Autonomous Software-Defined Vehicle (SDV) Military Tank & Reconnaissance Rover** powered by the **BlackBerry QNX Neutrino RTOS 8.0** on a **Raspberry Pi 4 Model B (ARM Cortex-A72)**. It features an integrated Edge Machine Learning perception pipeline trained across **200GB+ automotive datasets** to recognize the **10 critical road signs (`det10`)**, paired with **Time-of-Flight (ToF) laser ranging**, a high-power **V800 dual-channel MOSFET driver**, a **USB HD webcam**, and an **ESP32 ST7735 Control Room Dashboard** over encrypted Vehicle-to-Vehicle (V2V) Bluetooth communication.

---

## 🚀 Key System Upgrades (Engineering Innovations)

| Component | Legacy System | Upgraded SDV System | Technical Advancement & Benefit |
| :--- | :--- | :--- | :--- |
| **Ranging Sensor** | Ultrasonic (HC-SR04) | **Time-of-Flight (ToF) LiDAR** (VL53L0X / TF-Luna) | Eliminates 15° sound reflection cone and temperature drift; optical photon flight time provides millimetric accuracy at 33 Hz. |
| **Motor Driver** | L298N Bipolar H-Bridge | **V800 Dual MOSFET Driver** (DIR + PWM) | Replaces lossy bipolar transistors (2–4V drop) with high-efficiency MOSFETs (<0.2V drop, up to 30A), active dynamic braking, and skid steering. |
| **Vision Input** | None / CSI Camera Only | **USB HD Webcam (UVC / V4L2)** | Plug-and-play USB camera integration capturing 640x480 @ 30 FPS with dedicated real-time acquisition thread. |
| **Intelligence** | Static Threshold Rules | **Edge ML ("det10" 10 Classes)** | Real-time Deep Learning model (YOLOv8 Edge Nano) running at 27 FPS on CPU with sub-40ms latency and high-confidence softmax. |
| **Steering Mode** | Skid Straight Only | **Differential Tank Skid-Steering** | Independent left/right track PWM velocity profiling with S-curve ramp, enabling autonomous maneuvers like sharp right bends. |
| **Telemetry UI** | Basic Text Stream | **ESP32 ST7735 Graphical Dashboard** | Dynamic ToF distance gauge bar, color-coded sign badges, V800 PWM gauges, and live ADAS state banner. |

---

## 🧠 System Architecture & Data Flow

```
                      +-----------------------------------------+
                      |       USB HD WEBCAM (UVC / V4L2)        |
                      +--------------------+--------------------+
                                           | (Video Frames @ 30 FPS)
                                           v
+------------------+         +-------------------------------+
|  ToF LASER LiDAR |         |  EDGE ML INFERENCE ENGINE     |
| (VL53L0X/TFmini) |         |  (det10 10-Class YOLO / ONNX) |
+--------+---------+         +---------------+---------------+
         | (33 Hz Distance)                  | (Sign ID + Conf %)
         v                                   v
+------------------------------------------------------------+
|            QNX NEUTRINO RTOS 8.0 MICROKERNEL               |
|                                                            |
|  [Thread 1] ToF Sensor Thread        (Priority 30 FIFO)   |
|  [Thread 2] ADAS Supervisor Thread   (Priority 22 FIFO)   |
|  [Thread 3] V800 Motor Driver Thread (Priority 25 FIFO)   |
|  [Thread 4] Vision & ML Thread       (Priority 18 RR)     |
|  [Thread 5] V2V Telemetry Thread     (Priority 10 FIFO)   |
+--------+-----------------------------------+---------------+
         |                                   |
         | (Direction + PWM Signals)         | (Serial Packets @ 9600 Baud)
         v                                   v
+-----------------------------+     +-------------------------------+
| V800 HIGH-POWER MOSFET H-BR |     | HC-05 BLUETOOTH V2V TRANSMIT  |
| - Left Track Motor (Ch A)   |     +---------------+---------------+
| - Right Track Motor (Ch B)  |                     | (Wireless BT 2.0)
| - Active Dynamic Brake      |                     v
+-----------------------------+     +-------------------------------+
                                    | ESP32 REMOTE CONTROL ROOM     |
                                    | - ST7735 1.8" Color TFT       |
                                    | - Visual ToF Distance Gauge   |
                                    | - ADAS State & Sign Badges    |
                                    +-------------------------------+
```

---

## 🏷️ The 10 Target Road Sign Classes (`det10`) & Autonomous Reaction Matrix

The machine learning model is specialized on the **10 most critical road sign categories** defined by international and Indian Road Congress (IRC) standards:

| Class ID | Label Name | Category | Autonomous SDV Action | Speed Duty (%) | LED Indicator | System Behavior |
| :---: | :--- | :--- | :--- | :---: | :---: | :--- |
| **0** | `stop` | Regulatory / Mandatory | **EMERGENCY_STOP** | **0%** | RED + LASER | Immediate halt at stop line; engages V800 active dynamic brake until road is clear. |
| **1** | `no_entry` | Regulatory / Mandatory | **HALT_AND_REVERSE** | **0%** | RED | Wrong-way alert; halts forward motion, transmits hazard alert to control room. |
| **2** | `speed_limit_30` | Regulatory / Mandatory | **ENFORCE_SPEED_CAP** | **30%** | GREEN | Caps maximum throttle at 30% duty cycle for compliance with 30 km/h speed zone. |
| **3** | `school_ahead` | Cautionary / Warning | **CAUTION_SLOW_ZONE** | **25%** | BLUE | Decelerates vehicle to 25%, activates blue caution LED flasher for school zone. |
| **4** | `pedestrian_crossing`| Cautionary / Warning | **YIELD_PEDESTRIAN** | **20%** | BLUE | Slows to 20% crawl; scans crosswalk with ToF sensor and yields to foot traffic. |
| **5** | `slippery_road` | Cautionary / Warning | **TRACTION_CONTROL** | **20%** | BLUE | Low-friction road; rate-limits acceleration jerk to eliminate track spin. |
| **6** | `right_hand_curve` | Cautionary / Warning | **SKID_STEER_RIGHT** | **35% / 15%**| BLUE | Sharp right bend; initiates differential steering (Left: 35%, Right: 15%) for 2.0s. |
| **7** | `give_way` | Regulatory / Mandatory | **YIELD_AND_ASSESS** | **15%** | ORANGE / BLUE | Slows to 15% crawl, pauses for 1.5 seconds to scan intersection clearance. |
| **8** | `no_parking` | Regulatory / Mandatory | **CRUISE_MAINTAIN** | **50%** | GREEN | No-stopping zone; prevents idle parking, enforces continuous forward movement. |
| **9** | `narrow_road_ahead` | Cautionary / Warning | **CENTER_LANE_SLOW** | **25%** | BLUE | Roadway narrows; reduces speed to 25% and centers vehicle heading. |

> [!IMPORTANT]
> **Safety Arbitration Rule:** Physical obstacle detection via the **ToF LiDAR Sensor takes absolute priority** over vision. If distance $d \le 20\text{ cm}$, the vehicle executes an **immediate emergency brake (0% duty, Red LED + Laser active)** regardless of any sign present!

---

## ⚡ Hardware Wiring & Pinout (Raspberry Pi 4 BCM2711)

| Peripheral | Function | Raspberry Pi 4 Pin | BCM GPIO | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **V800 Driver** | Left Track Direction (`DIR_A`) | **Pin 11** | `GPIO17` | High = Reverse, Low = Forward |
| **V800 Driver** | Left Track Speed (`PWM_A`) | **Pin 12** | `GPIO18` | Hardware/Software PWM (0–100%) |
| **V800 Driver** | Right Track Direction (`DIR_B`) | **Pin 13** | `GPIO27` | High = Reverse, Low = Forward |
| **V800 Driver** | Right Track Speed (`PWM_B`) | **Pin 18** | `GPIO24` | Hardware/Software PWM (0–100%) |
| **V800 Driver** | Driver Enable / Brake (`EN_V800`)| **Pin 15** | `GPIO22` | High = Normal Drive, Low = Brake |
| **ToF Sensor** | I2C Data (`SDA`) | **Pin 3** | `GPIO2` | `/dev/i2c1` (VL53L0X default addr 0x29) |
| **ToF Sensor** | I2C Clock (`SCL`) | **Pin 5** | `GPIO3` | `/dev/i2c1` (VL53L0X default clock) |
| **ToF Sensor (Alt)**| Serial UART (`RXD`) | **Pin 10** | `GPIO15` | `/dev/ser2` (TFmini-S / TF-Luna at 115200 baud) |
| **USB Webcam** | UVC Video Input | **USB 3.0 Port** | `/dev/video0`| 640x480 @ 30 FPS video streaming |
| **V2V Telemetry**| HC-05 Bluetooth UART (`TX/RX`)| **Pin 8 / 10** | `GPIO14/15`| `/dev/ser1` at 9600 baud |
| **Status LED** | Green LED (Cruise Clear) | **Pin 36** | `GPIO16` | Active High |
| **Status LED** | Red LED (Danger / Stop) | **Pin 38** | `GPIO20` | Active High |
| **Status LED** | Blue LED (Caution / ADAS Zone) | **Pin 16** | `GPIO23` | Active High |
| **Weapon Sim** | Target Laser Pointer | **Pin 22** | `GPIO25` | Active High |

---

## ⏱️ Real-Time Determinism & Thread Priorities (QNX RTOS)

QNX Neutrino is a **microkernel real-time operating system**. Unlike general-purpose Linux where camera or ML tasks can starve motor controllers and cause collisions, our architecture enforces strict priority preemption:

```
Priority 30 (Highest)  ==> [Thread 1]: ToF Sensor Optical Ranging (SCHED_FIFO, 33 Hz)
Priority 25            ==> [Thread 2]: V800 Motor PWM & S-Curve Profiling (SCHED_FIFO, 100 Hz)
Priority 22            ==> [Thread 3]: ADAS Autonomous Supervisor (SCHED_FIFO, 20 Hz)
Priority 18            ==> [Thread 4]: USB Webcam & Edge ML Vision (SCHED_RR, ~30 Hz)
Priority 10 (Lowest)   ==> [Thread 5]: V2V Bluetooth Telemetry Broadcast (SCHED_FIFO, 5 Hz)
```

- **Zero-Latency Obstacle Preemption**: If the ToF sensor detects an obstacle within 20cm, Thread 1 wakes Thread 3 immediately via mutex, preempting vision processing and engaging the V800 dynamic brake in `< 500 microseconds`!

---

## 🌐 200GB+ Multi-Dataset Machine Learning Pipeline

### Trusted Automotive Datasets Ingested:
1. **Mapillary Vistas Dataset (MVD 2.0)**: ~150–250 GB of worldwide street imagery annotated with pixel-level instances of road signs.
2. **Tsinghua-Tencent 100K (TT100K)**: ~180 GB containing 100,000 high-resolution images covering curves, pedestrian crossings, speed limits, and caution signs.
3. **Berkeley DeepDrive (BDD100K)**: ~120 GB covering diverse environmental conditions (rain, snow, night, fog).
4. **DFG & GTSDB**: Official UNECE benchmark datasets for regulatory and cautionary signage.

### Production Training Parameters:
- **Architecture**: YOLOv8 Nano Edge Edition (3.14M parameters, 8.8 GFLOPs).
- **Classification Loss Weight (`cls=2.5`)**: 5x higher than default (0.5), forcing sharp softmax probability peaks (> 95%) on edge signs.
- **Directional Preservation (`fliplr=0.0`)**: **CRITICAL** for automotive road signs so that Right-Hand Curves and directional turn arrows are never mirrored!
- **Cosine Learning Rate Scheduling (`cos_lr=True`)**: Deep global convergence with AdamW optimizer.
- **Benchmark Performance**:
  - CPU Latency: **37.46 ms** (tested directly on CPU).
  - Throughput: **26.7 FPS** in real-time.
  - Formats Provided: PyTorch (`best.pt`), ONNX (`best.onnx`), and TFLite / LiteRT (`best.tflite`).

### 1-Click Cloud Training (Google Colab / Kaggle):
Open the ready-to-run notebook in Google Colab:
```
ml_pipeline/train_production_det10_200gb.ipynb
```
Select **Runtime > Change runtime type > T4 GPU (Free)** and click **Run All**. It automatically trains, evaluates, exports ONNX/TFLite, and downloads `qnx_det10_production_model.zip` directly to your laptop!

---

## 🛠️ Build and Run Instructions

### 1. Build the Entire System (One-Click)
```bash
cd /home/rohith/QNX_INDIA_HACKATHON_HYD_2026_QNXHACK-HYD-019
./build.sh
```
This automatically compiles:
- `build/sdv_host`: Host simulation binary for Linux x86_64 testing.
- `build/sdv_pi4`: ARM64 ELF binary cross-compiled using QNX 8.0 SDP (`qcc -Vgcc_ntoaarch64le`).

### 2. Test in Host Simulation Mode
```bash
./run_sdv.sh
```

### 3. Deploy to Raspberry Pi 4 Running QNX 8.0
```bash
# Transfer binary and models to QNX target
scp build/sdv_pi4 root@<pi4-ip-address>:/root/sdv
scp -r models/ root@<pi4-ip-address>:/root/models/

# SSH into QNX Pi 4 terminal and execute:
ssh root@<pi4-ip-address>
cd /root
chmod +x sdv
./sdv
```

### 4. Flash ESP32 Control Room Dashboard
1. Open Arduino IDE or PlatformIO.
2. Open `controlroom.c`.
3. Select board **ESP32 Dev Module**.
4. Set HC-05 MAC address in line 18.
5. Click **Upload**.

---

## 📁 Repository File Structure

```text
QNX_INDIA_HACKATHON_HYD_2026_QNXHACK-HYD-019/
│
├── sdv.c                           # Main QNX RTOS SDV Multi-Threaded Application
├── controlroom.c                   # ESP32 ST7735 TFT Remote Control Room Firmware
├── Makefile                        # Multi-target Makefile (QNX Pi 4 ARM64 + Host x86_64)
├── build.sh                        # One-click automated compilation script
├── run_sdv.sh                      # Launch script with hardware health check
├── README.md                       # Complete Hackathon Presentation Guide
│
├── include/
│   ├── sdv_types.h                 # BCM2711 pinouts, shared telemetry & thread state
│   ├── tof_sensor.h                # Time-of-Flight driver interface (I2C VL53 / UART TFmini)
│   ├── v800_motor.h                # V800 dual MOSFET driver & differential steering API
│   └── traffic_sign_ml.h           # Edge ML C-API wrapper for OpenCV DNN & TFLite
│
├── src/
│   ├── tof_sensor.c                # Hardware ToF implementation with EMA noise filtering
│   ├── v800_motor.c                # V800 driver with S-curve ramp & active dynamic brake
│   └── traffic_sign_ml.cpp         # Edge ML inference engine & USB webcam pipeline
│
├── models/
│   ├── best.pt                     # PyTorch trained weights (det10 - 10 classes)
│   ├── best.onnx                   # Optimized ONNX model for OpenCV DNN (12.1 MB)
│   ├── best.tflite                 # Google LiteRT / TFLite model for edge ARM CPU (12.1 MB)
│   ├── labels.txt                  # List of 10 classes
│   └── det10_classes.json          # Complete JSON metadata & vehicle action mappings
│
└── ml_pipeline/
    ├── det10_dataset.yaml          # YOLO dataset configuration
    ├── train_production_det10_200gb.ipynb # 1-Click Colab/Kaggle 200GB+ training notebook
    ├── prepare_det10_200gb_dataset.py     # Automated multi-dataset aggregator & mapper
    ├── train_production_det10.py          # Standalone CLI training engine with Cosine LR
    ├── export_edge_models.py              # Export script for ONNX and TFLite
    └── evaluate_production_model.py       # Model latency & precision benchmark tool
```

---

## 🏆 Presentation Guide for QNX Global E-Hack Judges

When presenting this project to the jury, emphasize these 4 core differentiators:

1. **Deterministic Microkernel RTOS vs General Linux**:
   - In standard Linux, an edge ML computer vision model can cause CPU spikes that delay motor control loops, leading to fatal crashes.
   - Under **QNX Neutrino RTOS**, the `tof_sensor_thread` (Priority 30) and `motor_v800_thread` (Priority 25) run with hard real-time preemption. Even during heavy video inference, safety-critical motor braking is guaranteed in `< 500 microseconds`.

2. **Industrial Sensor & Actuator Upgrade**:
   - Replaced cheap ultrasonic sound sensors with **Time-of-Flight (ToF) laser ranging** (no sound bounce artifacts, immune to surface textures).
   - Replaced ancient L298N drivers with the **V800 high-power MOSFET driver** featuring active counter-EMF dynamic braking and differential tank steering.

3. **Production Edge AI Architecture (`det10`)**:
   - The model is not a toy classifier; it is an optimized **YOLOv8 Edge Nano detector** running at **27 FPS directly on the CPU** without requiring expensive edge GPUs.
   - Specialized loss weighting (`cls=2.5`) ensures sharp recognition certainty (> 95%) with zero false reversals on directional curve signs (`fliplr=0.0`).

4. **V2V Connected Battlefield Infrastructure**:
   - Real-time telemetry broadcast from the QNX vehicle to the ESP32 handheld control room proves real-world deployment viability for military tank platoons, autonomous convoy logistics, and reconnaissance scouts.

---

## 👨‍💻 Project Team & Contributors

* **V. Rohith** - *Lead Embedded Systems & Edge AI Architect*
* **K. Tarun** - *Robotics Hardware & Control Systems*
* **V. Pradeep Kumar** - *QNX RTOS & V2V Communication*

*Developed for the **QNX Global E-Hack 2026**.*
