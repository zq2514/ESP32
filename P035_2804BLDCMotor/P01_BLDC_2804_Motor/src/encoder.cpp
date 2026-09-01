/**
 * @file encoder.cpp
 * @brief AS5600 I2C encoder driver implementation.
 *
 * Handles I2C bus reset, initialisation, register read, magnet
 * detection, and full-bus scanning.  All I2C errors are rate-limited
 * so they never flood the serial console.
 */

#include "config.h"
#include "encoder.h"
#include <Wire.h>

// ============================================================================
//  LOCAL STATE
// ============================================================================
static uint8_t  g_addr        = AS5600_DEFAULT_ADDR;
static uint32_t g_clock       = I2C_DEFAULT_CLOCK;
static bool     g_found       = false;
static uint32_t g_lastErrMs   = 0;
static uint32_t g_errCount    = 0;

// Known encoder addresses (tried in order of likelihood)
static const uint8_t KNOWN_ADDRS[] = {
    0x36, 0x37, 0x40, 0x38, 0x39, 0x3A, 0x3B
};
static const uint8_t NUM_KNOWN = sizeof(KNOWN_ADDRS) / sizeof(KNOWN_ADDRS[0]);

// ============================================================================
//  I2C BUS RESET
// ============================================================================

/** Send up to 9 clock pulses on SCL to unstick a hung bus. */
static void bus_reset() {
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);
    delayMicroseconds(10);

    if (digitalRead(PIN_I2C_SDA) != LOW) return;   // not stuck

    Serial.println(F("[I2C] Bus appears stuck (SDA low), attempting reset..."));
    pinMode(PIN_I2C_SCL, OUTPUT);
    for (int i = 0; i < 9; i++) {
        digitalWrite(PIN_I2C_SCL, LOW);
        delayMicroseconds(5);
        digitalWrite(PIN_I2C_SCL, HIGH);
        delayMicroseconds(5);
        if (digitalRead(PIN_I2C_SDA) == HIGH) break;
    }
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);

    if (digitalRead(PIN_I2C_SDA) == HIGH)
        Serial.println(F("[I2C] Bus reset successful"));
    else
        Serial.println(F("[I2C] Bus still stuck -- check wiring / pull-up resistors"));
}

// ============================================================================
//  I2C INITIALISATION
// ============================================================================

static void i2c_reinit() {
    Wire.end();
    delay(50);

    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);
    delay(10);

    bus_reset();
    delay(10);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(g_clock);

    Serial.printf("[I2C] Initialised: SDA=GPIO%d SCL=GPIO%d Clock=%lu Hz\n",
                  PIN_I2C_SDA, PIN_I2C_SCL, g_clock);
}

// ============================================================================
//  ERROR LOGGING (rate-limited)
// ============================================================================

static void log_error() {
    g_errCount++;
    uint32_t now = millis();
    if (now - g_lastErrMs >= I2C_ERROR_COOLDOWN_MS) {
        g_lastErrMs = now;
        Serial.printf("[I2C] ERROR: encoder at 0x%02X not responding "
                      "(%u errors total, rate-limited to 1 per %u s)\n",
                      g_addr, g_errCount,
                      (unsigned)(I2C_ERROR_COOLDOWN_MS / 1000));
    }
}

// ============================================================================
//  ENCODER DETECTION
// ============================================================================

static uint8_t detect() {
    Serial.println(F("[I2C] Searching for encoder..."));

    for (uint8_t i = 0; i < NUM_KNOWN; i++) {
        uint8_t a = KNOWN_ADDRS[i];
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C] Encoder found at 0x%02X!\n", a);
            return a;
        }
    }

    Serial.println(F("[I2C] No known encoder address responded. Full scan:"));
    int found = 0;
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C]   Device found at 0x%02X\n", a);
            found++;
        }
    }

    if (found == 0) {
        Serial.println(F("[I2C]   *** NO devices found on I2C bus! ***"));
        Serial.println(F("[I2C]   Possible causes:"));
        Serial.println(F("[I2C]     1. SDA/SCL wiring incorrect or disconnected"));
        Serial.println(F("[I2C]     2. Missing pull-up resistors (need ~3.3k-10k to 3.3V)"));
        Serial.println(F("[I2C]     3. Encoder not powered (check VCC/GND)"));
        Serial.println(F("[I2C]     4. Wrong GPIO pins (should be 21=SDA, 22=SCL)"));
    }
    return 0;
}

// ============================================================================
//  PUBLIC API
// ============================================================================

void encoder_init() {
    i2c_reinit();
    uint8_t a = detect();
    g_addr  = (a != 0) ? a : AS5600_DEFAULT_ADDR;
    g_found = (a != 0);

    if (g_found) {
        if (encoder_magnet_ok())
            Serial.println(F("[INFO] Magnet detected -- encoder ready"));
        else
            Serial.println(F("[INFO] Encoder found but magnet not detected"));
    }
}

bool encoder_is_found() {
    return g_found;
}

uint16_t encoder_read_raw() {
    if (!g_found) return 0xFFFF;

    Wire.beginTransmission(g_addr);
    Wire.write(AS5600_RAW_ANGLE);
    if (Wire.endTransmission(false) != 0) {
        log_error();
        return 0xFFFF;
    }

    Wire.requestFrom(g_addr, (uint8_t)2);
    if (Wire.available() < 2) {
        log_error();
        return 0xFFFF;
    }

    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    return ((uint16_t)hi << 8) | lo;
}

float encoder_read_radians() {
    uint16_t raw = encoder_read_raw();
    if (raw == 0xFFFF) return NAN;
    return (float)raw * (2.0f * PI) / (float)ENCODER_CPR;
}

bool encoder_magnet_ok() {
    if (!g_found) return false;

    Wire.beginTransmission(g_addr);
    Wire.write(AS5600_STATUS);
    if (Wire.endTransmission(false) != 0) return false;

    Wire.requestFrom(g_addr, (uint8_t)1);
    if (Wire.available() < 1) return false;

    return (Wire.read() & 0x08) != 0;   // bit3 = magnet detected
}

void encoder_scan_i2c() {
    Serial.println();
    Serial.println(F("========================================="));
    Serial.printf("  I2C Bus Scan (clock=%lu Hz)\n", g_clock);
    Serial.println(F("========================================="));
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            const char *label = "";
            if (addr == 0x36 || addr == 0x37)      label = " <- AS5600?";
            else if (addr == 0x40)                 label = " <- AS5048A?";
            else if (addr == 0x3C || addr == 0x3D) label = " <- OLED?";
            Serial.printf("  [+] 0x%02X%s\n", addr, label);
            found++;
        }
    }
    if (found == 0) {
        Serial.println(F("  [--] NO DEVICES FOUND"));
        Serial.println(F("  ---------------------------------"));
        Serial.println(F("  Troubleshooting:"));
        Serial.println(F("  1. Are SDA(21) and SCL(22) connected?"));
        Serial.println(F("  2. Does the encoder have power (3.3V / GND)?"));
        Serial.println(F("  3. Do you have pull-up resistors?"));
        Serial.println(F("     -> Try: connect 4.7k from SDA to 3.3V"));
        Serial.println(F("     -> and  4.7k from SCL to 3.3V"));
        Serial.println(F("  4. Try lower clock: 'i2cspeed 50'"));
    }
    Serial.printf("  Done: %d device(s) found.\n", found);
    Serial.println(F("========================================="));
    Serial.println();
}

void encoder_set_address(uint8_t addr) {
    g_addr = addr;
    Wire.beginTransmission(g_addr);
    g_found = (Wire.endTransmission() == 0);
    if (g_found)
        Serial.printf("[OK] Encoder found at 0x%02X\n", g_addr);
    else
        Serial.printf("[WARN] No response at 0x%02X\n", g_addr);
}

void encoder_set_clock(uint32_t hz) {
    g_clock = hz;
    Serial.printf("[OK] I2C clock set to %lu Hz. Reinitialising...\n", g_clock);
    i2c_reinit();
    uint8_t a = detect();
    g_addr  = (a != 0) ? a : AS5600_DEFAULT_ADDR;
    g_found = (a != 0);
    if (g_found)
        Serial.printf("[OK] Encoder re-detected at 0x%02X\n", g_addr);
}

uint8_t  encoder_get_address()     { return g_addr; }
uint32_t encoder_get_clock()       { return g_clock; }
uint32_t encoder_get_error_count() { return g_errCount; }
