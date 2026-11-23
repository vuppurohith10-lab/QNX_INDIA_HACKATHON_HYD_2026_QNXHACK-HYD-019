/**
 * @file v800_motor.h
 * @brief High-Power V800 Dual-Channel MOSFET Motor Driver for QNX RTOS
 *
 * Upgraded from legacy L298N:
 * - Independent PWM + DIR dual-channel control (skid steer tank architecture)
 * - Active regenerative electronic braking
 * - S-curve velocity profiling & acceleration ramp
 * - Thermal and current-surge protection logic
 */

#ifndef V800_MOTOR_H
#define V800_MOTOR_H

#include "sdv_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile uint32_t *gpio_regs;
    bool hardware_mapped;

    /* Current speed setpoints (-100 to +100) */
    int target_duty_left;
    int target_duty_right;
    int current_duty_left;
    int current_duty_right;

    /* Ramp parameters */
    int max_slew_rate_pct_per_cycle; /* e.g., 5% change per 10ms cycle */
    bool dynamic_brake_engaged;

    /* Diagnostics */
    uint32_t pwm_cycle_count;
} v800_motor_controller_t;

/**
 * @brief Initialize V800 motor GPIO pins & controller
 * @param ctrl Pointer to controller instance
 * @param gpio_base Direct memory-mapped pointer to BCM2711 GPIO registers
 * @return 0 on success
 */
int v800_motor_init(v800_motor_controller_t *ctrl, volatile uint32_t *gpio_base);

/**
 * @brief Set independent left and right motor speeds with smooth ramp
 * @param duty_left -100 to +100 (negative = reverse, positive = forward)
 * @param duty_right -100 to +100
 */
void v800_motor_set_speeds(v800_motor_controller_t *ctrl, int duty_left, int duty_right);

/**
 * @brief Drive vehicle forward with matched tracks
 */
void v800_motor_forward(v800_motor_controller_t *ctrl, int duty);

/**
 * @brief Drive vehicle in reverse
 */
void v800_motor_reverse(v800_motor_controller_t *ctrl, int duty);

/**
 * @brief Differential skid steer steering
 * @param base_duty Base forward speed (0 - 100%)
 * @param steer_bias Range [-1.0 (hard left) to +1.0 (hard right)]
 */
void v800_motor_steer(v800_motor_controller_t *ctrl, int base_duty, float steer_bias);

/**
 * @brief Execute active dynamic regenerative brake (stops vehicle within 5-10cm)
 */
void v800_motor_brake(v800_motor_controller_t *ctrl);

/**
 * @brief Free-wheeling coast stop
 */
void v800_motor_coast(v800_motor_controller_t *ctrl);

/**
 * @brief Low-level PWM execution cycle (call within high-priority motor thread)
 * @param slice_duration_us Time spent in this PWM cycle
 */
void v800_motor_pwm_cycle(v800_motor_controller_t *ctrl, int slice_duration_us);

#ifdef __cplusplus
}
#endif

#endif /* V800_MOTOR_H */
