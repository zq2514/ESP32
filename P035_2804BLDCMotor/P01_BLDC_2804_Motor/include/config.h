/**
 * @file config.h
 * @brief Hardware pin mapping, motor parameters, and compile-time constants.
 *
 * Every magic number in the project lives here.  Change pin assignments,
 * PWM frequency, or PI gains in one place and everything rebuilds
 * consistently.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
//  GPIO PIN MAPPING
// ============================================================================
#define PIN_PHASE_A       14      // Phase A PWM signal
#define PIN_PHASE_B       27      // Phase B PWM signal
#define PIN_PHASE_C       26      // Phase C PWM signal
#define PIN_ENABLE        25      // Driver enable (HIGH = outputs active)

#define PIN_I2C_SDA       21      // Encoder I2C data
#define PIN_I2C_SCL       22      // Encoder I2C clock

// ============================================================================
//  MOTOR PARAMETERS
// ============================================================================
#define MOTOR_POLE_PAIRS  7       // 2804 motor: 14 poles = 7 pole pairs
#define ENCODER_CPR       4096    // AS5600: 12-bit = 4096 counts per revolution

// ============================================================================
//  PWM CONFIGURATION  (ESP32 LEDC)
// ============================================================================
#define PWM_FREQ          20000   // 20 kHz — above audible range
#define PWM_RESOLUTION    10      // 10-bit resolution → 0~1023
#define PWM_MAX           1023    // Maximum duty value

#define LEDC_CH_A         0       // LEDC channel for Phase A
#define LEDC_CH_B         1       // LEDC channel for Phase B
#define LEDC_CH_C         2       // LEDC channel for Phase C

// ============================================================================
//  ENCODER — AS5600 REGISTER MAP
// ============================================================================
// Common I2C addresses for magnetic encoders:
//   0x36 — AS5600 default (ADDR pin = GND)
//   0x37 — AS5600 (ADDR pin = VCC)
//   0x38, 0x39, 0x3A, 0x3B — AS5600 alternate
//   0x40 — AS5048A (another common magnetic encoder)
#define AS5600_DEFAULT_ADDR  0x36
#define AS5600_RAW_ANGLE     0x0C   // Raw angle register (12-bit, 2 bytes)
#define AS5600_STATUS        0x0B   // bit5=TooStrong, bit4=TooWeak, bit3=Detected
#define AS5600_MANGLE        0x0E   // Magnitude (optional, for diagnostics)

// ============================================================================
//  6-STEP COMMUTATION TABLE
// ============================================================================
// State codes for each phase:
//    1 = HIGH  (top FET on,  connects phase to Vbus)
//    0 = LOW   (bottom FET on, connects phase to GND)
//   -1 = PWM   (modulated for speed control)
//
// The sequence below drives clockwise (CW) rotation.
// Reverse the array order for CCW.
//
// NOTE: the actual table instance lives in motor.cpp so every
// translation unit that includes this header doesn't get its own copy.

struct CommStep {
    int8_t a, b, c;
};

extern const CommStep COMM_TABLE[6];

// ============================================================================
//  SPEED PI CONTROLLER GAINS
// ============================================================================
// Increase KI if the motor cannot hold a target speed under load;
// decrease KP if speed hunting / oscillation is visible.
#define PI_KP              0.3f    // Proportional gain
#define PI_KI              1.0f    // Integral gain
#define PI_MAX_I           30.0f   // Integral anti-windup ceiling
#define PI_DUTY_MAX        35.0f   // Hard duty-cap in speed mode (%)

// ============================================================================
//  SOFT-START RAMP
// ============================================================================
#define RAMP_RATE          0.5f    // Max duty-change per second (0.5 → 2 s to 100 %)

// ============================================================================
//  DIAGNOSTICS
// ============================================================================
#define I2C_ERROR_COOLDOWN_MS   5000   // Print at most one I2C error every 5 s

// Default I2C clock (Hz) — 100 kHz is conservative and works without
// external pull-up resistors on most setups.
#define I2C_DEFAULT_CLOCK   100000

#endif  // CONFIG_H
