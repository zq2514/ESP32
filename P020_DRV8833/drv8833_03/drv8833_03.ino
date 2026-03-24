/*
 * This is a test.
 * 调试drv8834的时候用的，可行
 */
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

//------臂3-----------
#define ARM3_SUP_PIN1 4 // PWM输出引脚；   臂3支撑
#define ARM3_SUP_PIN2 5  // PWM输出引脚；

#define ARM3_ACT_PIN1 2  // PWM输出引脚；  臂3驱动
#define ARM3_ACT_PIN2 15  // PWM输出引脚；

//------臂4-----------
#define ARM4_SUP_PIN1 14 // PWM输出引脚；   臂3支撑
#define ARM4_SUP_PIN2 12  // PWM输出引脚；

#define ARM4_ACT_PIN1 13  // PWM输出引脚；  臂3驱动
#define ARM4_ACT_PIN2 21  // PWM输出引脚；


// ===========串口字符串数据解析===================
const int MAX_CMD_LEN =64;      // 命令最大长度

char inputBuffer[MAX_CMD_LEN];  // 存储接收到的命令
int bufferIndex = 0;            // 缓冲区当前位置
bool commandReady = false;      // 标志，是否收到完整命令

void setup() {
                                                                                               
  
  //建立 LEDC 通道 臂1
  ledcAttach(SERVO1_PIN1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  ledcAttach(SERVO1_PIN2, FREQ, RESOLUTION);    // 通道，频率，分辨率；

  ledcAttach(SERVO2_PIN1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  ledcAttach(SERVO2_PIN2, FREQ, RESOLUTION);    // 通道，频率，分辨率；
  
  //-------臂2--------
  ledcAttach(SERVO3_PIN1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  ledcAttach(SERVO3_PIN2, FREQ, RESOLUTION);    // 通道，频率，分辨率；

  ledcAttach(SERVO4_PIN1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  ledcAttach(SERVO4_PIN2, FREQ, RESOLUTION);    // 通道，频率，分辨率；

  //-------臂3--------
  ledcAttach(ARM3_SUP_PIN1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  ledcAttach(ARM3_SUP_PIN2, FREQ, RESOLUTION);    // 通道，频率，分辨率；

  ledcAttach(ARM3_ACT_PIN1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  ledcAttach(ARM3_ACT_PIN2, FREQ, RESOLUTION);    // 通道，频率，分辨率；

  //-------臂4--------
  ledcAttach(ARM4_SUP_PIN1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  ledcAttach(ARM4_SUP_PIN2, FREQ, RESOLUTION);    // 通道，频率，分辨率；

  ledcAttach(ARM4_ACT_PIN1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  ledcAttach(ARM4_ACT_PIN2, FREQ, RESOLUTION);    // 通道，频率，分辨率；
  
  
  int motorPins[] = {BT_LED_PIN};

  for(int pin:motorPins){
    pinMode(pin, OUTPUT);
  }

  Serial.begin(9600);    
  Serial.println("Serial Ready. Commands: support1, hug1, forward1, backward1, stop, on, off");

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


/**
 * 函数：解析并执行命令
 * 原理：使用strcmp()比较字符串，匹配则执行对应操作
 * 
 */
void parseAndExecuteCommand(char* cmd) {
  Serial.print("Executing: ");
  Serial.println(cmd); // 回声，显示收到的命令
  
  // 使用 if-else 或 switch-case 进行命令匹配
  int speed = 100;
  int supportSpeed1 = 600;   // 臂1 的支撑速度
  int speed1 = 600;         // 臂1 的驱动速度；

  int supportSpeed2 = 600;   // 臂2 的支撑速度
  int speed2 = 600;         // 臂2 的驱动速度；

  int supportSpeed3 = 600;   // 臂3 的支撑速度
  int speed3 = 600;         // 臂3 的驱动速度；

  int supportSpeed4 = 600;   // 臂4 的支撑速度
  int speed4 = 600;         // 臂4 的驱动速度；
  
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
    Serial.println(" Robot arm1-3 is moving backward.");
  }  
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


/**
 * 函数：清空缓冲区并重置状态
 */
void clearBufferAndReset() {
  // 清空缓冲区（设为全0）
  memset(inputBuffer, 0, MAX_CMD_LEN);
  bufferIndex = 0;
  commandReady = false;
}

//-----------------------臂1-------------------
void support1(int supportSpeed1){
  ledcWrite(SERVO1_PIN1,0); ledcWrite(SERVO1_PIN2,supportSpeed1);
}

void hug1(int supportSpeed1){  
  ledcWrite(SERVO1_PIN1,supportSpeed1 ); ledcWrite(SERVO1_PIN2,0);
}

void  goForward1(int speed1){
  ledcWrite(SERVO2_PIN1,speed1);  ledcWrite(SERVO2_PIN2,0);
}

void goBackward1(int speed1){
  ledcWrite(SERVO2_PIN1,0);  ledcWrite(SERVO2_PIN2,speed1);
}

//-----------------------臂2-------------------
void support2(int supportSpeed2){
  ledcWrite(SERVO3_PIN1,0); ledcWrite(SERVO3_PIN2,supportSpeed2);
}

void hug2(int supportSpeed2){  
  ledcWrite(SERVO3_PIN1,supportSpeed2 ); ledcWrite(SERVO3_PIN2,0);
}

void  goForward2(int speed2){
  ledcWrite(SERVO4_PIN1,speed2);  ledcWrite(SERVO4_PIN2,0);
}

void goBackward2(int speed2){
  ledcWrite(SERVO4_PIN1,0);  ledcWrite(SERVO4_PIN2,speed2);
}

//-----------------------臂3-------------------
void support3(int supportSpeed3){
  ledcWrite(ARM3_SUP_PIN1,0); ledcWrite(ARM3_SUP_PIN2,supportSpeed3);
}

void hug3(int supportSpeed3){  
  ledcWrite(ARM3_SUP_PIN1,supportSpeed3); ledcWrite(ARM3_SUP_PIN2,0);
}

void  goForward3(int speed3){
  ledcWrite(ARM3_ACT_PIN1,speed3);  ledcWrite(ARM3_ACT_PIN2,0);
}

void goBackward3(int speed3){
  ledcWrite(ARM3_ACT_PIN1,0);  ledcWrite(ARM3_ACT_PIN2,speed3);
}
//-----------------------臂4-------------------
void support4(int supportSpeed4){
  ledcWrite(ARM4_SUP_PIN1,0); ledcWrite(ARM4_SUP_PIN2,supportSpeed4);
}

void hug4(int supportSpeed4){  
  ledcWrite(ARM4_SUP_PIN1,supportSpeed4); ledcWrite(ARM4_SUP_PIN2,0);
}

void  goForward4(int speed4){
  ledcWrite(ARM4_ACT_PIN1,speed4);  ledcWrite(ARM4_ACT_PIN2,0);
}

void goBackward4(int speed4){
  ledcWrite(ARM4_ACT_PIN1,0);  ledcWrite(ARM4_ACT_PIN2,speed4);
}

void stopMotors(){
  ledcWrite(SERVO1_PIN1,0); ledcWrite(SERVO1_PIN2,0);
  ledcWrite(SERVO2_PIN1,0); ledcWrite(SERVO2_PIN2,0);
  ledcWrite(SERVO3_PIN1,0); ledcWrite(SERVO3_PIN2,0);
  ledcWrite(SERVO4_PIN1,0); ledcWrite(SERVO4_PIN2,0);
  ledcWrite(ARM3_SUP_PIN1,0); ledcWrite(ARM3_SUP_PIN2,0); 
  ledcWrite(ARM3_ACT_PIN1,0); ledcWrite(ARM3_ACT_PIN2,0);
  ledcWrite(ARM4_SUP_PIN1,0); ledcWrite(ARM4_SUP_PIN2,0);
  ledcWrite(ARM4_ACT_PIN1,0); ledcWrite(ARM4_ACT_PIN2,0);
}

void led_on(){
  digitalWrite(BT_LED_PIN,HIGH);
}

void led_off(){
  digitalWrite(BT_LED_PIN,LOW);
}
