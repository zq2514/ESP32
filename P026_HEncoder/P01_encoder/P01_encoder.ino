// 编码器引脚定义
#define ENC_A_LEFT  25
#define ENC_B_LEFT  26

// 全局脉冲计数器
volatile long leftPulses = 0;
volatile long rightPulses = 0;  // 先不用，但保留

// 中断服务函数
void IRAM_ATTR leftEncoderISR() {
  // 通常只需要检测A相上升沿，如需方向可检测B相状态，此处简化
  leftPulses++;
}

void setup() {
  Serial.begin(115200);
  pinMode(ENC_A_LEFT, INPUT_PULLUP);
  pinMode(ENC_B_LEFT, INPUT_PULLUP);
  
  // 附着中断，上升沿触发
  attachInterrupt(digitalPinToInterrupt(ENC_A_LEFT), leftEncoderISR, RISING);
}

void loop() {
  // 读取脉冲数（注意关中断保护）
  noInterrupts();
  long pulses = leftPulses;
  interrupts();
  
  Serial.print("Pulses: ");
  Serial.println(pulses);
  delay(500);
}
