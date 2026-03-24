#include "BluetoothSerial.h"

BluetoothSerial BT;

char incoming_value = 0;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  BT.begin("ESP32_Device");
  pinMode(12,OUTPUT);

}

void loop() {
  // put your main code here, to run repeatedly:
  if(BT.available()){
    incoming_value = BT.read();
    Serial.println(incoming_value);
    if(incoming_value == '1'){
      digitalWrite(12,HIGH);
    }
    else if(incoming_value == '0'){
      digitalWrite(12,LOW);
    }
  }

}
