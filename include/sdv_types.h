/**
 * @file sdv_types.h
 * @brief Common types, hardware pinouts, and system configuration for QNX SDV
 *
 * QNX Global E-Hack 2026 - Autonomous SDV Military Tank & Ground Vehicle
 * target: Raspberry Pi 4 Model B (Broadcom BCM2711, ARM Cortex-A72, QNX 8.0)
 */

#ifndef SDV_TYPES_H
#define SDV_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * BCM2711 (Raspberry Pi 4) Direct Register & Pin Definitions
 * ========================================================================= */
#define BCM2711_PERI_BASE   0xFE000000
#define GPIO_BASE           0xFE200000
#define BLOCK_SIZE          4096

/* GPIO Register Offsets */
#define GPFSEL0 (0x00/4)
#define GPFSEL1 (0x04/4)
#define GPFSEL2 (0x08/4)
#define GPSET0  (0x1C/4)
#define GPCLR0  (0x28/4)
#define GPLEV0  (0x34/4)

/* =========================================================================
 * ACTUATOR: V800 High-Power Dual-Channel MOSFET Driver Pinout
 * Replaces obsolete L298N with high-efficiency PWM+DIR H-Bridge
 * ========================================================================= */
/* Left Track Motor Channel (Motor A) */
#define V800_DIR_LEFT    (1 << 17)  /* GPIO17 (Physical Pin 11) -> Direction */
#define V800_PWM_LEFT    (1 << 18)  /* GPIO18 (Physical Pin 12) -> Speed PWM */

/* Right Track Motor Channel (Motor B) */
#define V800_DIR_RIGHT   (1 << 27)  /* GPIO27 (Physical Pin 13) -> Direction */
#define V800_PWM_RIGHT   (1 << 24)  /* GPIO24 (Physical Pin 18) -> Speed PWM */

/* V800 Driver Enable / Fault / Dynamic Brake */
#define V800_ENABLE_PIN  (1 << 22)  /* GPIO22 (Physical Pin 15) -> Enable/Active Low Brake */

/* Backward compatibility aliases */
#define MOTOR_DIR_L      V800_DIR_LEFT
#define MOTOR_PWM_L      V800_PWM_LEFT
#define MOTOR_DIR_R      V800_DIR_RIGHT
#define MOTOR_PWM_R      V800_PWM_RIGHT

/* =========================================================================
 * SENSOR: Time-of-Flight (ToF) Sensor Interfaces
 * Replaces ultrasonic pulse measurement with millimetric photon flight time
 * ========================================================================= */
/* Option 1: I2C VL53L0X / VL53L1X on QNX I2C Bus 1 */
#define TOF_I2C_DEVICE       "/dev/i2c1"
#define TOF_VL53_I2C_ADDR    0x29        /* 7-bit standard I2C address */
#define TOF_XSHUT_PIN        (1 << 4)    /* GPIO4 (Physical Pin 7) -> Shutdown/Reset */
#define TOF_GPIO_INT_PIN     (1 << 5)    /* GPIO5 (Physical Pin 29) -> Interrupt / Data Ready */

/* Option 2: Serial UART ToF (TFmini-S / TF-Luna 100Hz LiDAR) */
#define TOF_SERIAL_PORT      "/dev/ser2" /* Secondary UART /dev/ser2 or /dev/ttyUSB0 */
#define TOF_SERIAL_BAUD      115200

/* =========================================================================
 * VISUAL INDICATORS & WEAPON SIMULATOR
 * ========================================================================= */
#define GREEN_LED   (1 << 16)  /* GPIO16 (Physical Pin 36) -> Normal Cruising / Path Clear */
#define RED_LED     (1 << 20)  /* GPIO20 (Physical Pin 38) -> Emergency Stop / Danger */
#define BLUE_LED    (1 << 23)  /* GPIO23 (Physical Pin 16) -> Caution / School / Crossing */
#define LASER_PIN   (1 << 25)  /* GPIO25 (Physical Pin 22) -> Target Acquisition Laser */

/* =========================================================================
 * V2V TELEMETRY & CONTROL ROOM COMMUNICATION
 * ========================================================================= */
#define TELEMETRY_PORT       "/dev/ser1" /* HC-05 Bluetooth UART on Pi 4 */
#define TELEMETRY_BAUD       9600

/* =========================================================================
 * DISTANCE THRESHOLDS & SPEED PROFILES (Centimeters & % Duty Cycle)
 * ========================================================================= */
#define DIST_EMERGENCY_STOP_CM  20.0f  /* <= 20cm: Full immediate halt, Red + Laser */
#define DIST_CAUTION_SLOW_CM    40.0f  /* 20cm < d <= 40cm: Decelerate, Blue LED */
#define DIST_CLEAR_CRUISE_CM    40.0f  /* > 40cm: Normal autonomous cruise */

#define DUTY_STOP                0     /* 0% throttle */
#define DUTY_CREEP              15     /* 15% crawl (yield / give way) */
#define DUTY_SLOW               25     /* 25% caution speed (school, crossing, rain) */
#define DUTY_SPEED_30           30     /* 30% strict speed limit cap */
#define DUTY_NORMAL             50     /* 50% normal military reconnaissance cruise */
#define DUTY_OVERTAKE_MAX       75     /* 75% max burst speed when clear */

/* =========================================================================
 * TRAFFIC SIGN CLASSES ("det10")
 * ========================================================================= */
typedef enum {
    SIGN_NONE               = -1,
    SIGN_STOP               = 0,   /* Stop sign: full halt */
    SIGN_NO_ENTRY           = 1,   /* No entry: prohibited path, halt and reverse */
    SIGN_SPEED_LIMIT_30     = 2,   /* Speed limit 30: enforce 30% duty */
    SIGN_SCHOOL_AHEAD       = 3,   /* School ahead: 25% duty, blue alert flasher */
    SIGN_PEDESTRIAN_CROSSING= 4,   /* Pedestrian crossing: 20% duty, prepare to stop */
    SIGN_SLIPPERY_ROAD      = 5,   /* Slippery road: traction control, low torque */
    SIGN_RIGHT_HAND_CURVE   = 6,   /* Right hand curve: differential skid steer right */
    SIGN_GIVE_WAY           = 7,   /* Give way: yield, scan intersection */
    SIGN_NO_PARKING         = 8,   /* No parking: maintain continuous cruising */
    SIGN_NARROW_ROAD_AHEAD  = 9,   /* Narrow road ahead: center path, 25% duty */
    NUM_DET10_CLASSES       = 10
} det10_class_id_t;

/* Human-readable label strings */
static const char *const DET10_CLASS_NAMES[NUM_DET10_CLASSES] = {
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
};

/* Vehicle high-level operational state */
typedef enum {
    VEHICLE_STATE_INIT = 0,
    VEHICLE_STATE_CLEAR,           /* Clear path, normal autonomous cruising */
    VEHICLE_STATE_SLOW_ZONE,       /* Approaching obstacle or caution zone */
    VEHICLE_STATE_OBSTACLE_STOP,   /* Obstacle within critical distance */
    VEHICLE_STATE_SIGN_STOP,       /* Mandatory stop sign or no entry enforced */
    VEHICLE_STATE_SIGN_LIMITED,    /* Speed capped by traffic sign */
    VEHICLE_STATE_CURVE_MANEUVER,  /* Executing right-hand skid steer turn */
    VEHICLE_STATE_EMERGENCY        /* Sensor failure or external e-stop */
} vehicle_state_t;

/* Motor drive motion command */
typedef enum {
    MOTOR_CMD_BRAKE = 0,           /* Active dynamic regenerative braking */
    MOTOR_CMD_COAST,               /* Free rolling stop */
    MOTOR_CMD_FORWARD,             /* Dual tracks forward */
    MOTOR_CMD_REVERSE,             /* Dual tracks reverse */
    MOTOR_CMD_SKID_RIGHT,          /* Left faster than right */
    MOTOR_CMD_SKID_LEFT,           /* Right faster than left */
    MOTOR_CMD_PIVOT_RIGHT,         /* Left forward, right reverse (tank spin) */
    MOTOR_CMD_PIVOT_LEFT           /* Left reverse, right forward */
} motor_command_t;

/* =========================================================================
 * VEHICLE TELEMETRY & ADAS STATUS STRUCTURE (Thread-Safe Shared State)
 * ========================================================================= */
typedef struct {
    pthread_mutex_t lock;

    /* ToF Sensor Ranging */
    float tof_distance_cm;
    float tof_distance_raw_cm;
    bool  tof_sensor_valid;
    uint32_t tof_reading_counter;

    /* V800 Motor Driver */
    motor_command_t motor_cmd;
    int duty_left;                 /* 0 - 100 % */
    int duty_right;                /* 0 - 100 % */
    bool v800_driver_active;

    /* Machine Learning Vision */
    det10_class_id_t detected_sign;
    float sign_confidence;
    char sign_label[32];
    uint32_t vision_frame_count;
    float vision_fps;
    bool camera_active;

    /* Autonomous Supervisor State */
    vehicle_state_t vehicle_state;
    char vehicle_id[16];
    char mode_str[16];
    char state_str[32];
    char status_msg[128];
    int enforced_speed_limit;

    /* Emergency & System Flags */
    bool emergency_stop_active;
    bool system_shutdown;
} sdv_vehicle_context_t;

#ifdef __cplusplus
}
#endif

#endif /* SDV_TYPES_H */
