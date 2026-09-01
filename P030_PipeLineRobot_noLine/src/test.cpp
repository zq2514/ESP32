#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

void setup(){
    Serial.begin(115200);
}

void loop(){
    Serial.printf("OK!");
    delay(1000);

}