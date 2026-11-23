/**
 * @file tof_sensor.h
 * @brief High-Precision Time-of-Flight (ToF) Sensor Driver for QNX Neutrino RTOS
 *
 * Supports:
 * - VL53L0X / VL53L1X I2C Time-of-Flight Sensor (default address 0x29)
 * - TFmini / TF-Luna High-Speed Serial LiDAR ToF (UART 115200 baud)
 * - Deterministic non-blocking millimeter ranging with Kalman/EMA noise filtering
 */

#ifndef TOF_SENSOR_H
#define TOF_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TOF_MODE_AUTO = 0,         /* Try I2C VL53, fall back to UART or simulation */
    TOF_MODE_I2C_VL53L0X,      /* Direct I2C VL53L0X/VL53L1X via /dev/i2c1 */
    TOF_MODE_UART_TFLUNA,      /* Serial UART ToF via /dev/ser2 */
    TOF_MODE_SIMULATION        /* Software test simulator for CI/CD / bench validation */
} tof_sensor_mode_t;

typedef struct {
    tof_sensor_mode_t mode;
    const char *device_path;   /* e.g., "/dev/i2c1" or "/dev/ser2" */
    uint8_t i2c_address;       /* 0x29 */
    int baud_rate;             /* 115200 for UART */
    float filter_alpha;        /* 0.6 = responsive, 0.2 = heavy smoothing */
    float min_valid_cm;        /* 2.0 cm */
    float max_valid_cm;        /* 400.0 cm (4 meters) */
} tof_config_t;

typedef struct {
    tof_config_t config;
    int fd;
    bool is_initialized;
    float last_distance_cm;
    float raw_distance_cm;
    uint32_t sample_count;
    uint32_t error_count;
} tof_sensor_handle_t;

/**
 * @brief Initialize the Time-of-Flight sensor hardware on QNX
 * @param handle Pointer to driver handle
 * @param config Configuration parameters
 * @return 0 on success, negative errno on failure
 */
int tof_sensor_init(tof_sensor_handle_t *handle, const tof_config_t *config);

/**
 * @brief Perform a non-blocking measurement cycle
 * @param handle Pointer to driver handle
 * @param distance_out_cm Output variable for filtered distance in centimeters
 * @return 0 on success, negative value on read error / timeout
 */
int tof_sensor_read_distance(tof_sensor_handle_t *handle, float *distance_out_cm);

/**
 * @brief Close sensor file descriptor and release resources
 */
void tof_sensor_close(tof_sensor_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* TOF_SENSOR_H */
