/**
 * @file encoder.h
 * @brief AS5600 (or compatible) magnetic encoder interface.
 *
 * All I2C details are hidden behind these functions.  The rest of the
 * firmware only ever asks for an angle in radians.
 */

#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

// ---- lifecycle -----------------------------------------------------------

/** One-time initialisation: reset I2C bus, detect encoder, check magnet. */
void encoder_init();

/** Return true if the encoder responded during init / last scan. */
bool encoder_is_found();

// ---- angle reading -------------------------------------------------------

/** Raw 12-bit angle (0–4095).  Returns 0xFFFF on communication error. */
uint16_t encoder_read_raw();

/** Mechanical angle in radians [0, 2π).  Returns NAN on error. */
float encoder_read_radians();

// ---- magnet --------------------------------------------------------------

/** True if the AS5600 reports a magnet within the usable range. */
bool encoder_magnet_ok();

// ---- I2C management ------------------------------------------------------

/** Full I2C-bus scan (1–127) with pretty-printed output. */
void encoder_scan_i2c();

/** Override the encoder I2C address (e.g. "i2caddr 37"). */
void encoder_set_address(uint8_t addr);

/** Change I2C clock speed and re-initialise the bus. */
void encoder_set_clock(uint32_t hz);

// ---- getters (for status display) ----------------------------------------

uint8_t  encoder_get_address();
uint32_t encoder_get_clock();
uint32_t encoder_get_error_count();

#endif  // ENCODER_H
