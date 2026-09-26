/*
测试程序的目的：教学助教内容涉及直流电机的PID控制，之前没有动手做过尝试制作；


硬件包含： 带霍尔编码器的N20直流减速电机；
          ESP32;
          DRV8833 红色板，带Standby引脚；
          
基础程序，实现了电机的正传反转，停止，和编码器控制：现象： F30,F50 F100电机以不同的速度转动； 理论上，F100对应 75r/min

速度计算程序；

加入速度控制PID控制；

*/

#include <Arduino.h>
#include <driver/pcnt.h>

// ==================== 引脚定义 ====================
#define AIN1_PIN 18   // DRV8833 AIN1
#define AIN2_PIN 19   // DRV8833 AIN2
#define STANDBY  13   // DRV8833 standby   
#define ENC_A_PIN 25  // 霍尔编码器 A 相
#define ENC_B_PIN 26  // 霍尔编码器 B 相

// ==================== PWM 配置 ====================
#define PWM_FREQ 5000      // 5kHz
#define PWM_RESOLUTION 8   // 8位分辨率 (0~255)
#define PWM_CHANNEL_1 0    // AIN1 使用通道 0
#define PWM_CHANNEL_2 1    // AIN2 使用通道 1


// ==================== 编码器参数 ====================
// 需要根据你的电机实际参数修改！
// 计算方式：编码器每转脉冲数(PPR) × 4(4倍频) × 减速比
// 例如：PPR=13，减速比=20，则 CPR = 13 × 4 × 20 = 1040
// 实际计算： PPR = 7  则 CPR = 7 x 2 x 210 = 5880 （目前应该是2倍频）
// CPR 定义电机输出轴转一圈对应的脉冲总数，转速计算时要用到。
#define ENCODER_CPR 2940 

// ==================== 默认速度 ====================
#define DEFAULT_SPEED_PERCENT 50   // 默认速度 50%（范围 0~100）

// ==================== PCNT 配置 ====================
#define PCNT_UNIT PCNT_UNIT_0

int16_t lastCount = 0;
int64_t totalPulses = 0;

// 转速计算相关
int64_t lastPulsesForRPM = 0;      // 上次计算转速时的脉冲总数
unsigned long lastRPMTime = 0;      // 上次计算转速的时间
float currentRPM = 0.0;             // 当前转速


// ==================== PID 参数 ====================
float Kp = 2.0;             // 比例系数
float Ki = 1.0;             // 积分系数
float Kd = 0.0;             // 微分系数（先设为0）

// PID 状态变量
float targetRPM = 0.0;      // 目标转速
float pidOutput = 0.0;      // PID 输出（PWM值）
float integral = 0.0;       // 积分累积
float lastError = 0.0;      // 上次误差
unsigned long lastPIDTime = 0;
bool pidEnabled = false;    // PID 是否启用


// ==================== 函数声明 ====================
void initMotor();
void initEncoder();
void updateEncoder();
void motorForward(int speedPercent);
void motorReverse(int speedPercent);
void motorStop();
void updateRPM(unsigned long samplePeriodMs);
float getRPM();
void processSerialCommand();
void updatePID(unsigned long samplePeriodMs);
void resetPID();

// ==================== 速度转换 ====================
// 将百分比（0~100）转换为 PWM 值（0~255）
int percentToPWM(int percent) {
  percent = constrain(percent, 0, 100);
  return map(percent, 0, 100, 0, 255);
}

// ==================== 电机初始化 ====================
void initMotor() {
  ledcSetup(PWM_CHANNEL_1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(AIN1_PIN, PWM_CHANNEL_1);
  
  ledcSetup(PWM_CHANNEL_2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(AIN2_PIN, PWM_CHANNEL_2);
  
  motorStop();
}

// ==================== 电机控制函数 ====================
// 正转，参数为速度百分比 0~100
void motorForward(int speedPercent) {
  int pwmValue = percentToPWM(speedPercent);
  ledcWrite(PWM_CHANNEL_1, pwmValue);  // AIN1 输出 PWM
  ledcWrite(PWM_CHANNEL_2, 0);         // AIN2 输出低电平
  Serial.printf("Motor forward: %d%% (PWM=%d)\n", speedPercent, pwmValue);
  Serial.println();
}

// 反转，参数为速度百分比 0~100
void motorReverse(int speedPercent) {
  int pwmValue = percentToPWM(speedPercent);
  ledcWrite(PWM_CHANNEL_1, 0);         // AIN1 输出低电平
  ledcWrite(PWM_CHANNEL_2, pwmValue);  // AIN2 输出 PWM
  Serial.printf("Motor Reverse, Speed: %d%% (PWM=%d)\n", speedPercent, pwmValue);
}

// 停止（刹车）
void motorStop() {
  ledcWrite(PWM_CHANNEL_1, 255);
  ledcWrite(PWM_CHANNEL_2, 255);
  pidEnabled = false;      // 新增：停止时关闭 PID
  resetPID();              // 新增：复位 PID 状态
  Serial.println("Motor stop!");
}

// ==================== 编码器初始化 ====================
void initEncoder() {
  pcnt_config_t pcnt_config = {
    .pulse_gpio_num = ENC_A_PIN,
    .ctrl_gpio_num = ENC_B_PIN,
    .lctrl_mode = PCNT_MODE_REVERSE,
    .hctrl_mode = PCNT_MODE_KEEP,
    .pos_mode = PCNT_COUNT_INC,
    .neg_mode = PCNT_COUNT_DEC,
    .counter_h_lim = 32767,
    .counter_l_lim = -32768,
    .unit = PCNT_UNIT,
    .channel = PCNT_CHANNEL_0,
  };
  pcnt_unit_config(&pcnt_config);
  
  pcnt_counter_pause(PCNT_UNIT);
  pcnt_counter_clear(PCNT_UNIT);
  pcnt_counter_resume(PCNT_UNIT);
  
  pcnt_get_counter_value(PCNT_UNIT, &lastCount);
}

// 读取并累加编码器脉冲
void updateEncoder() {
  int16_t curCount;
  pcnt_get_counter_value(PCNT_UNIT, &curCount);
  int16_t delta = curCount - lastCount;
  totalPulses += delta;
  lastCount = curCount;
}

// 计算转速
void updateRPM(unsigned long samplePeriodMs) {
  unsigned long now = millis();
  
  if (now - lastRPMTime >= samplePeriodMs) {
    // 计算本次采样周期内的脉冲增量
    int64_t pulseDelta = totalPulses - lastPulsesForRPM;
    
    // 计算实际经过的时间（毫秒）
    unsigned long elapsed = now - lastRPMTime;
    
    // 计算转速：RPM = (脉冲增量 / CPR) × (60000 / 时间ms)
    currentRPM = (pulseDelta * 60000.0) / (ENCODER_CPR * elapsed);
    
    // 更新记录
    lastPulsesForRPM = totalPulses;
    lastRPMTime = now;
  }
}

// 获取转速：
float getRPM() {
  return currentRPM;
}


// ==================== PID 控制 ====================
// 复位 PID 状态（清零积分和误差）
void resetPID() {
  integral = 0;
  lastError = 0;
  pidOutput = 0;
}

/**
 * PID 计算函数
 * @param samplePeriodMs PID 采样周期（毫秒）
 * 
 * 计算流程：
 *   1. 计算误差 = 目标转速 - 实际转速
 *   2. 更新积分项（带限幅防积分饱和）
 *   3. 计算微分项
 *   4. 输出 = Kp*误差 + Ki*积分 + Kd*微分
 *   5. 将输出限制在 0~255，并应用到电机
 */
void updatePID(unsigned long samplePeriodMs) {
  if (!pidEnabled) return;    // PID 未启用时直接返回
  
  unsigned long now = millis();
  if (now - lastPIDTime < samplePeriodMs) return;
  
  float dt = (now - lastPIDTime) / 1000.0;   // 转换为秒
  lastPIDTime = now;
  
  // 1. 计算误差
  float error = targetRPM - currentRPM;
  
  // 2. 积分项（带限幅，防止积分饱和）
  integral += error * dt;
  integral = constrain(integral, -1000, 1000);
  
  // 3. 微分项
  float derivative = (error - lastError) / dt;
  lastError = error;
  
  // 4. PID 输出
  pidOutput = Kp * error + Ki * integral + Kd * derivative;
  
  // 5. 限制输出范围（0~255）
  pidOutput = constrain(pidOutput, 0, 255);
  
  // 6. 应用到电机
  if (targetRPM >= 0) {
    // 正转
    ledcWrite(PWM_CHANNEL_1, (int)pidOutput);
    ledcWrite(PWM_CHANNEL_2, 0);
  } else {
    // 反转
    ledcWrite(PWM_CHANNEL_1, 0);
    ledcWrite(PWM_CHANNEL_2, (int)pidOutput);
  }
}

// ==================== 串口指令处理 ====================
void processSerialCommand() {
  if (!Serial.available()) return;
  
  // 读取一整行指令（以换行符结束）
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();          // 去除首尾空白
  cmd.toUpperCase();   // 转为大写，不区分大小写
  
  if (cmd.length() == 0) return;
  
  char action = cmd.charAt(0);   // 第一个字符：F / R / S / T / P / I / D
  int speed = DEFAULT_SPEED_PERCENT;  // 默认速度
  
  // 如果指令包含数字（如 F50），解析速度值
  if (cmd.length() > 1) {
    speed = cmd.substring(1).toInt();
    speed = constrain(speed, 0, 100);  // 限制在 0~100
  }

    // ---------- PID 速度控制：T<目标RPM> ----------
  if (action == 'T') {
    if (cmd.length() > 1) {
      targetRPM = cmd.substring(1).toFloat();
      targetRPM = constrain(targetRPM, -200, 200);   // 限制范围
      resetPID();
      pidEnabled = true;
      lastPIDTime = millis();
      Serial.printf("PID start, target: %.1f RPM\n", targetRPM);
    } else {
      Serial.println("Please specify target RPM, eg: T50");
    }
    return;
  }

  
  // ---------- 调整 PID 参数 ----------
  if (action == 'P' && cmd.length() > 1) {
    Kp = cmd.substring(1).toFloat();
    Serial.printf("Kp = %.3f\n", Kp);
    return;
  }
  if (action == 'I' && cmd.length() > 1) {
    Ki = cmd.substring(1).toFloat();
    Serial.printf("Ki = %.3f\n", Ki);
    return;
  }
  if (action == 'D' && cmd.length() > 1) {
    Kd = cmd.substring(1).toFloat();
    Serial.printf("Kd = %.3f\n", Kd);
    return;
  }

  
  switch (action) {
    case 'F':
      motorForward(speed);
      break;
    case 'R':
      motorReverse(speed);
      break;
    case 'S':
      motorStop();
      break;  
    
    default:
      Serial.println("Unknown command!");
      Serial.println("Commands:");
      Serial.println("  F<speed>  -> Forward, 0~100, eg: F50");
      Serial.println("  R<speed>  -> Reverse, 0~100, eg: R80");
      Serial.println("  S         -> Stop");
      Serial.println("  T<rpm>    -> PID speed control, eg: T50");
      Serial.println("  P<kp> I<ki> D<kd> -> Tune PID");
      break;
  }
}

// ==================== 主程序 ====================
void setup() {
  Serial.begin(115200);
  delay(500);

  initMotor();
  initEncoder();

  pinMode(STANDBY, OUTPUT);
  digitalWrite(STANDBY,HIGH);

  lastRPMTime = millis();
  lastPulsesForRPM = 0;
  
  Serial.println("=================================");
  Serial.println("Littel motor control system is running!");
  Serial.println("Commands specification:");
  Serial.println("  F<speed>  → Forward, speed 0~100");
  Serial.println("  R<speed>  → Reverse, speed 0~100");
  Serial.println("  S        → Stop");
  Serial.println("For example: F50 → speed 50%");
  Serial.println("=================================");
}

void loop() {
  // 1. 处理串口指令
  processSerialCommand();
  
  // 2. 每 50ms 更新编码器
  static unsigned long lastEncoderUpdate = 0;
  if (millis() - lastEncoderUpdate >= 50) {
    updateEncoder();
    lastEncoderUpdate = millis();
  }  

  // 3. 每 100ms 计算一次转速
  updateRPM(100);

  // 4. 每 100ms 执行一次 PID 控制
  updatePID(100);

  // 5. 每 1 秒打印状态
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    if (pidEnabled) {
      Serial.printf("Target: %.1f | Actual: %.1f RPM | PWM: %.0f | Error: %.1f\n",
                targetRPM, currentRPM, pidOutput, targetRPM - currentRPM); 
      Serial.println(); }      
    else {
      Serial.printf("PluseNum: %lld | Speed: %.1f RPM\n", totalPulses, currentRPM);
      Serial.println();
    }
    lastPrint = millis();
  } 


}