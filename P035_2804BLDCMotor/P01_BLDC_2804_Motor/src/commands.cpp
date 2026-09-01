/**
 * @file commands.cpp
 * @brief Serial command parser and status/help printers.
 *
 * Every user-facing text string lives here (or is reachable from here).
 * The parser delegates to the encoder and motor modules — it holds no
 * motor/encoder state of its own.
 */

#include "config.h"
#include "encoder.h"
#include "motor.h"
#include <Arduino.h>

// ============================================================================
//  HELP & STATUS (standalone — no module state needed)
// ============================================================================

static void print_help() {
    Serial.println();
    Serial.println(F("=========================================="));
    Serial.println(F("  BLDC 2804 Motor Control - Serial Commands"));
    Serial.println(F("=========================================="));
    Serial.println(F("  start           Enable motor output"));
    Serial.println(F("  stop            Disable motor output"));
    Serial.println(F("  sweep           Auto-sweep 12 offsets to find best"));
    Serial.println(F("  ol <duty> <ms>  Open-loop step test, e.g. 'ol 20 80'"));
    Serial.println(F("  speed <rpm>     Set target speed (CW=+, CCW=-)"));
    Serial.println(F("                  e.g. 'speed 30' CW, 'speed -20' CCW"));
    Serial.println(F("  duty <0-100>    Set PWM duty (voltage mode)"));
    Serial.println(F("  dir <cw|ccw>    Set rotation direction"));
    Serial.println(F("  offset <deg>    Set phase offset (degrees)"));
    Serial.println(F("  status          Print current status"));
    Serial.println(F("  scan            Full I2C bus scan"));
    Serial.println(F("  i2cspeed <kHz>  Set I2C clock (50, 100, 200, 400)"));
    Serial.println(F("  i2caddr <hex>   Set encoder I2C address manually"));
    Serial.println(F("                  (e.g. 'i2caddr 36' for 0x36)"));
    Serial.println(F("  help            Show this message"));
    Serial.println(F("=========================================="));
    Serial.println();
}

static void print_status() {
    // Encoder section
    Serial.println();
    Serial.println(F("--- Encoder Status ---"));
    Serial.printf("  Found:       %s\n", encoder_is_found() ? "YES" : "NO");
    Serial.printf("  I2C addr:    0x%02X\n", encoder_get_address());
    Serial.printf("  I2C clock:   %lu Hz\n", encoder_get_clock());
    Serial.printf("  I2C errors:  %u\n", encoder_get_error_count());

    if (encoder_is_found()) {
        uint16_t raw = encoder_read_raw();
        Serial.printf("  Raw angle:   %u / 4095\n", raw);
        float rad = encoder_read_radians();
        if (!isnan(rad)) {
            Serial.printf("  Mech angle:  %.1f deg\n", rad * 180.0f / PI);
            Serial.printf("  Elec angle:  %.1f deg\n",
                          fmodf(rad * MOTOR_POLE_PAIRS, 2.0f * PI) * 180.0f / PI);
        }
        Serial.printf("  Magnet:      %s\n", encoder_magnet_ok() ? "OK" : "NOT DETECTED");
    }
    Serial.println(F("------------------------------"));

    // Motor section
    motor_print_status();
}

// ============================================================================
//  COMMAND PARSER
// ============================================================================

void commands_process() {
    if (!Serial.available()) return;

    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    input.toLowerCase();
    int sp = input.indexOf(' ');
    String cmd = (sp > 0) ? input.substring(0, sp) : input;
    String arg = (sp > 0) ? input.substring(sp + 1) : "";

    // ---- start ----
    if (cmd == "start") {
        motor_enable();
    }
    // ---- stop ----
    else if (cmd == "stop") {
        motor_disable();
    }
    // ---- sweep ----
    else if (cmd == "sweep") {
        motor_sweep();
    }
    // ---- openloop / ol ----
    else if (cmd == "openloop" || cmd == "ol") {
        float    pct    = 20.0f;
        uint32_t stepMs = 80;
        if (arg.length() > 0) {
            int sp2 = arg.indexOf(' ');
            pct = arg.toFloat();
            if (sp2 > 0) stepMs = arg.substring(sp2 + 1).toInt();
        }
        motor_openloop(pct, stepMs);
    }
    // ---- speed <rpm> ----
    else if (cmd == "speed") {
        motor_set_speed(arg.toFloat());
    }
    // ---- duty <0-100> ----
    else if (cmd == "duty") {
        motor_set_duty(arg.toFloat());
    }
    // ---- dir <cw|ccw> ----
    else if (cmd == "dir") {
        if (arg == "cw")       motor_set_direction(true);
        else if (arg == "ccw") motor_set_direction(false);
        else Serial.println(F("[ERR] Usage: dir <cw|ccw>"));
    }
    // ---- offset <deg> ----
    else if (cmd == "offset") {
        motor_set_offset(arg.toFloat());
    }
    // ---- status ----
    else if (cmd == "status") {
        print_status();
    }
    // ---- scan ----
    else if (cmd == "scan") {
        encoder_scan_i2c();
    }
    // ---- i2cspeed <kHz> ----
    else if (cmd == "i2cspeed") {
        uint32_t khz = arg.toInt();
        if      (khz == 50)  encoder_set_clock(50000);
        else if (khz == 100) encoder_set_clock(100000);
        else if (khz == 200) encoder_set_clock(200000);
        else if (khz == 400) encoder_set_clock(400000);
        else Serial.println(F("[ERR] Valid speeds (kHz): 50, 100, 200, 400"));
    }
    // ---- i2caddr <hex> ----
    else if (cmd == "i2caddr") {
        uint8_t a = (uint8_t)strtol(arg.c_str(), NULL, 16);
        if (a < 1 || a > 127)
            Serial.println(F("[ERR] Address must be 01-7F hex. Example: i2caddr 36"));
        else
            encoder_set_address(a);
    }
    // ---- help ----
    else if (cmd == "help") {
        print_help();
    }
    // ---- unknown ----
    else {
        Serial.printf("[ERR] Unknown command: '%s'. Type 'help' for commands.\n",
                      cmd.c_str());
    }
}

// ============================================================================
//  BANNER (printed once at startup)
// ============================================================================

void commands_print_banner() {
    Serial.println();
    Serial.println(F("=========================================="));
    Serial.println(F("  BLDC 2804 Motor - 6-Step Commutation"));
    Serial.println(F("------------------------------------------"));
    Serial.println(F("  Phases:  14(A) 27(B) 26(C)  EN: 25"));
    Serial.println(F("  Encoder: I2C on 21(SDA) 22(SCL)"));
    Serial.println(F("=========================================="));
    Serial.println();
}
