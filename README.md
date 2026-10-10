# QNX_INDIA_HACKATHON_HYD_2026_AIRAVATA-19
SDV BASEd MILITARY VECHICLE USING QNX
# SDV Based Military Vehicle using QNX

## 🛡️ QNX-Based Real-Time Military Ground Vehicle using SDV & V2V Communication

## Introduction

This project focuses on building a real-time military ground vehicle using a Software Defined Vehicle (SDV) approach powered by QNX RTOS. It uses a Raspberry Pi 4 along with sensors and communication modules to simulate how a vehicle can sense its surroundings, make quick decisions, and share its status with other vehicles. The goal is to improve mission safety and reduce risk to personnel through intelligent and real-time control.

## Problem Statement

Current military ground vehicles depend heavily on human reaction, which leads to delays, collisions, and risk to personnel in hazardous or low-visibility terrain. There is a need for a system that can respond quickly, share information, and operate more safely.

## Proposed Solution

This system:

* Monitors surroundings using an ultrasonic sensor
* Processes data in real time using QNX RTOS
* Communicates with other vehicles using V2V
* Controls vehicle movement based on obstacle distance

## Key Features

* Real-time monitoring with fast response
* Deterministic performance using QNX RTOS
* State-based decision model (Stop / Slow / Normal)
* V2V communication using Bluetooth
* LED and laser status indicators

## 🧠 System Architecture

The system is organized into layers for better clarity and control:

Sensing → Processing → Decision → Communication → Actuation

## 🔄 Methodology

1. Sensor measures distance
2. Data is processed in QNX
3. Vehicle state is determined
4. Decision is made
5. Information is shared via Bluetooth
6. Vehicle movement is adjusted

## 🧰 Hardware Components

* Raspberry Pi 4 (QNX RTOS)
* ESP32 module
* Ultrasonic sensor
* HC-05 Bluetooth module
* L298N motor driver
* DC motors
* LEDs and laser indicator
* TFT display

## 💻 Software Components

* QNX RTOS
* C programming
* Multi-threading (`pthread`)
* UART communication

## 📊 Results

* Smooth operation in clear conditions
* Gradual speed control near obstacles
* Immediate stop when an obstacle is too close
* Continuous status sharing with other vehicles

## 🚀 Applications

* Military convoy movement
* Reconnaissance vehicles
* Unmanned logistics and supply transport
* Collision avoidance

## 🔮 Future Scope

* GPS and camera integration
* AI-based obstacle detection
* V2X communication
* Real-world deployment

## 👨‍💻 Contributors

* V.Rohith
* K.Tarun
* V.Pradeep Kumar
 
