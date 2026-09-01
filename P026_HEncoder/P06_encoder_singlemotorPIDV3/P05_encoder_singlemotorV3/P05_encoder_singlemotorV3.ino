/**
 * 在这个程序中，要结合编码器与drv8333,结合MPU6050;
 * 经验证，该程序可运行，复制后进行升级——20260323；
 * 下面程序可以计算出电机的速度；
 * 复制后补充PID控制；
 */

// ===编码器引脚定义===
#define ENC_A_LEFT  25
#define ENC_B_LEFT  26

// -- 编码器参数 --
#define PPR       7     // 编码器每转脉冲数（电机本体）
#define REDUCTION 210     // 减速比
#define PULSES_PER_REV 1470  // (PPR * REDUCTION) 轮子转一圈的脉冲数 // 这么定义对吗？zq


// ===电机控制参数定义===
#define FREQ    50 //定义频率
#define RESOLUTION 10 // 分辨率10，意思是2的10次方 1024+1 中分辨率，数值越大精度越高，取值在0-20值之间，用来驱动电机；
#define SERVO1_PIN1 32  // PWM输出引脚；
#define SERVO1_PIN2 33  // PWM输出引脚；  


// ===霍尔脉冲全局脉冲计数器===
volatile long leftPulses = 0;
volatile long rightPulses = 0;  // 先不用，但保留
unsigned long lastTime = 0; // 测量转速时，我们需要计算一段时间内的脉冲增量。
long lastPulses = 0; 

// 中断服务函数
void IRAM_ATTR leftEncoderISR() {
  // 通常只需要检测A相上升沿，如需方向可检测B相状态，此处简化
  leftPulses++;
}


// ===================串口字符串数据解析===================
const int MAX_CMD_LEN =64;      // 命令最大长度

char inputBuffer[MAX_CMD_LEN];  // 存储接收到的命令
int bufferIndex = 0;            // 缓冲区当前位置
bool commandReady = false;      // 标志，是否收到完整命令


// =====================PID控制参数====================
// float Kp = 0.6, Ki = 0.3, Kd = 0.05;
float Kp = 0.3, Ki = 0.3, Kd = 0.05;
float target_speed = 60;   // 目标转速 100 RPM
float integral = 0;
float last_error = 0;
float output = 0;  //定义PID输出；

void setup() {
  Serial.begin(115200);
  pinMode(ENC_A_LEFT, INPUT_PULLUP);
  pinMode(ENC_B_LEFT, INPUT_PULLUP);
  
  // 附着中断，上升沿触发
  attachInterrupt(digitalPinToInterrupt(ENC_A_LEFT), leftEncoderISR, RISING);


    //建立 LEDC 通道 臂1
  ledcAttach(SERVO1_PIN1, FREQ, RESOLUTION);    // 通道，频率，分辨率；  ledcSetup() ledcAttachipin() 已经被淘汰了；
  ledcAttach(SERVO1_PIN2, FREQ, RESOLUTION);    // 通道，频率，分辨率；

  lastTime = millis();  
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

  motorSpeedCalculate();
}

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
 * 函数：清空缓冲区并重置状态
 */
void motorSpeedCalculate(){
  unsigned long now = millis(); // 读取微秒；
  if(now - lastTime >= 50){
    // 读取脉冲数（注意关中断保护）
    noInterrupts();
    long pulses = leftPulses;
    interrupts();

    long deltaPulses = pulses - lastPulses;
    float deltaTime = (now - lastTime) / 1000.0;   // 秒
    float speed_rps = deltaPulses*1.0 / PULSES_PER_REV / deltaTime;  // 转/秒  BUG 这个地方有个bug，需要*1.0，负责不会进行计算；
    float speed_rpm = speed_rps * 60;

    Serial.print("Speed: ");
    Serial.print(speed_rpm,4);
    Serial.print(" RPM");   // 转每分钟；

    //PID 计算
    float error = target_speed - speed_rpm;
    integral += error * deltaTime;
    float derivative = (error - last_error) / deltaTime;
    output = Kp * error + Ki * integral + Kd * derivative;

    // 输出限幅
    if (output > 1024) output = 1024;
    if (output < 0) output = 0;

    // 更新上次值
    lastPulses = pulses;
    lastTime = now;
    last_error = error;

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
  
  int supportSpeed1 = output;   // 臂1 的支撑速度  // 分辨率10，意思是2的10次方 1024+1 中分辨率，数值越大精度越高，取值在0-20值之间，用来驱动电机；
  int speed1 = output; s        // 臂1 的驱动速度；
  
  if (strcmp(cmd, "f") == 0) {
    support1(supportSpeed1);
    Serial.println("Robot arm1 is supporting...");
  }  else if (strcmp(cmd, "b") == 0) {
    hug1(supportSpeed1);
    Serial.println("Robot arm1 is hugging");
  } else if (strcmp(cmd, "stop") == 0) {
    stopMotors();
    Serial.println(" Robot stop.");
  } else {
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


void support1(int supportSpeed1){
  ledcWrite(SERVO1_PIN1,0); ledcWrite(SERVO1_PIN2,supportSpeed1);
}

void hug1(int supportSpeed1){  
  ledcWrite(SERVO1_PIN1,supportSpeed1 ); ledcWrite(SERVO1_PIN2,0);
}

void stopMotors(){
  ledcWrite(SERVO1_PIN1,0); ledcWrite(SERVO1_PIN2,0);
}
