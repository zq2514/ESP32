#include "BluetoothSerial.h"

#define in1  14
#define in2  27
#define in3  26
#define ena  4
#define in25  25
#define in33  33
#define enb  5
#define BT_LED_PIN 12

BluetoothSerial BT;

void handleBluetoothCommand(char cmd);
void support(int speed);
void huggingAction(int speed);
void goForward(int speed);
void goBackward(int speed);
void stopMotors();
void bluetoothOpenLED();
void bluetoothCloseLED();



int myPMW = 200;
int value = 0;

// 储存蓝牙收到的指令
char bluetoothCommand = 0;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  BT.begin("ESP32_Device");

  // 将所有相关引脚设置为输出模式
  int motorPins[] = {in1, in2, in3, ena,in25, in33, enb,BT_LED_PIN};

  for(int pin:motorPins){
    pinMode(pin, OUTPUT);
  }

  digitalWrite(in3,HIGH);  // TB6612 STBY启用，使能输出；
}

void loop() {
  // 随时检查蓝牙串口是否有新指令
  if(BT.available()){
    bluetoothCommand = BT.read();
    handleBluetoothCommand(bluetoothCommand);
  }

}

// =============================================================
// ==                 蓝牙指令处理函数                        ==
// =============================================================

void handleBluetoothCommand(char cmd){
  Serial.print("BT Command: ");Serial.println(cmd);

  int speed = 100;  // 遥控模式下PWM速度
  switch (cmd){
    case 'O' : bluetoothOpenLED(); break;
    case 'C' : bluetoothCloseLED(); break;
    case 'S' : support(speed); break;
    case 'H' : huggingAction(speed); break;
    case 'F' : goForward(speed); break;
    case 'B' : goBackward(speed); break;
    case 'P' : stopMotors(); break;
  }
  
}

// =============================================================
// ==          电机底层驱动函数             ==
// =============================================================

void setMotorPWM(int supportPwm, int actuatorPwm){
  const int minPwm = 80;  //电机最小启动PWM值；

  //---处理支撑臂电机----
  int acturalSupportPwm = abs(supportPwm);
  // 如果PWM值太小，直接置为0
  if (acturalSupportPwm < minPwm){
    acturalSupportPwm = 0;
    Serial.println("实际PWM值小于支撑电机最小启动PWM，PWM置为0");
  }

  if (supportPwm > minPwm){
    digitalWrite(in1,HIGH);  digitalWrite(in2,LOW);
  }else if(supportPwm < -minPwm){
    digitalWrite(in1,LOW );  digitalWrite(in2,HIGH);
  }else {
    digitalWrite(in1,LOW);  digitalWrite(in2,LOW);
  }
  analogWrite(ena,constrain(acturalSupportPwm, 0, 225));


  //---处理驱动电机----
  int actualActuatorPwm = abs(actuatorPwm);
  // 如果PWM值太小，直接置为0
  if (actualActuatorPwm < minPwm){
    actualActuatorPwm = 0;
    Serial.println("实际PWM值小于驱动电机最小启动PWM，PWM置为0");
  }
  
  if (actuatorPwm > minPwm){
  digitalWrite(in25,HIGH);  digitalWrite(in33,LOW);
  }else if(actuatorPwm < -minPwm){
    digitalWrite(in25,LOW );  digitalWrite(in33,HIGH);
  }else {
    digitalWrite(in25,LOW);  digitalWrite(in33,LOW);
  }
  analogWrite(enb,constrain(actualActuatorPwm, 0, 225));
}

// 支撑动作；
// setMotorPWM(支撑速度，移动速度)
void support(int speed){ 
  setMotorPWM(speed,0);
}

// 抱合动作
void huggingAction(int speed){
  setMotorPWM(-speed,0);
}

void goForward(int speed){ // 前进
  setMotorPWM(0,speed);
}

void goBackward(int speed){ // 后退
  setMotorPWM(0,-speed);
}

void stopMotors(){ // 停止；
  setMotorPWM(0,0);
}

//------------------------------------------
//----         蓝牙测试指示灯      --------
//------------------------------------------

void bluetoothOpenLED(){
  digitalWrite(BT_LED_PIN,HIGH);
}

void bluetoothCloseLED(){
  digitalWrite(BT_LED_PIN,LOW);
}
