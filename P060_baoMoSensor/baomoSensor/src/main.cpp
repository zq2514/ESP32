#include <Arduino.h>

//推荐优先使用 ADC1 通道的引脚 32到39引脚都是ADC1

const int sensorPin = 32; // 替换为你实际连接的ADC引脚 (例如 GPIO32)

void setup() {
  Serial.begin(115200);
  pinMode(sensorPin, INPUT);
}

void loop() {
  int sensorValue = analogRead(sensorPin); // 读取 ADC 值 (0-4095)
  Serial.println(sensorValue); // 在串口监视器查看数值
  delay(200);
}