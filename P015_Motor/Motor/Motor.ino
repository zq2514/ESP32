/*
  Use two signal to control motors. realse forward and backward;-251211
  After testing, We found red drv8833 board doesn't support two signal to control motors, It need 3 port, PWM,INA1 and INA2;
  Function ledcSetup() and ledcAttachPin() was dismiss.  The new Function is ledcAttach(). The specifications as follows;
  noted in 20251213 by zzq;
*/

#define FREQ    50 //定义频率
#define CHANNEL1 4   //高速通道是0-7，由80hz时钟驱动，低速通道是8-15，由1MHz的始终驱动；
#define CHANNEL2 5
#define RESOLUTION 10 // 分辨率10，意思是2的10次方 1024+1 中分辨率，数值越大精度越高，取值在0-20值之间，用来驱动电机；
#define SERVO1_PIN1 14  // PWM输出引脚；
#define SERVO1_PIN2 27  // PWM输出引脚；
#define SERVO2_PIN1 25  // PWM输出引脚；
#define SERVO2_PIN2 33  // PWM输出引脚；
#define in3  26

void setup() {
  Serial.begin(9600);
  
  //建立 LEDC 通道
  ledcAttach(CHANNEL1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  
  ledcAttach(CHANNEL2, FREQ, RESOLUTION);    // 通道，频率，分辨率；
  
  // 将所有相关引脚设置为输出模式
  int motorPins[] = {SERVO1_PIN1, SERVO1_PIN2, SERVO2_PIN1, SERVO2_PIN2, in3};

  for(int pin:motorPins){
    pinMode(pin, OUTPUT);
  }
  
  digitalWrite(in3,HIGH);  // TB6612 STBY启用，使能输出；
}

void loop(){

  digitalWrite(SERVO1_PIN1,HIGH);  digitalWrite(SERVO1_PIN2,LOW);
  ledcWrite(CHANNEL1, 1024);
  delay(3000);

  digitalWrite(SERVO1_PIN1,LOW);  digitalWrite(SERVO1_PIN2,LOW);
  analogWrite(CHANNEL1, 0);
  delay(3000);

  digitalWrite(SERVO1_PIN1,LOW);  digitalWrite(SERVO1_PIN2,HIGH);
  ledcWrite(CHANNEL1, 500);
  delay(3000);

}
