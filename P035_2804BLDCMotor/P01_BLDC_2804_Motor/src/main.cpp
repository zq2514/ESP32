/**
 * @file main.cpp
 * @brief Entry point — initialise subsystems, then run the control loop.
 *
 * All hardware-specific logic lives in encoder.cpp / motor.cpp.
 * All user-interface text lives in commands.cpp.
 * This file only wires them together.
 */

#include <Arduino.h>
#include "config.h"
#include "encoder.h"
#include "motor.h"
#include "commands.h"

// ============================================================================
//  SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    commands_print_banner();

    encoder_init();
    motor_init();

    if (encoder_is_found()) {
        Serial.println(F("[INFO] Encoder ready"));
    } else {
        Serial.println(F("[WARN] Encoder not detected -- motor control disabled"));
        Serial.println(F("[WARN] Use 'scan' to troubleshoot, 'i2cspeed' to adjust clock,"));
        Serial.println(F("[WARN] or 'i2caddr <hex>' to set address manually."));
    }

    Serial.println();
    Serial.println(F("[READY] Type 'help' for command list."));
    Serial.println();
}

// ============================================================================
//  LOOP
// ============================================================================
void loop() {
    commands_process();
    motor_run();

    // Periodic status (every 2 s while motor is running)
    static uint32_t lastPrint = 0;
    if (motor_is_enabled() && millis() - lastPrint > 2000) {
        lastPrint = millis();
        float a = encoder_read_radians();
        if (!isnan(a)) {
            if (fabsf(motor_get_target_speed()) > 0.1f) {
                Serial.printf("[RUN] %4.0f/%4.0f RPM | Duty: %3.0f%% | Angle: %5.0f deg\n",
                              motor_get_actual_speed(), motor_get_target_speed(),
                              motor_get_actual_duty(), a * 180.0f / PI);
            } else {
                float elec = a * MOTOR_POLE_PAIRS;
                int sec = (int)(fmodf(elec + motor_get_phase_offset() * (PI / 180.0f),
                                      2.0f * PI) / (PI / 3.0f)) % 6;
                Serial.printf("[RUN] Duty: %3.0f%% | Sector: %d | Angle: %5.0f deg\n",
                              motor_get_actual_duty(), sec, a * 180.0f / PI);
            }
        }
    }

    delayMicroseconds(100);
}
