/**
 * @file motor.h
 * @brief BLDC motor control — 6-step commutation, PI speed loop,
 *        phase-offset sweep calibration, and open-loop diagnostics.
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

// ---- lifecycle -----------------------------------------------------------

/** Initialise LEDC PWM channels and enable pin.  Call once in setup(). */
void motor_init();

/** Must be called every loop iteration (~10 kHz). */
void motor_run();

// ---- power ---------------------------------------------------------------

/** Enable the driver outputs.  Refuses if encoder is not detected. */
void motor_enable();

/** Disable driver outputs (coast stop), reset ramps and PI state. */
void motor_disable();

bool motor_is_enabled();

// ---- control mode (mutually exclusive) -----------------------------------

/** Voltage / duty-cycle mode.  pct in 0..100.  Exits speed mode. */
void motor_set_duty(float pct_0_100);

/**
 * @brief Speed closed-loop (PI) mode.
 * @param rpm  Signed mechanical RPM: +CW, -CCW, 0 = exit speed mode.
 */
void motor_set_speed(float rpm);

// ---- tuning --------------------------------------------------------------

/** Electrical phase offset in degrees.  Adjusted by sweep / manually. */
void motor_set_offset(float deg);

/** Rotation direction (voltage mode only; speed mode uses rpm sign). */
void motor_set_direction(bool cw);

// ---- calibration & diagnostics -------------------------------------------

/** Sweep 12 offsets, pick the one with strongest CW rotation. */
void motor_sweep();

/**
 * @brief Open-loop 6-step at fixed rate — hardware verification.
 * @param dutyPct   PWM duty 1..50 %
 * @param stepMs    Milliseconds per commutation step
 */
void motor_openloop(float dutyPct, uint32_t stepMs);

// ---- getters (for status display) ----------------------------------------

float    motor_get_target_duty();    // 0..100 %
float    motor_get_actual_duty();    // 0..100 %
float    motor_get_phase_offset();   // degrees
bool     motor_get_direction();      // true = CW
float    motor_get_target_speed();   // signed RPM (0 = voltage mode)
float    motor_get_actual_speed();   // signed RPM

/** Print a one-page status summary to Serial. */
void motor_print_status();

#endif  // MOTOR_H
