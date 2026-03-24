/*
 * This is a test.
 * 调试drv8834的时候用的，可行
 */
#define FREQ    50 //定义频率
#define RESOLUTION 10 // 分辨率10，意思是2的10次方 1024+1 中分辨率，数值越大精度越高，取值在0-20值之间，用来驱动电机；
#define BT_LED_PIN 25

#define SERVO1_PIN1 18  // PWM输出引脚；
#define SERVO1_PIN2 19  // PWM输出引脚；

#define SERVO2_PIN1 13  // PWM输出引脚；
#define SERVO2_PIN2 23  // PWM输出引脚；

#define SERVO3_PIN1 26 // PWM输出引脚；
#define SERVO3_PIN2 27  // PWM输出引脚；

#define SERVO4_PIN1 32  // PWM输出引脚；
#define SERVO4_PIN2 33  // PWM输出引脚；

// ===========串口字符串数据解析===================
const int MAX_CMD_LEN =64;      // 命令最大长度

char inputBuffer[MAX_CMD_LEN];  // 存储接收到的命令
int bufferIndex = 0;            // 缓冲区当前位置
bool commandReady = false;      // 标志，是否收到完整命令

void setup() {
                                                                                               
  
  //建立 LEDC 通道
  ledcAttach(SERVO1_PIN1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  ledcAttach(SERVO1_PIN2, FREQ, RESOLUTION);    // 通道，频率，分辨率；
  
  int motorPins[] = {SERVO2_PIN1,SERVO2_PIN2,SERVO3_PIN1,SERVO3_PIN2,SERVO4_PIN1,SERVO4_PIN2,BT_LED_PIN};

  for(int pin:motorPins){
    pinMode(pin, OUTPUT);
  }

  Serial.begin(9600);    
  Serial.println("Serial Ready. Commands: support, hug, forward, backward, stop, on, off");

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
  if (strcmp(cmd, "support") == 0) {
    support(speed);
    Serial.println("Robot is supporting...");
  } 
  else if (strcmp(cmd, "hug") == 0) {
    hug(speed);
    Serial.println("Robot is hugging");
  } 
  else if (strcmp(cmd, "forward") == 0) {
    goForward(speed);
    Serial.println(" Robot is moving forward.");
  } 
  else if (strcmp(cmd, "backward") == 0) {
    goBackward(speed);
    Serial.println(" Robot is moving backward.");
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

void support(int speed){
  ledcWrite(SERVO1_PIN1,speed ); ledcWrite(SERVO1_PIN2,0);  
  digitalWrite(SERVO2_PIN1,HIGH);  digitalWrite(SERVO2_PIN2,LOW);
}

void hug(int speed){
  ledcWrite(SERVO1_PIN1,0); ledcWrite(SERVO1_PIN2,1024);
  digitalWrite(SERVO2_PIN1,LOW);  digitalWrite(SERVO2_PIN2,HIGH);
}

void  goForward(int speed){
  digitalWrite(SERVO3_PIN1,HIGH);  digitalWrite(SERVO3_PIN2,LOW);
  digitalWrite(SERVO4_PIN1,HIGH);  digitalWrite(SERVO4_PIN2,LOW);
}

 void goBackward(int speed){
   digitalWrite(SERVO3_PIN1,LOW);  digitalWrite(SERVO3_PIN2,HIGH);
   digitalWrite(SERVO4_PIN1,LOW);  digitalWrite(SERVO4_PIN2,HIGH);
 }

void stopMotors(){
  ledcWrite(SERVO1_PIN1,0); ledcWrite(SERVO1_PIN2,0);
  digitalWrite(SERVO2_PIN1,LOW);  digitalWrite(SERVO2_PIN2,LOW);
  digitalWrite(SERVO3_PIN1,LOW);  digitalWrite(SERVO3_PIN2,LOW);
  digitalWrite(SERVO4_PIN1,LOW);  digitalWrite(SERVO4_PIN2,LOW);
}


void led_on(){
  digitalWrite(BT_LED_PIN,HIGH);
}

void led_off(){
  digitalWrite(BT_LED_PIN,LOW);
}
