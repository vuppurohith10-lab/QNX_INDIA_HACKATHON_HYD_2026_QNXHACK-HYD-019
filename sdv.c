/**
 * ============================================================================
 * QNX GLOBAL E-HACK 2026 - AUTONOMOUS SOFTWARE-DEFINED VEHICLE (SDV)
 * Real-Time Military Ground Vehicle & ADAS Edge Intelligence System
 *
 * Target: Raspberry Pi 4 Model B (Broadcom BCM2711, ARM Cortex-A72, QNX 8.0)
 *
 * Upgraded Features:
 *  1. Time-of-Flight (ToF) Sensor (VL53L0X / TF-Luna) replaces Ultrasonic
 *  2. High-Power V800 Dual-Channel MOSFET Motor Driver replaces L298N
 *  3. USB Webcam Live Video Capture & Streaming
 *  4. Edge Machine Learning Traffic Sign Detection ("det10" 10 Classes)
 *  5. Deterministic Real-Time Multi-Threading (POSIX SCHED_FIFO Priorities)
 *  6. Rich V2V Telemetry Broadcast via HC-05 Bluetooth UART to ESP32 Control Room
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <termios.h>
#include <pthread.h>
#include <sched.h>

#ifdef __QNX__
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <hw/inout.h>
#endif

#include "include/sdv_types.h"
#include "include/tof_sensor.h"
#include "include/v800_motor.h"
#include "include/traffic_sign_ml.h"

/* Global Shared Vehicle Context */
static sdv_vehicle_context_t g_vctx;
static volatile uint32_t *g_gpio = NULL;
static tof_sensor_handle_t g_tof;
static v800_motor_controller_t g_v800;

/* Clean Termination Signal Handler */
static void sigint_handler(int sig) {
    (void)sig;
    printf("\n[SDV] Shutdown signal received. Executing safe stop...\n");
    g_vctx.system_shutdown = true;
    v800_motor_brake(&g_v800);
    if (g_gpio) {
        g_gpio[GPCLR0] = GREEN_LED | RED_LED | BLUE_LED | LASER_PIN;
        g_gpio[GPCLR0] = V800_PWM_LEFT | V800_PWM_RIGHT | V800_DIR_LEFT | V800_DIR_RIGHT;
    }
    tof_sensor_close(&g_tof);
    traffic_sign_ml_shutdown();
    exit(0);
}

/* =========================================================================
 * THREAD 1: TIME-OF-FLIGHT (ToF) SENSOR THREAD (Priority 30 - SCHED_FIFO)
 * High-speed, non-blocking optical ranging & collision detection
 * ========================================================================= */
void *tof_sensor_thread(void *arg) {
    (void)arg;
    printf("[Thread] ToF Sensor Thread started (Priority: 30, SCHED_FIFO)\n");

    while (!g_vctx.system_shutdown) {
        float distance_cm = 0.0f;
        int status = tof_sensor_read_distance(&g_tof, &distance_cm);

        pthread_mutex_lock(&g_vctx.lock);
        if (status == 0) {
            g_vctx.tof_distance_cm = distance_cm;
            g_vctx.tof_sensor_valid = true;
            g_vctx.tof_reading_counter++;
        } else {
            g_vctx.tof_sensor_valid = false;
        }
        pthread_mutex_unlock(&g_vctx.lock);

        usleep(30000); /* 33 Hz ranging cycle */
    }
    return NULL;
}

/* =========================================================================
 * THREAD 2: V800 DUAL-CHANNEL MOTOR DRIVER THREAD (Priority 25 - SCHED_FIFO)
 * Executes software PWM slices, differential tank steering, dynamic brake
 * ========================================================================= */
void *motor_v800_thread(void *arg) {
    (void)arg;
    printf("[Thread] V800 Motor Controller Thread started (Priority: 25, SCHED_FIFO)\n");

    while (!g_vctx.system_shutdown) {
        /* Run high-resolution 10ms PWM slice */
        v800_motor_pwm_cycle(&g_v800, 10000);
    }

    v800_motor_brake(&g_v800);
    return NULL;
}

/* =========================================================================
 * THREAD 3: USB WEBCAM & EDGE ML VISION THREAD (Priority 18 - SCHED_RR)
 * Captures live frames, runs 10-class traffic sign model, updates telemetry
 * ========================================================================= */
void *vision_ml_thread(void *arg) {
    (void)arg;
    printf("[Thread] Vision & Edge ML Thread started (Priority: 18, SCHED_RR)\n");

    traffic_sign_ml_config_t ml_cfg;
    ml_cfg.model_path = "models/best.onnx";
    ml_cfg.labels_path = "models/labels.txt";
    ml_cfg.camera_device_id = 0;
    ml_cfg.conf_threshold = 0.45f;
    ml_cfg.nms_threshold = 0.45f;
    ml_cfg.input_width = 320;
    ml_cfg.input_height = 320;
    ml_cfg.required_frames = 2;

    if (traffic_sign_ml_init(&ml_cfg) == 0) {
        g_vctx.camera_active = true;
    } else {
        printf("[Vision] Warning: Camera/ML initialization failed. Continuing with sensor-only ADAS.\n");
        g_vctx.camera_active = false;
    }

    while (!g_vctx.system_shutdown) {
        traffic_sign_ml_step(&g_vctx);
        usleep(33000); /* ~30 FPS inference loop */
    }

    traffic_sign_ml_shutdown();
    return NULL;
}

/* =========================================================================
 * THREAD 4: ADAS AUTONOMOUS VEHICLE SUPERVISOR THREAD (Priority 22 - SCHED_FIFO)
 * Evaluates ToF distance, traffic signs, rules, LEDs, and issues motor commands
 * ========================================================================= */
void *adas_supervisor_thread(void *arg) {
    (void)arg;
    printf("[Thread] ADAS Autonomous Supervisor started (Priority: 22, SCHED_FIFO)\n");

    int curve_maneuver_ticks = 0;
    int yield_wait_ticks = 0;

    while (!g_vctx.system_shutdown) {
        float d;
        det10_class_id_t sign;
        char sign_name[32];

        pthread_mutex_lock(&g_vctx.lock);
        d = g_vctx.tof_distance_cm;
        sign = g_vctx.detected_sign;
        snprintf(sign_name, sizeof(sign_name), "%s", g_vctx.sign_label);
        pthread_mutex_unlock(&g_vctx.lock);

        /* =====================================================================
         * ARBITRATION RULE 1 (CRITICAL SAFETY): Physical Obstacle Emergency Stop
         * ToF distance <= 20 cm takes absolute precedence over all other inputs!
         * ===================================================================== */
        if (d <= DIST_EMERGENCY_STOP_CM) {
            v800_motor_brake(&g_v800);

            if (g_gpio) {
                g_gpio[GPSET0] = RED_LED | LASER_PIN;
                g_gpio[GPCLR0] = GREEN_LED | BLUE_LED;
            }

            pthread_mutex_lock(&g_vctx.lock);
            g_vctx.vehicle_state = VEHICLE_STATE_OBSTACLE_STOP;
            strcpy(g_vctx.state_str, "OBSTACLE");
            snprintf(g_vctx.status_msg, sizeof(g_vctx.status_msg),
                     "TARGET DETECTED AT %.1f CM - EMERGENCY BRAKE", d);
            g_vctx.duty_left = 0;
            g_vctx.duty_right = 0;
            pthread_mutex_unlock(&g_vctx.lock);

            usleep(50000);
            continue;
        }

        /* =====================================================================
         * ARBITRATION RULE 2: Traffic Sign ADAS Policy ("det10")
         * ===================================================================== */
        int target_duty = DUTY_NORMAL;
        float steer_bias = 0.0f;
        const char *state_label = "CLEAR";
        const char *msg_text = "NORMAL CRUISE";

        /* Handle active curve maneuver countdown */
        if (curve_maneuver_ticks > 0) {
            curve_maneuver_ticks--;
            target_duty = DUTY_SLOW;
            steer_bias = 0.70f; /* Skid steer right */
            state_label = "TRAFFIC";
            msg_text = "EXECUTING RIGHT CURVE MANEUVER";
        }
        /* Handle active yield / give-way wait */
        else if (yield_wait_ticks > 0) {
            yield_wait_ticks--;
            target_duty = DUTY_CREEP;
            state_label = "TRAFFIC";
            msg_text = "YIELDING AT INTERSECTION";
        }
        /* Evaluate newly detected sign */
        else if (sign != SIGN_NONE) {
            switch (sign) {
                case SIGN_STOP:
                    target_duty = DUTY_STOP;
                    state_label = "OBSTACLE";
                    msg_text = "STOP SIGN ENFORCED - VEHICLE HALTED";
                    break;

                case SIGN_NO_ENTRY:
                    target_duty = DUTY_STOP;
                    state_label = "EMERGENCY";
                    msg_text = "NO ENTRY - WRONG WAY WARNING";
                    break;

                case SIGN_SPEED_LIMIT_30:
                    target_duty = DUTY_SPEED_30;
                    state_label = "TRAFFIC";
                    msg_text = "SPEED LIMIT 30 KM/H ZONE ENFORCED";
                    break;

                case SIGN_SCHOOL_AHEAD:
                    target_duty = DUTY_SLOW;
                    state_label = "TRAFFIC";
                    msg_text = "SCHOOL ZONE AHEAD - CAUTION 25%";
                    break;

                case SIGN_PEDESTRIAN_CROSSING:
                    target_duty = DUTY_SLOW;
                    state_label = "TRAFFIC";
                    msg_text = "PEDESTRIAN CROSSING - YIELD SPEED";
                    break;

                case SIGN_SLIPPERY_ROAD:
                    target_duty = DUTY_SLOW;
                    state_label = "TRAFFIC";
                    msg_text = "SLIPPERY SURFACE - TRACTION CONTROL ACTIVE";
                    break;

                case SIGN_RIGHT_HAND_CURVE:
                    curve_maneuver_ticks = 40; /* 2.0 seconds curve execution */
                    target_duty = DUTY_SLOW;
                    steer_bias = 0.70f;
                    state_label = "TRAFFIC";
                    msg_text = "RIGHT HAND CURVE - STEERING ENGAGED";
                    break;

                case SIGN_GIVE_WAY:
                    yield_wait_ticks = 30; /* 1.5 seconds yield check */
                    target_duty = DUTY_CREEP;
                    state_label = "TRAFFIC";
                    msg_text = "GIVE WAY - CHECKING ROADWAY CLEARANCE";
                    break;

                case SIGN_NO_PARKING:
                    target_duty = DUTY_NORMAL;
                    state_label = "CLEAR";
                    msg_text = "NO PARKING ZONE - MAINTAINING MOTION";
                    break;

                case SIGN_NARROW_ROAD_AHEAD:
                    target_duty = DUTY_SLOW;
                    state_label = "TRAFFIC";
                    msg_text = "NARROW CORRIDOR - LANE CENTER ACTIVE";
                    break;

                default:
                    break;
            }
        }
        /* Fallback: Proximity distance grading */
        else if (d <= DIST_CAUTION_SLOW_CM) {
            target_duty = DUTY_SLOW;
            state_label = "TRAFFIC";
            msg_text = "APPROACHING OBSTACLE (SLOW ZONE)";
        } else {
            target_duty = DUTY_NORMAL;
            state_label = "CLEAR";
            msg_text = "NO TARGET - PATH CLEAR";
        }

        /* =====================================================================
         * ACTUATOR CONTROL: Update V800 Motor and Visual LEDs
         * ===================================================================== */
        if (target_duty == 0) {
            v800_motor_brake(&g_v800);
            if (g_gpio) {
                g_gpio[GPSET0] = RED_LED | LASER_PIN;
                g_gpio[GPCLR0] = GREEN_LED | BLUE_LED;
            }
        } else if (steer_bias != 0.0f) {
            v800_motor_steer(&g_v800, target_duty, steer_bias);
            if (g_gpio) {
                g_gpio[GPSET0] = BLUE_LED;
                g_gpio[GPCLR0] = RED_LED | GREEN_LED | LASER_PIN;
            }
        } else if (target_duty <= DUTY_SLOW) {
            v800_motor_forward(&g_v800, target_duty);
            if (g_gpio) {
                g_gpio[GPSET0] = BLUE_LED;
                g_gpio[GPCLR0] = RED_LED | GREEN_LED | LASER_PIN;
            }
        } else {
            v800_motor_forward(&g_v800, target_duty);
            if (g_gpio) {
                g_gpio[GPSET0] = GREEN_LED;
                g_gpio[GPCLR0] = RED_LED | BLUE_LED | LASER_PIN;
            }
        }

        /* Update shared context */
        pthread_mutex_lock(&g_vctx.lock);
        strcpy(g_vctx.state_str, state_label);
        strncpy(g_vctx.status_msg, msg_text, sizeof(g_vctx.status_msg) - 1);
        g_vctx.duty_left = g_v800.current_duty_left;
        g_vctx.duty_right = g_v800.current_duty_right;
        pthread_mutex_unlock(&g_vctx.lock);

        usleep(50000); /* 20 Hz supervisor loop */
    }
    return NULL;
}

/* =========================================================================
 * THREAD 5: V2V TELEMETRY & ESP32 CONTROL ROOM BROADCAST (Priority 10)
 * Sends formatted telemetry packets over HC-05 Bluetooth UART
 * ========================================================================= */
void *comm_telemetry_thread(void *arg) {
    (void)arg;
    printf("[Thread] V2V Telemetry Thread started (Priority: 10, SCHED_FIFO)\n");

    int fd = open(TELEMETRY_PORT, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        printf("[Comm] Warning: Could not open %s (%s). Printing telemetry to stdout.\n",
               TELEMETRY_PORT, strerror(errno));
    } else {
        struct termios options;
        if (tcgetattr(fd, &options) == 0) {
            cfsetispeed(&options, B9600);
            cfsetospeed(&options, B9600);
            options.c_cflag |= (CLOCAL | CREAD | CS8);
            options.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
            options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
            tcsetattr(fd, TCSANOW, &options);
            printf("[Comm] HC-05 Bluetooth UART opened on %s at 9600 baud.\n", TELEMETRY_PORT);
        }
    }

    while (!g_vctx.system_shutdown) {
        char packet[512];
        float d;
        char state[32];
        char sign[32];
        char msg[128];
        int dl, dr;

        pthread_mutex_lock(&g_vctx.lock);
        d = g_vctx.tof_distance_cm;
        snprintf(state, sizeof(state), "%s", g_vctx.state_str);
        snprintf(sign, sizeof(sign), "%s", g_vctx.sign_label);
        snprintf(msg, sizeof(msg), "%s", g_vctx.status_msg);
        dl = g_vctx.duty_left;
        dr = g_vctx.duty_right;
        pthread_mutex_unlock(&g_vctx.lock);

        /* Standardized V2V Telemetry Frame */
        snprintf(packet, sizeof(packet),
                 "ID:%s;MODE:%s;STATE:%s;TOF:%.1f;SIGN:%s;PWM:%d,%d;MSG:%s\n",
                 g_vctx.vehicle_id, g_vctx.mode_str, state, d, sign, dl, dr, msg);

        if (fd >= 0) {
            int n = write(fd, packet, strlen(packet));
            if (n < 0) {
                printf("[V2V TX Err] %s", packet);
            }
        } else {
            /* Console output for simulation/bench testing */
            printf("[V2V Telemetry] %s", packet);
        }

        usleep(200000); /* 5 Hz telemetry transmission */
    }

    if (fd >= 0) close(fd);
    return NULL;
}

/* =========================================================================
 * MAIN ENTRY POINT: QNX RTOS SDV INITIALIZATION & SCHEDULING
 * ========================================================================= */
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("\n============================================================\n");
    printf("🚗 QNX GLOBAL E-HACK 2026: SDV AUTONOMOUS MILITARY VEHICLE\n");
    printf("   Hardware: Raspberry Pi 4 (Cortex-A72) | QNX Neutrino 8.0\n");
    printf("   Sensors : ToF LiDAR (VL53/TFmini) | USB HD Webcam\n");
    printf("   Actuator: V800 High-Power Dual-Channel MOSFET Driver\n");
    printf("   AI Edge : 10-Class Traffic Sign Recognition (det10)\n");
    printf("============================================================\n\n");

    /* Register termination signals */
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    /* Initialize shared context */
    memset(&g_vctx, 0, sizeof(g_vctx));
    pthread_mutex_init(&g_vctx.lock, NULL);
    strcpy(g_vctx.vehicle_id, "A1206");
    strcpy(g_vctx.mode_str, "SDV_AUTONOMOUS");
    strcpy(g_vctx.state_str, "CLEAR");
    strcpy(g_vctx.sign_label, "none");
    strcpy(g_vctx.status_msg, "SYSTEM INITIALIZING");
    g_vctx.tof_distance_cm = 100.0f;
    g_vctx.detected_sign = SIGN_NONE;

#ifdef __QNX__
    /* Request I/O privilege for direct hardware register mapping */
    if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
        printf("[WARN] ThreadCtl(_NTO_TCTL_IO) failed. Running without root I/O privileges.\n");
    }

    void *gpio_ptr = mmap_device_memory(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE | PROT_NOCACHE, 0, GPIO_BASE);
    if (gpio_ptr == MAP_FAILED) {
        printf("[WARN] mmap_device_memory failed. Hardware GPIO not mapped; using simulation.\n");
        g_gpio = NULL;
    } else {
        g_gpio = (volatile uint32_t *)gpio_ptr;
        printf("[QNX] BCM2711 GPIO mapped successfully at virtual address %p\n", (void*)g_gpio);
    }
#else
    printf("[Host] Running on non-QNX host platform. Direct GPIO memory mapping skipped.\n");
    g_gpio = NULL;
#endif

    /* Configure visual indicator GPIO pins */
    if (g_gpio) {
        /* GPIO16 (GREEN_LED) */
        g_gpio[GPFSEL1] &= ~(7 << 18);
        g_gpio[GPFSEL1] |=  (1 << 18);

        /* GPIO20 (RED_LED) */
        g_gpio[GPFSEL2] &= ~(7 << 0);
        g_gpio[GPFSEL2] |=  (1 << 0);

        /* GPIO23 (BLUE_LED) */
        g_gpio[GPFSEL2] &= ~(7 << 9);
        g_gpio[GPFSEL2] |=  (1 << 9);

        /* GPIO25 (LASER_PIN) */
        g_gpio[GPFSEL2] &= ~(7 << 15);
        g_gpio[GPFSEL2] |=  (1 << 15);

        /* Initial LED states */
        g_gpio[GPSET0] = GREEN_LED;
        g_gpio[GPCLR0] = RED_LED | BLUE_LED | LASER_PIN;
    }

    /* Initialize ToF Sensor */
    tof_config_t tof_cfg;
    memset(&tof_cfg, 0, sizeof(tof_cfg));
    tof_cfg.mode = TOF_MODE_AUTO;
    tof_cfg.device_path = "/dev/i2c1";
    tof_cfg.i2c_address = 0x29;
    tof_cfg.baud_rate = 115200;
    tof_cfg.filter_alpha = 0.65f;
    tof_cfg.min_valid_cm = 2.0f;
    tof_cfg.max_valid_cm = 400.0f;
    tof_sensor_init(&g_tof, &tof_cfg);

    /* Initialize V800 Motor Driver */
    v800_motor_init(&g_v800, g_gpio);

    /* Spawn Real-Time POSIX Threads with Deterministic Priorities */
    pthread_t t_tof, t_motor, t_vision, t_adas, t_comm;
    pthread_attr_t attr;
    struct sched_param param;

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    /* Thread 1: ToF Sensor (Priority 30) */
    param.sched_priority = 30;
    pthread_attr_setschedparam(&attr, &param);
    pthread_create(&t_tof, &attr, tof_sensor_thread, NULL);

    /* Thread 2: V800 Motor (Priority 25) */
    param.sched_priority = 25;
    pthread_attr_setschedparam(&attr, &param);
    pthread_create(&t_motor, &attr, motor_v800_thread, NULL);

    /* Thread 3: ADAS Supervisor (Priority 22) */
    param.sched_priority = 22;
    pthread_attr_setschedparam(&attr, &param);
    pthread_create(&t_adas, &attr, adas_supervisor_thread, NULL);

    /* Thread 4: Vision & ML (Priority 18 - SCHED_RR) */
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 18;
    pthread_attr_setschedparam(&attr, &param);
    pthread_create(&t_vision, &attr, vision_ml_thread, NULL);

    /* Thread 5: V2V Telemetry (Priority 10 - SCHED_FIFO) */
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    param.sched_priority = 10;
    pthread_attr_setschedparam(&attr, &param);
    pthread_create(&t_comm, &attr, comm_telemetry_thread, NULL);

    printf("\n[QNX Kernel] All 5 real-time threads dispatched successfully.\n");
    printf("[QNX Kernel] Autonomous SDV mission executive running...\n\n");

    /* Await thread completion */
    pthread_join(t_tof, NULL);
    pthread_join(t_motor, NULL);
    pthread_join(t_adas, NULL);
    pthread_join(t_vision, NULL);
    pthread_join(t_comm, NULL);

    return 0;
}
