/*
 * This is a test.
 * 
 */
#define FREQ    50 //定义频率
#define RESOLUTION 10 // 分辨率10，意思是2的10次方 1024+1 中分辨率，数值越大精度越高，取值在0-20值之间，用来驱动电机；
#define BT_LED_PIN 12
#define SERVO1_PIN1 18  // PWM输出引脚；
#define SERVO1_PIN2 19  // PWM输出引脚；

#define SERVO2_PIN1 14  // PWM输出引脚；
#define SERVO2_PIN2 23  // PWM输出引脚；

#define SERVO3_PIN1 26 // PWM输出引脚；
#define SERVO3_PIN2 27  // PWM输出引脚；

#define SERVO4_PIN1 32  // PWM输出引脚；
#define SERVO4_PIN2 33  // PWM输出引脚；

void setup() {
  Serial.begin(9600);                                                                                                 
  
  //建立 LEDC 通道
  //ledcAttach(CHANNEL1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  
  //ledcAttach(CHANNEL2, FREQ, RESOLUTION);    // 通道，频率，分辨率；
  int motorPins[] = {SERVO1_PIN1, SERVO1_PIN2,SERVO2_PIN1,SERVO2_PIN2,SERVO3_PIN1,SERVO3_PIN2,SERVO4_PIN1,SERVO4_PIN2,BT_LED_PIN};

  for(int pin:motorPins){
    pinMode(pin, OUTPUT);
  }

}

void loop() {
  digitalWrite(SERVO1_PIN1,HIGH);  digitalWrite(SERVO1_PIN2,LOW);
  digitalWrite(SERVO2_PIN1,HIGH);  digitalWrite(SERVO2_PIN2,LOW);
  digitalWrite(SERVO3_PIN1,HIGH);  digitalWrite(SERVO3_PIN2,LOW);
  digitalWrite(SERVO4_PIN1,HIGH);  digitalWrite(SERVO4_PIN2,LOW);
  digitalWrite(BT_LED_PIN,HIGH);
  delay(3000);

  digitalWrite(SERVO1_PIN1,LOW);  digitalWrite(SERVO1_PIN2,LOW);
  digitalWrite(SERVO2_PIN1,LOW);  digitalWrite(SERVO2_PIN2,LOW);
  digitalWrite(SERVO3_PIN1,LOW);  digitalWrite(SERVO3_PIN2,LOW);
  digitalWrite(SERVO4_PIN1,LOW);  digitalWrite(SERVO4_PIN2,LOW);
  digitalWrite(BT_LED_PIN,LOW);
  delay(3000);

  digitalWrite(SERVO1_PIN1,LOW);  digitalWrite(SERVO1_PIN2,HIGH);
  digitalWrite(SERVO2_PIN1,LOW);  digitalWrite(SERVO2_PIN2,HIGH);
  digitalWrite(SERVO3_PIN1,LOW);  digitalWrite(SERVO3_PIN2,HIGH);
  digitalWrite(SERVO4_PIN1,LOW);  digitalWrite(SERVO4_PIN2,HIGH);
  delay(3000);

  
  digitalWrite(SERVO1_PIN1,LOW);  digitalWrite(SERVO1_PIN2,LOW);
  digitalWrite(SERVO2_PIN1,LOW);  digitalWrite(SERVO2_PIN2,LOW);
  digitalWrite(SERVO3_PIN1,LOW);  digitalWrite(SERVO3_PIN2,LOW);
  digitalWrite(SERVO4_PIN1,LOW);  digitalWrite(SERVO4_PIN2,LOW);
  delay(3000);

}
