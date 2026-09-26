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

// ==================== 默认速度 ====================
#define DEFAULT_SPEED_PERCENT 50   // 默认速度 50%（范围 0~100）

// ==================== PCNT 配置 ====================
#define PCNT_UNIT PCNT_UNIT_0

int16_t lastCount = 0;
int64_t totalPulses = 0;

// ==================== 函数声明 ====================
void initMotor();
void initEncoder();
void updateEncoder();
void motorForward(int speedPercent);
void motorReverse(int speedPercent);
void motorStop();
void processSerialCommand();

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

// ==================== 串口指令处理 ====================
void processSerialCommand() {
  if (!Serial.available()) return;
  
  // 读取一整行指令（以换行符结束）
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();          // 去除首尾空白
  cmd.toUpperCase();   // 转为大写，不区分大小写
  
  if (cmd.length() == 0) return;
  
  char action = cmd.charAt(0);   // 第一个字符：F / R / S
  int speed = DEFAULT_SPEED_PERCENT;  // 默认速度
  
  // 如果指令包含数字（如 F50），解析速度值
  if (cmd.length() > 1) {
    speed = cmd.substring(1).toInt();
    speed = constrain(speed, 0, 100);  // 限制在 0~100
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
      Serial.println("Unknow commands!");
      Serial.println("Commands format:");
      Serial.println("  F<speed>  → Forward, Speed 0~100, eg: F50");
      Serial.println("  R<Speed>  → Reverse, Speed 0~100, eg:R80");
      Serial.println("  S        → Stop!");
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
  
  // 3. 每 1 秒打印编码器脉冲数
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    Serial.printf("Encoder pulses Number: %lld \n", totalPulses);
    Serial.println();
    lastPrint = millis();
  }
}