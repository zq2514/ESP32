#include <Arduino.h>

/*
 * This is a project about climb pipeline robot.
 * 
 */
 //-------GPIO扩展板 I2C 输入----------
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
 
#define FREQ    50 //定义频率
#define RESOLUTION 10 // 分辨率10，意思是2的10次方 1024+1 中分辨率，数值越大精度越高，取值在0-20值之间，用来驱动电机；
#define BT_LED_PIN 25

//------臂1-----------
#define SERVO1_PIN1 18  // PWM输出引脚； 臂1支撑
#define SERVO1_PIN2 19  // PWM输出引脚；  

#define SERVO2_PIN1 13  // PWM输出引脚； 臂1驱动
#define SERVO2_PIN2 23  // PWM输出引脚；  

//------臂2-----------
#define SERVO3_PIN1 26 // PWM输出引脚；   臂2支撑
#define SERVO3_PIN2 27  // PWM输出引脚；

#define SERVO4_PIN1 32  // PWM输出引脚；  臂2驱动
#define SERVO4_PIN2 33  // PWM输出引脚；

//------------这里的数字代表的是GPIO上的编号，不是ESP32的引脚------
const int ARM1_SUP_PIN1 = 2;
const int ARM1_SUP_PIN2 = 3;
const int ARM1_ACT_PIN1 = 0;
const int ARM1_ACT_PIN2 = 1;

const int ARM2_SUP_PIN1 = 6;
const int ARM2_SUP_PIN2 = 7;
const int ARM2_ACT_PIN1 = 5;
const int ARM2_ACT_PIN2 = 4;

const int ARM3_SUP_PIN1 = 10;
const int ARM3_SUP_PIN2 = 11;
const int ARM3_ACT_PIN1 = 9;
const int ARM3_ACT_PIN2 = 8;

const int ARM4_SUP_PIN1 = 13;
const int ARM4_SUP_PIN2 = 12;
const int ARM4_ACT_PIN1 = 15;
const int ARM4_ACT_PIN2 = 14;
//-------------------------------
const int ARM5_SUP_PIN1 = 1;
const int ARM5_SUP_PIN2 = 0;
const int ARM5_ACT_PIN1 = 3;
const int ARM5_ACT_PIN2 = 2;

const int ARM6_SUP_PIN1 = 4;
const int ARM6_SUP_PIN2 = 5;
const int ARM6_ACT_PIN1 = 6;
const int ARM6_ACT_PIN2 = 7;


//------臂4---------------- 
//------臂4使用扩展IO输出----
// called this way, it uses the default address 0x40
// define GPIO board1,address, 
// if you want add another one, you can named board2 
// 实例化PCA9686对象。调用Adafruit_PWMServoDriver(0x40)构造函数；
Adafruit_PWMServoDriver board1 = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver board2 = Adafruit_PWMServoDriver(0x41);
#define SERVOMIN  125 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  575 // this is the 'maximum' pulse length count (out of 4096)



// ===========串口字符串数据解析===================
const int MAX_CMD_LEN =64;      // 命令最大长度

char inputBuffer[MAX_CMD_LEN];  // 存储接收到的命令
int bufferIndex = 0;            // 缓冲区当前位置
bool commandReady = false;      // 标志，是否收到完整命令



/**
 * 函数：清空缓冲区并重置状态
 */
void clearBufferAndReset() {
  // 清空缓冲区（设为全0）
  memset(inputBuffer, 0, MAX_CMD_LEN);
  bufferIndex = 0;
  commandReady = false;
}


// ==================== 核心函数定义 ====================

/**
 * 函数：接收串口命令
 * 原理：逐个读取字符，以换行符'\n'作为命令结束标志
 */
void receiveSerialCommand() {
  while (Serial.available() > 0 && !commandReady) {
    char incomingChar = Serial.read(); // 读取一个字符
    
    // 判断是否为命令结束符（也可以是'\r'回车符）
    if (incomingChar == '\n') {
      inputBuffer[bufferIndex] = '\0'; // 在字符串末尾添加终止符
      commandReady = true;             // 标志命令已就绪
    } 
    else {
      // 将字符存入缓冲区，但防止溢出
      if (bufferIndex < MAX_CMD_LEN - 1) {
        inputBuffer[bufferIndex] = incomingChar;
        bufferIndex++;
      } else {
        // 缓冲区溢出，清空并提示错误
        Serial.println("Error: Command too long!");
        clearBufferAndReset();
      }
    }
  }
}





//-----------------------臂1-------------------
void support1(int supportSpeed1){
  board1.setPWM(ARM1_SUP_PIN1, 0, supportSpeed1 );  board1.setPWM(ARM1_SUP_PIN2, 0, 0);
}

void hug1(int supportSpeed1){  
  board1.setPWM(ARM1_SUP_PIN1, 0, 0 );  board1.setPWM(ARM1_SUP_PIN2, 0, supportSpeed1);
}

void  goForward1(int speed1){
  board1.setPWM(ARM1_ACT_PIN1, 0, speed1 );  board1.setPWM(ARM1_ACT_PIN2, 0, 0);
}

void goBackward1(int speed1){
  board1.setPWM(ARM1_ACT_PIN1, 0, 0 );  board1.setPWM(ARM1_ACT_PIN2, 0, speed1);
}

//-----------------------臂2-------------------
void support2(int supportSpeed2){
  board1.setPWM(ARM2_SUP_PIN1, 0, supportSpeed2 );  board1.setPWM(ARM2_SUP_PIN2, 0, 0);
}

void hug2(int supportSpeed2){  
  board1.setPWM(ARM2_SUP_PIN1, 0, 0 );  board1.setPWM(ARM2_SUP_PIN2, 0, supportSpeed2);
}

void  goForward2(int speed2){
  board1.setPWM(ARM2_ACT_PIN1, 0, speed2 );  board1.setPWM(ARM2_ACT_PIN2, 0, 0);
}

void goBackward2(int speed2){
  board1.setPWM(ARM2_ACT_PIN1, 0, 0 );  board1.setPWM(ARM2_ACT_PIN2, 0, speed2);
}

//-----------------------臂3-------------------
void support3(int supportSpeed3){
  board1.setPWM(ARM3_SUP_PIN1, 0, supportSpeed3 );  board1.setPWM(ARM3_SUP_PIN2, 0, 0);
}

void hug3(int supportSpeed3){  
  board1.setPWM(ARM3_SUP_PIN1, 0, 0 );  board1.setPWM(ARM3_SUP_PIN2, 0, supportSpeed3);
}

void  goForward3(int speed3){
  board1.setPWM(ARM3_ACT_PIN1, 0, speed3 );  board1.setPWM(ARM3_ACT_PIN2, 0, 0);
}

void goBackward3(int speed3){
  board1.setPWM(ARM3_ACT_PIN1, 0, 0 );  board1.setPWM(ARM3_ACT_PIN2, 0, speed3);
}
//-----------------------臂4-------------------
void support4(int supportSpeed4){
  board1.setPWM(ARM4_SUP_PIN1, 0, supportSpeed4 );  board1.setPWM(ARM4_SUP_PIN2, 0, 0);
}

void hug4(int supportSpeed4){  
  board1.setPWM(ARM4_SUP_PIN1, 0, 0 );  board1.setPWM(ARM4_SUP_PIN2, 0, supportSpeed4);
}

void  goForward4(int speed4){
  board1.setPWM(ARM4_ACT_PIN1, 0, speed4 );  board1.setPWM(ARM4_ACT_PIN2, 0, 0);
}
void goBackward4(int speed4){
  board1.setPWM(ARM4_ACT_PIN1, 0, 0 );  board1.setPWM(ARM4_ACT_PIN2, 0, speed4);
}
//-----------------------臂5-------------------
void support5(int supportSpeed5){
  board2.setPWM(ARM5_SUP_PIN1, 0, supportSpeed5 );  board2.setPWM(ARM5_SUP_PIN2, 0, 0);
}

void hug5(int supportSpeed5){  
  board2.setPWM(ARM5_SUP_PIN1, 0, 0 );  board2.setPWM(ARM5_SUP_PIN2, 0, supportSpeed5);
}

void  goForward5(int speed5){
  board2.setPWM(ARM5_ACT_PIN1, 0, speed5);  board2.setPWM(ARM5_ACT_PIN2, 0, 0);
}
void goBackward5(int speed5){
  board2.setPWM(ARM5_ACT_PIN1, 0, 0 );  board2.setPWM(ARM5_ACT_PIN2, 0, speed5);
}
//-----------------------臂6-------------------
void support6(int supportSpeed6){
  board2.setPWM(ARM6_SUP_PIN1, 0, supportSpeed6 );  board2.setPWM(ARM6_SUP_PIN2, 0, 0);
}

void hug6(int supportSpeed6){  
  board2.setPWM(ARM6_SUP_PIN1, 0, 0 );  board2.setPWM(ARM6_SUP_PIN2, 0, supportSpeed6);
}

void  goForward6(int speed6){
  board2.setPWM(ARM6_ACT_PIN1, 0, speed6 );  board2.setPWM(ARM6_ACT_PIN2, 0, 0);
}
void goBackward6(int speed6){
  board2.setPWM(ARM6_ACT_PIN1, 0, 0 );  board2.setPWM(ARM6_ACT_PIN2, 0, speed6);
}
//------------------------stop---------------------
void stopMotors(){
  board1.setPWM(ARM1_SUP_PIN1, 0, 0);  board1.setPWM(ARM1_SUP_PIN2, 0, 0);
  board1.setPWM(ARM1_ACT_PIN1, 0, 0);  board1.setPWM(ARM1_ACT_PIN2, 0, 0);

  board1.setPWM(ARM2_SUP_PIN1, 0, 0);  board1.setPWM(ARM2_SUP_PIN2, 0, 0);
  board1.setPWM(ARM2_ACT_PIN1, 0, 0);  board1.setPWM(ARM2_ACT_PIN2, 0, 0);
 
  board1.setPWM(ARM3_SUP_PIN1, 0, 0 );  board1.setPWM(ARM3_SUP_PIN2, 0, 0);
  board1.setPWM(ARM3_ACT_PIN1, 0, 0 );  board1.setPWM(ARM3_ACT_PIN2, 0, 0);

  board1.setPWM(ARM4_SUP_PIN1, 0, 0 );  board1.setPWM(ARM4_SUP_PIN2, 0, 0);
  board1.setPWM(ARM4_ACT_PIN1, 0, 0 );  board1.setPWM(ARM4_ACT_PIN2, 0, 0);

  board2.setPWM(ARM5_SUP_PIN1, 0, 0 );  board2.setPWM(ARM5_SUP_PIN2, 0, 0);
  board2.setPWM(ARM5_ACT_PIN1, 0, 0 );  board2.setPWM(ARM5_ACT_PIN2, 0, 0);

  board2.setPWM(ARM6_SUP_PIN1, 0, 0 );  board2.setPWM(ARM6_SUP_PIN2, 0, 0);
  board2.setPWM(ARM6_ACT_PIN1, 0, 0 );  board2.setPWM(ARM6_ACT_PIN2, 0, 0);
}

void led_on(){
  digitalWrite(BT_LED_PIN,HIGH);
}

void led_off(){
  digitalWrite(BT_LED_PIN,LOW);
}



/**
 * 函数：解析并执行命令
 * 原理：使用strcmp()比较字符串，匹配则执行对应操作
 * 
 */
void parseAndExecuteCommand(char* cmd) {

  // 去除末尾的换行符和回车符
  int len = strlen(cmd);
  while (len > 0 && (cmd[len-1] == '\n' || cmd[len-1] == '\r')) {
      cmd[len-1] = '\0';
      len--;
  }

  Serial.print("Executing: ");
  Serial.println(cmd); // 回声，显示收到的命令
  
  // 使用 if-else 或 switch-case 进行命令匹配
  int speed = 100;
  int supportSpeed1 = 1200;   // 臂1 的支撑速度
  int speed1 = 800;           // 臂1 的驱动速度；

  int supportSpeed2 = 1200;   // 臂2 的支撑速度
  int speed2 = 800;           // 臂2 的驱动速度；

  int supportSpeed3 = 1200;   // 臂3 的支撑速度
  int speed3 = 800;           // 臂3 的驱动速度；

  int supportSpeed4 = 1200;   // 臂4 的支撑速度
  int speed4 = 800;           // 臂4 的驱动速度；

  int supportSpeed5 = 1200;   // 臂5 的支撑速度
  int speed5 = 800;           // 臂5 的驱动速度；

  int supportSpeed6 = 1200;   // 臂6 的支撑速度
  int speed6 = 800;           // 臂6 的驱动速度；
  
  if (strcmp(cmd, "support1") == 0) {
    support1(supportSpeed1);
    Serial.println("Robot arm1 is supporting...");
  } 
  else if (strcmp(cmd, "hug1") == 0) {
    hug1(supportSpeed1);
    Serial.println("Robot arm1 is hugging");
  } 
  else if (strcmp(cmd, "forward1") == 0) {
    goForward1(speed1);
    Serial.println(" Robot arm1 is moving forward.");
  } 
  else if (strcmp(cmd, "backward1") == 0) {
    goBackward1(speed1);
    Serial.println(" Robot arm1 is moving backward.");
  }  
  // ----------------臂2-----------------------
  else if (strcmp(cmd, "support2") == 0) {
    support2(supportSpeed2);
    Serial.println("Robot arm2 is supporting...");
  } 
  else if (strcmp(cmd, "hug2") == 0) {
    hug2(supportSpeed2);
    Serial.println("Robot arm2 is hugging");
  } 
  else if (strcmp(cmd, "forward2") == 0) {
    goForward2(speed2);
    Serial.println(" Robot arm2 is moving forward.");
  } 
  else if (strcmp(cmd, "backward2") == 0) {
    goBackward2(speed2);
    Serial.println(" Robot arm2 is moving backward.");
  }
  //--------------臂3--------------
  else if (strcmp(cmd, "support3") == 0) {
    support3(supportSpeed3);
    Serial.println("Robot arm3 is supporting...");
  } 
  else if (strcmp(cmd, "hug3") == 0) {
    hug3(supportSpeed3);
    Serial.println("Robot arm3 is hugging");
  } 
  else if (strcmp(cmd, "forward3") == 0) {
    goForward3(speed3);
    Serial.println(" Robot arm3 is moving forward.");
  } 
  else if (strcmp(cmd, "backward3") == 0) {
    goBackward3(speed3);
    Serial.println(" Robot arm3 is moving backward.");
  }
  //--------------臂4--------------
  else if (strcmp(cmd, "support4") == 0) {
    support4(supportSpeed4);
    Serial.println("Robot arm4 is supporting...");
  } 
  else if (strcmp(cmd, "hug4") == 0) {
    hug4(supportSpeed4);
    Serial.println("Robot arm4 is hugging");
  } 
  else if (strcmp(cmd, "forward4") == 0) {
    goForward4(speed4);
    Serial.println(" Robot arm4 is moving forward.");
  } 
  else if (strcmp(cmd, "backward4") == 0) {
    goBackward4(speed4);
    Serial.println(" Robot arm4 is moving backward.");
  }
    //--------------臂5--------------
  else if (strcmp(cmd, "support5") == 0) {
    support5(supportSpeed5);
    Serial.println("Robot arm5 is supporting...");
  } 
  else if (strcmp(cmd, "hug5") == 0) {
    hug5(supportSpeed5);
    Serial.println("Robot arm5 is hugging");
  } 
  else if (strcmp(cmd, "forward5") == 0) {
    goForward5(speed5);
    Serial.println(" Robot arm5 is moving forward.");
  } 
  else if (strcmp(cmd, "backward5") == 0) {
    goBackward5(speed5);
    Serial.println(" Robot arm5 is moving backward.");
  }
  //--------------臂6--------------
  else if (strcmp(cmd, "support6") == 0) {
    support6(supportSpeed6);
    Serial.println("Robot arm6 is supporting...");
  } 
  else if (strcmp(cmd, "hug6") == 0) {
    hug6(supportSpeed6);
    Serial.println("Robot arm6 is hugging");
  } 
  else if (strcmp(cmd, "forward6") == 0) {
    goForward6(speed6);
    Serial.println(" Robot arm6 is moving forward.");
  } 
  else if (strcmp(cmd, "backward6") == 0) {
    goBackward6(speed6);
    Serial.println(" Robot arm6 is moving backward.");
  }
  //----------Half_Control----------------
  else if (strcmp(cmd, "support13") == 0) {
    support1(supportSpeed1);
    support2(supportSpeed2);
    support3(supportSpeed3);
    Serial.println("Robot arm1-3 is supporting...");
  } 
  else if (strcmp(cmd, "hug13") == 0) {
    hug1(supportSpeed1);
    hug2(supportSpeed2);
    hug3(supportSpeed3);
    Serial.println("Robot arm1-3 is hugging");
  } 
  else if (strcmp(cmd, "forward13") == 0) {
    goForward1(speed1);
    goForward2(speed2);
    goForward3(speed3);
    Serial.println(" Robot arm1-3 is moving forward.");
  } 
  else if (strcmp(cmd, "backward13") == 0) {
    goBackward1(speed1);
    goBackward2(speed2);
    goBackward3(speed3);
    Serial.println(" Robot arm4-6 is moving backward.");
  } 
  //----------Half_Control_Other----------------
  else if (strcmp(cmd, "support46") == 0) {
    support4(supportSpeed4);
    support5(supportSpeed5);
    support6(supportSpeed6);
    Serial.println("Robot arm4-6 is supporting...");
  } 
  else if (strcmp(cmd, "hug46") == 0) {
    hug4(supportSpeed4);
    hug5(supportSpeed5);
    hug6(supportSpeed6);
    Serial.println("Robot arm4-6 is hugging");
  } 
  else if (strcmp(cmd, "forward46") == 0) {
    goForward4(speed4);
    goForward5(speed5);
    goForward6(speed6);
    Serial.println(" Robot arm4-6 is moving forward.");
  } 
  else if (strcmp(cmd, "backward46") == 0) {
    goBackward4(speed4);
    goBackward5(speed5);
    goBackward6(speed6);
    Serial.println(" Robot arm4-6 is moving backward.");
  }  
  //----------All_Control_Other----------------
  else if (strcmp(cmd, "support16") == 0) {
    support1(supportSpeed1);
    support2(supportSpeed2);
    support3(supportSpeed3);
    support4(supportSpeed4);
    support5(supportSpeed5);
    support6(supportSpeed6);
    Serial.println("Robot arm1-6 is supporting...");
  } 
  else if (strcmp(cmd, "hug16") == 0) {
    hug1(supportSpeed1);
    hug2(supportSpeed2);
    hug3(supportSpeed3);
    hug4(supportSpeed4);
    hug5(supportSpeed5);
    hug6(supportSpeed6);
    Serial.println("Robot arm1-6 is hugging");
  } 
  else if (strcmp(cmd, "forward16") == 0) {
    goForward1(speed1);
    goForward2(speed2);
    goForward3(speed3);
    goForward4(speed4);
    goForward5(speed5);
    goForward6(speed6);
    Serial.println(" Robot arm1-6 is moving forward.");
  } 
  else if (strcmp(cmd, "backward16") == 0) {
    goBackward1(speed1);
    goBackward2(speed2);
    goBackward3(speed3);
    goBackward4(speed4);
    goBackward5(speed5);
    goBackward6(speed6);
    Serial.println(" Robot arm1-6 is moving backward.");
  }  
  //------------stop------------
  else if (strcmp(cmd, "stop") == 0) {
    stopMotors();
    Serial.println(" Robot stop.");
  } 
  else if (strcmp(cmd, "on") == 0) {
    led_on();
    Serial.println(" LED ON.");
  } 
  else if (strcmp(cmd, "off") == 0) {
    led_off();
    Serial.println(" LED OFF.");
  } 
  else {
    Serial.print("Unknown command: '");
    Serial.print(cmd);
    Serial.println("'");
  }
}



void setup() {  
  
  int motorPins[] = {BT_LED_PIN};

  for(int pin:motorPins){
    pinMode(pin, OUTPUT);
  }

  Serial.begin(115200);    
  Serial.println("This is pipeline robot project!"); 
  Serial.println("Serial Ready. Commands: support1, hug1, forward1, backward1, stop, on, off");
  board1.begin();
  board2.begin();
  board1.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
  board2.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
}


void loop() {

  // =========第一步，监听串口并收集串口数据============
    receiveSerialCommand();
  
  // 第二步：如果收到完整命令，则解析并执行
  if (commandReady) {
    parseAndExecuteCommand(inputBuffer);
    
    // 第三步：重置状态，准备接收下一条命令
    clearBufferAndReset();
  }
}
