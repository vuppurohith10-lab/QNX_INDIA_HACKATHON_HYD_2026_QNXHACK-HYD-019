/**
 * @file traffic_sign_ml.h
 * @brief C-compatible interface for Edge ML Traffic Sign Recognition and USB Webcam
 *
 * Runs on QNX Neutrino RTOS (Raspberry Pi 4 / ARM64 / x86_64)
 */

#ifndef TRAFFIC_SIGN_ML_H
#define TRAFFIC_SIGN_ML_H

#include "sdv_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *model_path;     /* "models/best.onnx" or "models/best.tflite" */
    const char *labels_path;    /* "models/labels.txt" */
    int camera_device_id;       /* 0 for /dev/video0 */
    float conf_threshold;       /* 0.45f */
    float nms_threshold;        /* 0.45f */
    int input_width;            /* 320 */
    int input_height;           /* 320 */
    int required_frames;        /* 2 frames for confirmation */
} traffic_sign_ml_config_t;

/**
 * @brief Initialize USB webcam and Edge ML inference model
 * @param config Pointer to configuration struct
 * @return 0 on success, negative error code on failure
 */
int traffic_sign_ml_init(const traffic_sign_ml_config_t *config);

/**
 * @brief Process single video frame from USB webcam, run ML inference, update context
 * @param ctx Pointer to shared vehicle state context
 * @return Detected sign ID (0-9) or SIGN_NONE (-1)
 */
int traffic_sign_ml_step(sdv_vehicle_context_t *ctx);

/**
 * @brief Get human-readable label for a class ID
 */
const char* traffic_sign_get_name(int class_id);

/**
 * @brief Release camera and ML model resources
 */
void traffic_sign_ml_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* TRAFFIC_SIGN_ML_H */
