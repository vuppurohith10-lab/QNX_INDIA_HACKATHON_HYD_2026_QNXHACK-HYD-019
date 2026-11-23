# ============================================================================
# QNX GLOBAL E-HACK 2026 - SDV MAKEFILE
# Builds Autonomous Ground Vehicle for Raspberry Pi 4 (ARM64) & Host Simulation
# ============================================================================

QNX_ENV ?= /home/rohith/qnx800/qnxsdp-env.sh
BUILD_DIR = build

# QNX Cross-Compiler Tools
QCC_CC  = qcc -Vgcc_ntoaarch64le
QCC_CXX = qcc -Vgcc_ntoaarch64le_cxx

# Host Compiler Tools
HOST_CC  = gcc
HOST_CXX = g++

# Common Include and Compiler Flags
INCLUDES = -I. -Iinclude
CFLAGS   = -O2 -Wall -Wextra $(INCLUDES)
CXXFLAGS = -O2 -Wall -Wextra $(INCLUDES) -std=c++17

# QNX-Specific Link Flags
QNX_LDFLAGS = -lang-c++ -lm

# Host-Specific Link Flags
HOST_LDFLAGS = -lpthread -lm

SOURCES_C = sdv.c src/tof_sensor.c src/v800_motor.c
SOURCES_CPP = src/traffic_sign_ml.cpp

.PHONY: all pi4 host clean help

all: pi4 host

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Target: Raspberry Pi 4 (QNX Neutrino 8.0 aarch64)
pi4: $(BUILD_DIR)
	@echo "🔨 Cross-compiling for QNX 8.0 Raspberry Pi 4 (ARM64)..."
	@source $(QNX_ENV) 2>/dev/null || true; \
	$(QCC_CC) $(CFLAGS) -c src/tof_sensor.c -o $(BUILD_DIR)/tof_sensor_pi4.o && \
	$(QCC_CC) $(CFLAGS) -c src/v800_motor.c -o $(BUILD_DIR)/v800_motor_pi4.o && \
	$(QCC_CXX) $(CXXFLAGS) -c src/traffic_sign_ml.cpp -o $(BUILD_DIR)/traffic_sign_ml_pi4.o && \
	$(QCC_CC) $(CFLAGS) -c sdv.c -o $(BUILD_DIR)/sdv_pi4.o && \
	$(QCC_CXX) $(QNX_LDFLAGS) $(BUILD_DIR)/sdv_pi4.o $(BUILD_DIR)/tof_sensor_pi4.o $(BUILD_DIR)/v800_motor_pi4.o $(BUILD_DIR)/traffic_sign_ml_pi4.o -o $(BUILD_DIR)/sdv_pi4
	@echo "✅ Target binary ready: $(BUILD_DIR)/sdv_pi4"
	@file $(BUILD_DIR)/sdv_pi4 || true

# Target: Host Simulation (Linux x86_64)
host: $(BUILD_DIR)
	@echo "🔨 Compiling Host Simulator for Linux x86_64..."
	$(HOST_CC) $(CFLAGS) -c src/tof_sensor.c -o $(BUILD_DIR)/tof_sensor_host.o
	$(HOST_CC) $(CFLAGS) -c src/v800_motor.c -o $(BUILD_DIR)/v800_motor_host.o
	$(HOST_CXX) $(CXXFLAGS) -c src/traffic_sign_ml.cpp -o $(BUILD_DIR)/traffic_sign_ml_host.o
	$(HOST_CC) $(CFLAGS) -c sdv.c -o $(BUILD_DIR)/sdv_host.o
	$(HOST_CXX) $(BUILD_DIR)/sdv_host.o $(BUILD_DIR)/tof_sensor_host.o $(BUILD_DIR)/v800_motor_host.o $(BUILD_DIR)/traffic_sign_ml_host.o $(HOST_LDFLAGS) -o $(BUILD_DIR)/sdv_host
	@echo "✅ Host binary ready: $(BUILD_DIR)/sdv_host"

clean:
	rm -rf $(BUILD_DIR)
	@echo "🧹 Cleaned build directory."

help:
	@echo "Available build targets:"
	@echo "  make pi4    - Cross-compile for Raspberry Pi 4 running QNX 8.0 (ARM64)"
	@echo "  make host   - Compile simulation binary for local Linux host"
	@echo "  make all    - Build both Pi 4 and host binaries"
	@echo "  make clean  - Remove all compiled objects"
