/**
 * @file v800_motor.c
 * @brief High-Power V800 Dual-Channel MOSFET Motor Driver Implementation for QNX RTOS
 */

#include "v800_motor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int v800_motor_init(v800_motor_controller_t *ctrl, volatile uint32_t *gpio_base) {
    if (!ctrl) return -1;
    memset(ctrl, 0, sizeof(*ctrl));

    ctrl->gpio_regs = gpio_base;
    ctrl->max_slew_rate_pct_per_cycle = 8; /* Smooth 8% increment per 10ms cycle */
    ctrl->dynamic_brake_engaged = false;

    if (ctrl->gpio_regs) {
        ctrl->hardware_mapped = true;

        /* Set pins as outputs:
         * GPIO17 (GPFSEL1 bits 21-23 -> 001)
         * GPIO18 (GPFSEL1 bits 24-26 -> 001)
         * GPIO22 (GPFSEL2 bits 6-8   -> 001)
         * GPIO24 (GPFSEL2 bits 12-14 -> 001)
         * GPIO27 (GPFSEL2 bits 21-23 -> 001)
         */
        ctrl->gpio_regs[GPFSEL1] &= ~((7 << 21) | (7 << 24));
        ctrl->gpio_regs[GPFSEL1] |=  ((1 << 21) | (1 << 24));

        ctrl->gpio_regs[GPFSEL2] &= ~((7 << 6) | (7 << 12) | (7 << 21));
        ctrl->gpio_regs[GPFSEL2] |=  ((1 << 6) | (1 << 12) | (1 << 21));

        /* Enable V800 Driver */
        ctrl->gpio_regs[GPSET0] = V800_ENABLE_PIN;

        /* Initialize motors to stopped state */
        ctrl->gpio_regs[GPCLR0] = V800_PWM_LEFT | V800_PWM_RIGHT | V800_DIR_LEFT | V800_DIR_RIGHT;
        printf("[V800] Hardware GPIO initialized successfully.\n");
    } else {
        ctrl->hardware_mapped = false;
        printf("[V800] Running in simulation/headless mode.\n");
    }

    return 0;
}

static int clamp_duty(int val) {
    if (val > 100) return 100;
    if (val < -100) return -100;
    return val;
}

void v800_motor_set_speeds(v800_motor_controller_t *ctrl, int duty_left, int duty_right) {
    if (!ctrl) return;
    ctrl->target_duty_left = clamp_duty(duty_left);
    ctrl->target_duty_right = clamp_duty(duty_right);
    ctrl->dynamic_brake_engaged = false;
}

void v800_motor_forward(v800_motor_controller_t *ctrl, int duty) {
    int d = clamp_duty(abs(duty));
    v800_motor_set_speeds(ctrl, d, d);
}

void v800_motor_reverse(v800_motor_controller_t *ctrl, int duty) {
    int d = clamp_duty(abs(duty));
    v800_motor_set_speeds(ctrl, -d, -d);
}

void v800_motor_steer(v800_motor_controller_t *ctrl, int base_duty, float steer_bias) {
    if (!ctrl) return;
    if (steer_bias > 1.0f) steer_bias = 1.0f;
    if (steer_bias < -1.0f) steer_bias = -1.0f;

    /* Differential tank drive calculation:
     * steer_bias > 0 (turning right): left track speeds up, right track slows down
     * steer_bias < 0 (turning left): right track speeds up, left track slows down
     */
    float left_factor  = 1.0f + (steer_bias * 0.5f);
    float right_factor = 1.0f - (steer_bias * 0.7f);

    int dl = clamp_duty((int)((float)base_duty * left_factor));
    int dr = clamp_duty((int)((float)base_duty * right_factor));

    v800_motor_set_speeds(ctrl, dl, dr);
}

void v800_motor_brake(v800_motor_controller_t *ctrl) {
    if (!ctrl) return;
    ctrl->target_duty_left = 0;
    ctrl->target_duty_right = 0;
    ctrl->current_duty_left = 0;
    ctrl->current_duty_right = 0;
    ctrl->dynamic_brake_engaged = true;

    if (ctrl->hardware_mapped && ctrl->gpio_regs) {
        /* Active Dynamic Braking: drive both PWM pins low, reverse direction briefly */
        ctrl->gpio_regs[GPCLR0] = V800_PWM_LEFT | V800_PWM_RIGHT;
        ctrl->gpio_regs[GPSET0] = V800_DIR_LEFT | V800_DIR_RIGHT;
        usleep(5000); /* 5ms dynamic counter-EMF damping */
        ctrl->gpio_regs[GPCLR0] = V800_DIR_LEFT | V800_DIR_RIGHT;
    }
}

void v800_motor_coast(v800_motor_controller_t *ctrl) {
    if (!ctrl) return;
    ctrl->target_duty_left = 0;
    ctrl->target_duty_right = 0;
    ctrl->current_duty_left = 0;
    ctrl->current_duty_right = 0;
    ctrl->dynamic_brake_engaged = false;

    if (ctrl->hardware_mapped && ctrl->gpio_regs) {
        ctrl->gpio_regs[GPCLR0] = V800_PWM_LEFT | V800_PWM_RIGHT | V800_DIR_LEFT | V800_DIR_RIGHT;
    }
}

static void apply_ramp(int target, int *current, int max_step) {
    if (*current < target) {
        *current += max_step;
        if (*current > target) *current = target;
    } else if (*current > target) {
        *current -= max_step;
        if (*current < target) *current = target;
    }
}

void v800_motor_pwm_cycle(v800_motor_controller_t *ctrl, int slice_duration_us) {
    if (!ctrl) return;
    ctrl->pwm_cycle_count++;

    if (ctrl->dynamic_brake_engaged) {
        if (ctrl->hardware_mapped && ctrl->gpio_regs) {
            ctrl->gpio_regs[GPCLR0] = V800_PWM_LEFT | V800_PWM_RIGHT;
        }
        usleep(slice_duration_us);
        return;
    }

    /* Apply smooth S-curve slew rate ramp */
    apply_ramp(ctrl->target_duty_left,  &ctrl->current_duty_left,  ctrl->max_slew_rate_pct_per_cycle);
    apply_ramp(ctrl->target_duty_right, &ctrl->current_duty_right, ctrl->max_slew_rate_pct_per_cycle);

    int left_duty  = ctrl->current_duty_left;
    int right_duty = ctrl->current_duty_right;

    if (!ctrl->hardware_mapped || !ctrl->gpio_regs) {
        usleep(slice_duration_us);
        return;
    }

    /* Set direction pins */
    if (left_duty >= 0) {
        ctrl->gpio_regs[GPCLR0] = V800_DIR_LEFT; /* Forward */
    } else {
        ctrl->gpio_regs[GPSET0] = V800_DIR_LEFT; /* Reverse */
    }

    if (right_duty >= 0) {
        ctrl->gpio_regs[GPCLR0] = V800_DIR_RIGHT; /* Forward */
    } else {
        ctrl->gpio_regs[GPSET0] = V800_DIR_RIGHT; /* Reverse */
    }

    int left_abs  = abs(left_duty);
    int right_abs = abs(right_duty);

    /* Software PWM slice breakdown (e.g. 10ms cycle) */
    int on_us_left   = (slice_duration_us * left_abs) / 100;
    int off_us_left  = slice_duration_us - on_us_left;

    if (left_abs == 0 && right_abs == 0) {
        ctrl->gpio_regs[GPCLR0] = V800_PWM_LEFT | V800_PWM_RIGHT;
        usleep(slice_duration_us);
    } else {
        /* Drive active phase */
        if (left_abs > 0)  ctrl->gpio_regs[GPSET0] = V800_PWM_LEFT;
        if (right_abs > 0) ctrl->gpio_regs[GPSET0] = V800_PWM_RIGHT;
        usleep(on_us_left > 0 ? on_us_left : 500);

        /* Drive low phase */
        ctrl->gpio_regs[GPCLR0] = V800_PWM_LEFT | V800_PWM_RIGHT;
        if (off_us_left > 0) usleep(off_us_left);
    }
}
