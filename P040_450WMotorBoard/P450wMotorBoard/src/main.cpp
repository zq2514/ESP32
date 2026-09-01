// ==================== 电机控制程序 ====================
// 引脚定义：
//   D12 (GPIO12) → EN 使能引脚
//   D14 (GPIO14) → PWML2 (PWM2)
//   D27 (GPIO27) → PWML1 (PWM1)
//
// 控制方式：
//   电平控制：EN=0, PWML1/PWML2 组合控制正转/反转/刹车
//   PWM控制：通过PWM占空比控制转速

// 电平控制方式：
//   EN=0, PWM1=0, PWM2=1 → 正转
//   EN=0, PWM1=1, PWM2=0 → 反转
//   EN=0, PWM1=1, PWM2=1 → 刹车
//   EN=1, PWM1=1, PWM2=1 → 自由
//
// PWM控制方式：
//   EN=0, PWM1=PWM, PWM2=1 → 正转（调速）
//   EN=0, PWM1=1, PWM2=PWM → 反转（调速）
//   EN=0, PWM1=1, PWM2=1 → 刹车
//   EN=1, PWM1=1, PWM2=1 → 自由
// ====================================================
// ====================================================

#include <Arduino.h>

// ---------- 引脚定义 ----------
#define PIN_EN    12    // 使能引脚
#define PIN_PWM1  14    // PWM1
#define PIN_PWM2  27    // PWM2

// ---------- PWM 参数 ----------
#define PWM_CHANNEL_1  0   // LEDC 通道0
#define PWM_CHANNEL_2  1   // LEDC 通道1
#define PWM_FREQ       5000 // PWM 频率 5kHz
#define PWM_RESOLUTION 8    // 8位分辨率 (0-255)

// ---------- 电机控制模式枚举 ----------
enum MotorMode {
  MODE_STOP,      // 停止（刹车）
  MODE_FORWARD,   // 正转
  MODE_REVERSE,   // 反转
  MODE_FREE,      // 自由（惯性滑行）
  MODE_PWM        // PWM调速模式
};

// --------------------global status-------------
bool isPWM_Mode = false;   // isPWM flag

// ---------- 函数声明 ----------
void motorInit();
void motorControl(MotorMode mode, int speed = 0);
void setMotorPins(int en, int pwm1, int pwm2);
void setPWM(int pwm1, int pwm2);
void processSerialCommand();

// ---------- setup ----------
void setup() {
  Serial.begin(115200);
  motorInit();
  Serial.println("电机控制系统已启动");
  Serial.println("可用指令：");
  Serial.println("  F  → 正转");
  Serial.println("  R  → 反转");
  Serial.println("  S  → 刹车");
  Serial.println("  X  → 自由");
  Serial.println("  Pnn → PWM调速 (nn=0~100)");
  Serial.println("  例: P50  → 50% 速度正转");
  Serial.println("       P0   → 停止");
}

// ---------- loop ----------
void loop() {
  processSerialCommand();
  delay(10); // 防止串口过载
}

// ==================== 电机初始化 ====================
void motorInit()
{
  // configurate output mode 
  pinMode(PIN_EN, OUTPUT);
  pinMode(PIN_PWM1, OUTPUT);
  pinMode(PIN_PWM2, OUTPUT);

  // Initial state

  setMotorPins(0, 1, 1);
  isPWM_Mode = false ;
  Serial.println("Motor initial success");
}

// ========= Power control function=========
// direct cortrol three pin 
void setMotorPins(int en, int pwm1, int pwm2)
{
  digitalWrite(PIN_EN, en);
  digitalWrite(PIN_PWM1, pwm1);
  digitalWrite(PIN_PWM2, pwm2);
}


// ==================== PWM 设置函数 ====================
// 设置两个PWM通道的占空比 (0-255)
void setPWM(int pwm1, int pwm2){
  ledcWrite(PWM_CHANNEL_1,pwm1);
  ledcWrite(PWM_CHANNEL_2,pwm2);
}


//======== core control functions=============
/**
 * 电机控制函数
 * @param mode  控制模式 (MotorMode)
 * @param speed PWM占空比 (0-255)，仅在 MODE_PWM 模式下有效
 */
//void motorControl(MotorMode mode, int speed=0)
void motorControl(MotorMode mode,int speed)
{
  switch(mode){
    case MODE_FORWARD:
      // pin control: EN=0, pwm1=0,pwm2=1,  forward
      // 1. 先解除PWM绑定，让引脚恢复为普通GPIO
      ledcDetachPin(PIN_PWM1);
      ledcDetachPin(PIN_PWM2);

      setMotorPins(0, 0, 1);
      Serial.println("Motor: forward.");
      break;

    case MODE_REVERSE:

      // pin level : EN=0, pwm1=1,pwm2=0,   reverse 
      // 1. 先解除PWM绑定，让引脚恢复为普通GPIO
      ledcDetachPin(PIN_PWM1);
      ledcDetachPin(PIN_PWM2);
      setMotorPins(0, 1, 0);
      Serial.println("Motor: Reverse.");
      break;

    case MODE_STOP:
      // pin level : EN=0, pwm1=1,pwm2=1,   stop
      // 1. 先解除PWM绑定，让引脚恢复为普通GPIO
      ledcDetachPin(PIN_PWM1);
      ledcDetachPin(PIN_PWM2);
      setMotorPins(0, 1, 1);
      Serial.println("Motor: Stop.");
      break;

    case MODE_FREE:
      // pin level : EN=1, pwm1=1,pwm2=1,   free
      // 1. 先解除PWM绑定，让引脚恢复为普通GPIO
      ledcDetachPin(PIN_PWM1);
      ledcDetachPin(PIN_PWM2);

      setMotorPins(1, 1, 1);
      Serial.println("Motor: Free");
      break;

    case MODE_PWM:
      // PWM control ,en =0, 
      // Speed range of 0-255;
        // bound pwm to pin 
        // configurate pwm channel
      ledcSetup(PWM_CHANNEL_1, PWM_FREQ, PWM_RESOLUTION);
      ledcSetup(PWM_CHANNEL_2, PWM_FREQ, PWM_RESOLUTION);
      ledcAttachPin(PIN_PWM1, PWM_CHANNEL_1);
      ledcAttachPin(PIN_PWM2, PWM_CHANNEL_2);

      if (speed<-255) speed=-255;
      if (speed > 255) speed =255;

      digitalWrite(PIN_EN, 0);  // enalbe
      
      if (speed == 0) {
        // 速度0 → 刹车
        setPWM(255, 255);
        Serial.println("PWM: Stop (speed 0)");
      } else if (speed > 0) {
        // 正转：PWM1=速度, PWM2=1(高电平)
        setPWM(speed, 255);
        Serial.printf("PWM: Forward Speed %d (0-255)\n", speed);
      } else {
        // 反转：PWM1=1(高电平), PWM2=-速度
        int pwmValue = -speed;
        setPWM(255, pwmValue);
        Serial.printf("PWM: Reverse Speed %d (0-255)\n", pwmValue);
      }
      break;

    default:
      Serial.println("Unknown control mode, please check!");
      break;

  }
}

// ===============Serial port  commands deal =============
void processSerialCommand(){
  if(!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim(); // clear space in head and tail;
  cmd.toUpperCase();   // turn Upper write;

  // null  ignore
  if (cmd.length()==0) return;

  // deal single word
  if(cmd.length() == 1){
    char c = cmd.charAt(0);
    switch(c){
      case 'F':
        motorControl(MODE_FORWARD);
        break;
      case 'R':
        motorControl(MODE_REVERSE);
        break;
      case 'S':
        motorControl(MODE_STOP);
        break;
      case 'X':
        motorControl(MODE_FREE);
        break;
      default:
        Serial.println("unknown command.");
        break;   
    }
    return;
  }

  if (cmd.charAt(0) == 'P'){
    String speedStr = cmd.substring(1);
    int speed =speedStr.toInt();
    Serial.print("speed_show:");
    Serial.println(speed);


    if (speed>=-100 && speed<=100){
      // 0-100 reflect to 0-255;
      int pwmValue = map (speed, -100 ,100, -255, 255);
      Serial.printf("pwmValue_show: %d \n",pwmValue);


     // if (pwmValue < 0) pwmValue = -pwmValue;
      motorControl(MODE_PWM, pwmValue);
      // if speed = 0, means stop;
      }else {
        Serial.println();
      }
      return;
    }

  // if unknow commmand
  Serial.println("unkown command");
  Serial.println("Valid command: F(Forward) , R(Reverse) , S(stop), X(Free), Pnn(nn=0-100)");

}



// ===================== pwm control function=============
// ==================== 可选：独立的PWM控制函数 ====================
/**
 * 直接控制PWM引脚，用于更灵活的调速
 * @param speed  -255 ~ 255，正数正转，负数反转，0停止
 */
void motorSpeed(int speed) {
  if (speed == 0) {
    motorControl(MODE_STOP);
    return;
  }
  
  // 限制速度范围
  if (speed > 255) speed = 255;
  if (speed < -255) speed = -255;
  
  digitalWrite(PIN_EN, 0);
  if (speed > 0) {
    // 正转：PWM1 = speed, PWM2 = 0
    setPWM(speed, 0);
    Serial.printf("PWM_Forward: %d\n", speed);
  } else {
    // 反转：PWM1 = 0, PWM2 = -speed
    setPWM(0, -speed);
    Serial.printf("PWM_Reverse: %d\n", -speed);
  }
}


