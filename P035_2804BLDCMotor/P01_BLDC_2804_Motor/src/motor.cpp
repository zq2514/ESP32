/**
 * @file motor.cpp
 * @brief BLDC motor control implementation.
 *
 * 6-step trapezoidal commutation with encoder feedback,
 * PI speed control, soft-start ramp, phase-offset sweep calibration,
 * and open-loop diagnostic stepping.
 */

#include "config.h"
#include "encoder.h"
#include "motor.h"

// ============================================================================
//  COMMUTATION TABLE  (the one real instance)
// ============================================================================
const CommStep COMM_TABLE[6] = {
    // Electrical angle sector        A      B      C
    /* Sector 0:   0° ~  60° */   {  1,     0,    -1 },
    /* Sector 1:  60° ~ 120° */   {  1,    -1,     0 },
    /* Sector 2: 120° ~ 180° */   {  0,    -1,     1 },
    /* Sector 3: 180° ~ 240° */   { -1,     0,     1 },
    /* Sector 4: 240° ~ 300° */   { -1,     1,     0 },
    /* Sector 5: 300° ~ 360° */   {  0,     1,    -1 },
};

// ============================================================================
//  LOCAL STATE
// ============================================================================
static bool  g_enabled       = false;
static bool  g_direction     = true;      // true = CW
static float g_targetDuty    = 0.15f;     // 0.0 ~ 1.0
static float g_phaseOffset   = 0.0f;      // electrical degrees
static float g_rampDuty      = 0.0f;      // soft-start actual duty

// Speed PI
static float g_targetSpeed   = 0.0f;      // signed RPM; 0 = voltage mode
static float g_actualSpeed   = 0.0f;      // low-pass filtered RPM
static float g_piIntegral    = 0.0f;
static float g_prevAngle     = NAN;
static uint32_t g_prevUs     = 0;

// ============================================================================
//  PWM HELPERS
// ============================================================================

static void write_phase(uint8_t channel, int8_t state, uint32_t duty) {
    switch (state) {
        case  1: ledcWrite(channel, PWM_MAX); break;
        case  0: ledcWrite(channel, 0);       break;
        case -1: ledcWrite(channel, duty);    break;
        default: break;
    }
}

static void all_phases_off() {
    ledcWrite(LEDC_CH_A, 0);
    ledcWrite(LEDC_CH_B, 0);
    ledcWrite(LEDC_CH_C, 0);
}

// ============================================================================
//  COMMUTATION
// ============================================================================

static void update_commutation(uint32_t duty) {
    float mech = encoder_read_radians();
    if (isnan(mech)) {
        all_phases_off();
        return;
    }

    float elec = mech * MOTOR_POLE_PAIRS;
    elec += g_phaseOffset * (PI / 180.0f);

    elec = fmodf(elec, 2.0f * PI);
    if (elec < 0.0f) elec += 2.0f * PI;

    int sector = (int)(elec / (PI / 3.0f)) % 6;
    if (!g_direction) sector = 5 - sector;

    const CommStep &s = COMM_TABLE[sector];
    write_phase(LEDC_CH_A, s.a, duty);
    write_phase(LEDC_CH_B, s.b, duty);
    write_phase(LEDC_CH_C, s.c, duty);
}

// ============================================================================
//  SPEED MEASUREMENT + PI CONTROLLER
// ============================================================================

static void update_speed_pi(uint32_t nowUs) {
    if (fabsf(g_targetSpeed) < 0.1f) return;   // not in speed mode

    float angle = encoder_read_radians();
    if (isnan(angle)) return;

    if (!isnan(g_prevAngle)) {
        float dA = angle - g_prevAngle;
        if (dA >  PI) dA -= 2.0f * PI;
        if (dA < -PI) dA += 2.0f * PI;

        float dt = (float)(nowUs - g_prevUs) * 1.0e-6f;
        if (dt > 0.0005f) {
            float rpm = (dA / dt) * 60.0f / (2.0f * PI);
            g_actualSpeed = g_actualSpeed * 0.7f + rpm * 0.3f;

            float error = g_targetSpeed - g_actualSpeed;
            float pTerm = PI_KP * error;
            g_piIntegral += PI_KI * error * dt;

            if (g_piIntegral >  PI_MAX_I) g_piIntegral =  PI_MAX_I;
            if (g_piIntegral < -PI_MAX_I) g_piIntegral = -PI_MAX_I;

            static float prevErr = 0.0f;
            if (error * prevErr < 0.0f) g_piIntegral *= 0.3f;
            prevErr = error;

            float out = (pTerm + g_piIntegral) / 100.0f;
            float d   = fabsf(out);
            if (d > PI_DUTY_MAX / 100.0f) d = PI_DUTY_MAX / 100.0f;
            g_targetDuty = d;
        }
    }
    g_prevAngle = angle;
    g_prevUs    = nowUs;
}

// ============================================================================
//  SOFT-START RAMP
// ============================================================================

static void update_ramp(float dt) {
    float target = g_enabled ? g_targetDuty : 0.0f;
    float step   = RAMP_RATE * dt;
    if (g_rampDuty < target) {
        g_rampDuty += step;
        if (g_rampDuty > target) g_rampDuty = target;
    } else if (g_rampDuty > target) {
        g_rampDuty -= step;
        if (g_rampDuty < target) g_rampDuty = target;
    }
}

// ============================================================================
//  PUBLIC API
// ============================================================================

void motor_init() {
    ledcSetup(LEDC_CH_A, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(LEDC_CH_B, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(LEDC_CH_C, PWM_FREQ, PWM_RESOLUTION);

    ledcAttachPin(PIN_PHASE_A, LEDC_CH_A);
    ledcAttachPin(PIN_PHASE_B, LEDC_CH_B);
    ledcAttachPin(PIN_PHASE_C, LEDC_CH_C);

    all_phases_off();

    pinMode(PIN_ENABLE, OUTPUT);
    digitalWrite(PIN_ENABLE, LOW);
}

void motor_run() {
    static uint32_t lastRampUs = 0;
    uint32_t nowUs = micros();
    float dt = (float)(nowUs - lastRampUs) * 1.0e-6f;
    lastRampUs = nowUs;
    if (dt > 0.1f) dt = 0.1f;
    if (dt <= 0.0f) dt = 1.0e-6f;

    update_ramp(dt);

    if (g_enabled && fabsf(g_targetSpeed) > 0.1f)
        update_speed_pi(nowUs);

    if (g_enabled && g_rampDuty > 0.001f) {
        uint32_t dv = (uint32_t)(g_rampDuty * (float)PWM_MAX);
        update_commutation(dv);
    } else if (!g_enabled) {
        all_phases_off();
    }
}

// ---- power ---------------------------------------------------------------

void motor_enable() {
    if (!encoder_is_found()) {
        Serial.println(F("[ERR] Cannot start: encoder not detected!"));
        return;
    }
    digitalWrite(PIN_ENABLE, HIGH);
    g_enabled = true;
    Serial.println("[OK] Motor enabled");
}

void motor_disable() {
    digitalWrite(PIN_ENABLE, LOW);
    g_enabled    = false;
    g_rampDuty   = 0.0f;
    g_piIntegral = 0.0f;
    g_prevAngle  = NAN;
    g_actualSpeed = 0.0f;
    all_phases_off();
    Serial.println("[OK] Motor disabled");
}

bool motor_is_enabled() { return g_enabled; }

// ---- control mode --------------------------------------------------------

void motor_set_duty(float pct) {
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    g_targetDuty  = pct / 100.0f;
    g_targetSpeed = 0.0f;
    g_piIntegral  = 0.0f;
    Serial.printf("[OK] Target duty set to %.1f %% (voltage mode)\n", pct);
}

void motor_set_speed(float rpm) {
    g_targetSpeed = rpm;
    g_piIntegral  = 0.0f;
    if (fabsf(rpm) < 0.1f) {
        Serial.println(F("[OK] Speed mode OFF -- back to duty control"));
    } else {
        g_direction = (rpm >= 0.0f);
        Serial.printf("[OK] Target: %.0f RPM %s (PI control)\n",
                      fabsf(rpm), g_direction ? "CW" : "CCW");
    }
}

// ---- tuning --------------------------------------------------------------

void motor_set_offset(float deg) {
    g_phaseOffset = deg;
    Serial.printf("[OK] Phase offset set to %.1f deg\n", deg);
}

void motor_set_direction(bool cw) {
    g_direction = cw;
    Serial.printf("[OK] Direction: %s\n", cw ? "CW" : "CCW");
}

// ---- calibration ---------------------------------------------------------

void motor_sweep() {
    if (!encoder_is_found()) {
        Serial.println(F("[ERR] Encoder not detected -- cannot sweep"));
        return;
    }

    float origOffset  = g_phaseOffset;
    bool  origEnabled = g_enabled;

    digitalWrite(PIN_ENABLE, LOW);
    all_phases_off();
    delay(150);

    const float    testDuty    = 0.15f;
    const uint32_t testDutyVal = (uint32_t)(testDuty * PWM_MAX);
    const uint32_t testTimeMs  = 800;
    const uint32_t coolDownMs  = 500;

    float bestOffset   = origOffset;
    float bestMovement = -999.0f;

    Serial.println();
    Serial.println(F("[SWEEP] Testing 12 offsets (every 30 deg electrical)..."));
    Serial.println(F("[SWEEP] Offset |  Movement  | Result"));
    Serial.println(F("[SWEEP] -------|------------|----------"));

    for (int i = 0; i < 12; i++) {
        float testOffset = (float)(i * 30);
        g_phaseOffset = testOffset;

        float a1 = encoder_read_radians();
        if (isnan(a1)) {
            Serial.printf("[SWEEP] %6.0f  |  ENC ERR   | skip\n", testOffset);
            continue;
        }

        digitalWrite(PIN_ENABLE, HIGH);
        uint32_t t0 = millis();
        while (millis() - t0 < testTimeMs) {
            update_commutation(testDutyVal);
            delayMicroseconds(100);
        }

        digitalWrite(PIN_ENABLE, LOW);
        all_phases_off();
        delay(50);

        float a2 = encoder_read_radians();
        if (isnan(a2)) {
            Serial.printf("[SWEEP] %6.0f  |  ENC ERR   | skip\n", testOffset);
            continue;
        }

        float delta = a2 - a1;
        if (delta >  PI) delta -= 2.0f * PI;
        if (delta < -PI) delta += 2.0f * PI;
        float dDeg = delta * 180.0f / PI;

        const char *v = "no move";
        if      (dDeg >  5.0f) v = "CW  ***";
        else if (dDeg >  1.5f) v = "CW  *  ";
        else if (dDeg < -1.5f) v = "CCW    ";

        Serial.printf("[SWEEP] %6.0f  | %+8.1f deg | %s\n",
                      testOffset, dDeg, v);

        if (dDeg > bestMovement) { bestMovement = dDeg; bestOffset = testOffset; }

        delay(coolDownMs);
    }

    if (bestMovement > 1.5f) {
        g_phaseOffset = bestOffset;
        Serial.println(F("[SWEEP] --------------------------------"));
        Serial.printf("[SWEEP] BEST offset = %.0f deg  (moved %+.1f deg)\n",
                      bestOffset, bestMovement);
        Serial.println(F("[SWEEP] Applied!  Now:  duty 12  ->  start"));
    } else {
        g_phaseOffset = origOffset;
        Serial.println(F("[SWEEP] --------------------------------"));
        Serial.println(F("[SWEEP] No offset produced clear CW rotation."));
        Serial.println(F("[SWEEP] Try:  duty 15  ->  start"));
        Serial.println(F("[SWEEP] If still nothing, check phase wiring."));
    }
    Serial.println();

    g_enabled = false;
    digitalWrite(PIN_ENABLE, LOW);
}

void motor_openloop(float dutyPct, uint32_t stepMs) {
    if (dutyPct < 1.0f)  dutyPct = 1.0f;
    if (dutyPct > 50.0f) dutyPct = 50.0f;
    if (stepMs < 10) stepMs = 10;

    const uint32_t dutyVal   = (uint32_t)(dutyPct / 100.0f * PWM_MAX);
    const uint32_t durationMs = 4000;

    Serial.printf("[OL] Open-loop: %.0f %% duty, %u ms/step, 4 s total\n",
                  dutyPct, stepMs);
    Serial.println(F("[OL] Stepping through 6 sectors repeatedly..."));

    digitalWrite(PIN_ENABLE, HIGH);
    uint32_t tStart = millis();
    int step = 0;
    while (millis() - tStart < durationMs) {
        const CommStep &s = COMM_TABLE[step];
        ledcWrite(LEDC_CH_A, (s.a == 1) ? PWM_MAX : ((s.a == -1) ? dutyVal : 0));
        ledcWrite(LEDC_CH_B, (s.b == 1) ? PWM_MAX : ((s.b == -1) ? dutyVal : 0));
        ledcWrite(LEDC_CH_C, (s.c == 1) ? PWM_MAX : ((s.c == -1) ? dutyVal : 0));
        delay(stepMs);
        step = (step + 1) % 6;
    }

    all_phases_off();
    digitalWrite(PIN_ENABLE, LOW);

    float a = encoder_read_radians();
    if (!isnan(a))
        Serial.printf("[OL] Done. Final encoder angle: %.1f deg\n", a * 180.0f / PI);
    Serial.println(F("[OL] If motor rotated: hardware is OK, just need to fix offset."));
    Serial.println(F("[OL] If motor buzzed but stayed still: check phase wiring order."));
    Serial.println();
}

// ---- getters -------------------------------------------------------------

float motor_get_target_duty()  { return g_targetDuty * 100.0f; }
float motor_get_actual_duty()  { return g_rampDuty * 100.0f; }
float motor_get_phase_offset() { return g_phaseOffset; }
bool  motor_get_direction()    { return g_direction; }
float motor_get_target_speed() { return g_targetSpeed; }
float motor_get_actual_speed() { return g_actualSpeed; }

void motor_print_status() {
    Serial.println();
    Serial.println(F("--- Motor Status ---"));
    Serial.printf("  State:       %s\n", g_enabled ? "ENABLED" : "DISABLED");
    Serial.printf("  Direction:   %s\n", g_direction ? "CW" : "CCW");
    if (fabsf(g_targetSpeed) > 0.1f) {
        Serial.printf("  Mode:        SPEED (PI)\n");
        Serial.printf("  Target RPM:  %.0f\n", g_targetSpeed);
        Serial.printf("  Actual RPM:  %.1f\n", g_actualSpeed);
    } else {
        Serial.printf("  Mode:        VOLTAGE (duty)\n");
    }
    Serial.printf("  Target duty: %.1f %%\n", g_targetDuty * 100.0f);
    Serial.printf("  Actual duty: %.1f %%\n", g_rampDuty * 100.0f);
    Serial.printf("  Phase offset: %.1f deg\n", g_phaseOffset);
    Serial.println(F("------------------------------"));
    Serial.println();
}
