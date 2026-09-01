/**
 * 在这个程序中，要结合编码器与drv8333,结合MPU6050;
 * 经验证，该程序可运行，复制后进行升级——20260323；
 * 下面程序可以计算出电机的速度；
 * 复制后补充PID控制；
 * 修正后的程序：PID 实时控制电机速度
 * 通过串口命令启动/停止电机，并实时调节速度
 * PID调通了；
 */

// ===编码器引脚定义===
#define ENC_A_LEFT  25
#define ENC_B_LEFT  26

// -- 编码器参数 --
#define PPR       7        // 编码器每转脉冲数（电机本体）
#define REDUCTION 210      // 减速比
#define PULSES_PER_REV 1470  // (PPR * REDUCTION)

// ===电机控制参数定义===
#define FREQ        50   // PWM 频率（Hz），20kHz 适合电机
#define RESOLUTION  10      // 分辨率 10 位（0~1023）
#define SERVO1_PIN1 32      // 正转 PWM 引脚
#define SERVO1_PIN2 33      // 反转 PWM 引脚

// ===霍尔脉冲全局计数器===
volatile long leftPulses = 0;
unsigned long lastTime = 0;   // 测量转速时，我们需要计算一段时间内的脉冲增量。
long lastPulses = 0;

// 中断服务函数
void IRAM_ATTR leftEncoderISR() {
  leftPulses++;     // 通常只需要检测A相上升沿，如需方向可检测B相状态，此处简化
}

// ===串口命令缓冲区===
const int MAX_CMD_LEN = 64;    // 命令最大长度
char inputBuffer[MAX_CMD_LEN]; // 存储接收到的命令
int bufferIndex = 0;           // 缓冲区当前位置
bool commandReady = false;     // 标志，是否收到完整命令

// ===PID 控制参数===
float Kp = 0.3, Ki = 0.3, Kd = 0.05;
float target_speed = 0;       // 目标转速（RPM），0 表示停止
float integral = 0;
float last_error = 0;
float output = 0;

// ===电机运行标志===
bool motor_running = false;    // 是否执行 PID 控制

void setup() {
  Serial.begin(115200);

  // 编码器初始化
  pinMode(ENC_A_LEFT, INPUT_PULLUP);
  pinMode(ENC_B_LEFT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A_LEFT), leftEncoderISR, RISING); // 附着中断，上升沿触发

  // PWM 初始化（新版 API）
  ledcAttach(SERVO1_PIN1, FREQ, RESOLUTION);
  ledcAttach(SERVO1_PIN2, FREQ, RESOLUTION);

  // 初始停止电机
  stopMotors();

  lastTime = millis();
}

void loop() {
  // 处理串口命令
  // =========第一步，监听串口并收集串口数据============
  receiveSerialCommand();

  // 第二步：如果收到完整命令，则解析并执行
  if (commandReady) {
    parseAndExecuteCommand(inputBuffer);

    // 第三步：重置状态，准备接收下一条命令
    clearBufferAndReset();
  }

  // 如果电机正在运行，则执行 PID 控制并更新 PWM
  if (motor_running) {
    motorSpeedCalculate();   // 内部会更新 PWM 输出
  }
}

// ====================== 电机速度计算与 PID 控制 ======================
void motorSpeedCalculate() {
  unsigned long now = millis();      // 读取毫秒；
  if (now - lastTime < 50) return;   // 每 50ms 计算一次

  // 读取脉冲数（关中断保护）
  noInterrupts();
  long pulses = leftPulses;
  interrupts();

  // 计算速度（RPM）
  long deltaPulses = pulses - lastPulses;
  float deltaTime = (now - lastTime) / 1000.0;  // 秒
  float speed_rps = (deltaPulses * 1.0) / PULSES_PER_REV / deltaTime;  // 转/秒  BUG 这个地方有个bug，需要*1.0，负责不会进行计算；
  float speed_rpm = speed_rps * 60; 

  // PID 计算（仅在目标速度不为 0 时积分）
  float error = target_speed - speed_rpm;
  if (abs(target_speed) > 0) {
    integral += error * deltaTime;
    // 积分限幅，防止过冲
    if (integral > 1000) integral = 1000;
    if (integral < -1000) integral = -1000;
  } else {
    integral = 0;  // 目标速度为 0 时清零积分
  }
  float derivative = (error - last_error) / deltaTime;
  output = Kp * error + Ki * integral + Kd * derivative;

  // 输出限幅到 0~1023（10 位 PWM）
  if (output > 1023) output = 1023;
  if (output < -1023) output = -1023;

  // 根据 output 正负决定电机转向和 PWM 占空比
  int dealOutput = (int)output;
  if (output > 0) {
    setMotorSpeed(dealOutput);
  } else if (output < 0) {
    setMotorSpeed(dealOutput);;
  } else {
    setMotorSpeed(0);
  }

  // 调试信息
  Serial.print("Speed: ");
  Serial.print(speed_rpm, 2);
  Serial.print(" RPM, Target: ");
  Serial.print(target_speed);
  Serial.print(", dealOutput: ");
  Serial.println(dealOutput);

  // 更新状态
  lastPulses = pulses;
  lastTime = now;
  last_error = error;
}

// ====================== 电机控制函数 ======================
void setMotorSpeed(int speed_pwm) {
  // 直接设置 PWM 占空比（不带 PID 时使用）
  if (speed_pwm > 0) {
    
    ledcWrite(SERVO1_PIN1, speed_pwm);    ledcWrite(SERVO1_PIN2, 0);
  } else if (speed_pwm < 0) {
    
    ledcWrite(SERVO1_PIN1, 0);    ledcWrite(SERVO1_PIN2, -speed_pwm);
  } else {
    
    ledcWrite(SERVO1_PIN1, 0);    ledcWrite(SERVO1_PIN2, 0);
  }
}

void stopMotors() {
  setMotorSpeed(0);
  motor_running = false;
  target_speed = 0;
  integral = 0;          // 复位积分
  last_error = 0;
  Serial.println("Motor stopped");
}

// ====================== 串口命令解析 ======================
void receiveSerialCommand() {
  while (Serial.available() > 0 && !commandReady) {
    char c = Serial.read();
    if (c == '\n') {
      inputBuffer[bufferIndex] = '\0';
      commandReady = true;
    } else {
      if (bufferIndex < MAX_CMD_LEN - 1) {
        inputBuffer[bufferIndex] = c;
        bufferIndex++;
      } else {
        Serial.println("Error: Command too long!");
        clearBufferAndReset();
      }
    }
  }
}

void parseAndExecuteCommand(char* cmd) {
  Serial.print("Executing: ");
  Serial.println(cmd);

  // 命令格式： "f" 正转（目标速度为正）; "b" 反转; "s 120" 设置目标速度; "stop" 停止
  if (strcmp(cmd, "stop") == 0) {
    stopMotors();
  }
  else if (strcmp(cmd, "forward") == 0) {
    target_speed = 60;        // 预设正转 60 RPM
    motor_running = true;
    integral = 0;
    last_error = 0;
    Serial.print("Forward, target speed: ");
    Serial.println(target_speed);
  }
  else if (strcmp(cmd, "backward") == 0) {
    target_speed = -60;       // 预设反转 60 RPM
    motor_running = true;
    integral = 0;
    last_error = 0;
    Serial.print("Backward, target speed: ");
    Serial.println(target_speed);
  }
  else if (strncmp(cmd, "s ", 1) == 0) {   // 设置目标速度，如 "s 100"
    int speed = atoi(cmd + 2);  // 跳过 's ' 两个字符
    if (speed != 0) {
      target_speed = speed;
      motor_running = true;
      integral = 0;
      last_error = 0;
      Serial.print("Set target speed: ");
      Serial.println(target_speed);
    } else {
      Serial.println("Invalid speed");
    }
  }
  else {
    Serial.print("Unknown command: '");
    Serial.print(cmd);
    Serial.println("'");
  }
}

void clearBufferAndReset() {
  memset(inputBuffer, 0, MAX_CMD_LEN);
  bufferIndex = 0;
  commandReady = false;
}
