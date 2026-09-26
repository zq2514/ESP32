#include <Arduino.h>
#include "rs485_protocol.h"

/*
 * ============================================================
 *  RS485 从站程序（Slave）—— 控制 ESP32 板载 LED
 * ============================================================
 *
 *  【从站的职责】
 *  从站是总线上"被动应答"的一方：它大部分时间都在静静监听总线，
 *  收到一帧后先看"地址"是不是发给自己的；是的话就执行命令、
 *  控制板载 LED，再回一帧应答；不是就忽略。
 *
 *  【硬件连接】（与主站相同，自动收发 TTL <-> RS485 模块）
 *    ESP32 3V3           -> 模块 VCC
 *    ESP32 GND           -> 模块 GND
 *    ESP32 GPIO17 (TX2)  -> 模块 RX / DI
 *    ESP32 GPIO16 (RX2)  -> 模块 RO / TX
 *
 *  【串口分配】
 *    Serial2 (GPIO16/17) = RS485 总线，波特率 9600
 *    Serial  (USB/UART0) = 调试输出，波特率 115200（不再污染 RS485 总线）
 *
 *  【板载 LED】
 *    LED 接在 GPIO2 上。不同板子的点亮极性不同：有的"低电平点亮"
 *    （active-low），有的"高电平点亮"（active-high）。本板实测为
 *    "高电平点亮"，见下方 LED_ACTIVE_LOW 定义。
 * ============================================================
 */

// ---- RS485 串口（Serial2）----
#define RS485_SERIAL    Serial2
#define RS485_BAUD      9600        // 必须和主站一致
#define RS485_RX_PIN    16
#define RS485_TX_PIN    17

// ---- 调试串口（USB / UART0）----
#define DEBUG_SERIAL    Serial
#define DEBUG_BAUD      115200

// ---- 从站配置 ----
// 本从站地址：默认 0x01，可在 platformio.ini 里用 build_flags 覆盖（如 -DSLAVE_ADDR=0x02）。
// 新增从站时不要改这里，在 platformio.ini 里加一个 [env:slave_XX] 环境即可。
// 注意：#ifndef 的意思是"如果编译时还没定义 SLAVE_ADDR 才用这个默认值"，
//       这样 platformio.ini 里 -DSLAVE_ADDR=0x02 就能把它覆盖掉。
#ifndef SLAVE_ADDR
#define SLAVE_ADDR     0x01
#endif

// ---- LED 配置 ----
#define LED_PIN        2           // ESP32 板载 LED = GPIO2
#define LED_ACTIVE_LOW 0           // 0 = 高电平点亮（on 时输出 HIGH）；1 = 低电平点亮

// =====================电机引脚定义====================
#define PIN_EN      12   // 使能引脚
#define PIN_PWM1     14   // PWM1
#define PIN_PWM2  27    // PWM2

// ---------- PWM 参数 ----------
#define PWM_CHANNEL_1  0
#define PWM_CHANNEL_2  1
#define PWM_FREQ       5000
#define PWM_RESOLUTION 8

// ---------- 电机控制模式枚举 ----------
enum MotorMode {
  MODE_STOP,
  MODE_FORWARD,
  MODE_REVERSE,
  MODE_FREE,
  MODE_PWM
};

bool isPWM_Mode = false;


static bool ledState = false;      // LED 逻辑状态（true=开, false=关），与引脚电平解耦



void motorInit();
void motorControl(MotorMode mode, int speed);
void setMotorPins(int en, int pwm1, int pwm2);
void setPWM(int pwm1, int pwm2);



/*
 * 应用 LED 状态。
 * 这里用 LED_ACTIVE_LOW 宏把"逻辑状态"翻译成"引脚电平"：
 *   - active-high(0)：开 -> 输出 HIGH，关 -> 输出 LOW
 *   - active-low (1)：开 -> 输出 LOW，关 -> 输出 HIGH
 * 好处：换一块极性相反的板子，只改这一个宏，其它逻辑都不用动。
 */
void applyLed(bool on) {
    ledState = on;
#if LED_ACTIVE_LOW
    digitalWrite(LED_PIN, on ? LOW : HIGH);
#else
    digitalWrite(LED_PIN, on ? HIGH : LOW);
#endif
}


// ==================== 电机初始化 ====================
void motorInit() {
    pinMode(PIN_EN, OUTPUT);
    pinMode(PIN_PWM1, OUTPUT);
    pinMode(PIN_PWM2, OUTPUT);
    setMotorPins(0, 1, 1);   // 初始：刹车
    isPWM_Mode = false;
    DEBUG_SERIAL.println("[Slave] Motor init done");
}

void setMotorPins(int en, int pwm1, int pwm2) {
    digitalWrite(PIN_EN, en);
    digitalWrite(PIN_PWM1, pwm1);
    digitalWrite(PIN_PWM2, pwm2);
}

void setPWM(int pwm1, int pwm2) {
    ledcWrite(PWM_CHANNEL_1, pwm1);
    ledcWrite(PWM_CHANNEL_2, pwm2);
}

// ==================== 电机核心控制 ====================
void motorControl(MotorMode mode, int speed) {
    switch (mode) {
        case MODE_FORWARD:
            ledcDetachPin(PIN_PWM1);
            ledcDetachPin(PIN_PWM2);
            setMotorPins(0, 0, 1);
            DEBUG_SERIAL.println("[Slave] Motor: forward");
            break;

        case MODE_REVERSE:
            ledcDetachPin(PIN_PWM1);
            ledcDetachPin(PIN_PWM2);
            setMotorPins(0, 1, 0);
            DEBUG_SERIAL.println("[Slave] Motor: reverse");
            break;

        case MODE_STOP:
            ledcDetachPin(PIN_PWM1);
            ledcDetachPin(PIN_PWM2);
            setMotorPins(0, 1, 1);
            DEBUG_SERIAL.println("[Slave] Motor: stop");
            break;

        case MODE_FREE:
            ledcDetachPin(PIN_PWM1);
            ledcDetachPin(PIN_PWM2);
            setMotorPins(1, 1, 1);
            DEBUG_SERIAL.println("[Slave] Motor: free");
            break;

        case MODE_PWM:
            ledcSetup(PWM_CHANNEL_1, PWM_FREQ, PWM_RESOLUTION);
            ledcSetup(PWM_CHANNEL_2, PWM_FREQ, PWM_RESOLUTION);
            ledcAttachPin(PIN_PWM1, PWM_CHANNEL_1);
            ledcAttachPin(PIN_PWM2, PWM_CHANNEL_2);

            if (speed < -255) speed = -255;
            if (speed > 255) speed = 255;

            digitalWrite(PIN_EN, 0);

            if (speed == 0) {
                setPWM(255, 255);
                DEBUG_SERIAL.println("[Slave] PWM: stop");
            } else if (speed > 0) {
                setPWM(speed, 255);
                DEBUG_SERIAL.printf("[Slave] PWM forward: %d\n", speed);
            } else {
                int pwmValue = -speed;
                setPWM(255, pwmValue);
                DEBUG_SERIAL.printf("[Slave] PWM reverse: %d\n", pwmValue);
            }
            break;

        default:
            DEBUG_SERIAL.println("[Slave] Unknown motor mode");
            break;
    }
}









// 发送一帧应答（发给主站）。从站应答时命令字要带上 RESP_FLAG。
void sendResponse(uint8_t cmd, const uint8_t *data, uint8_t len) {
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t total = buildFrame(frame, SLAVE_ADDR, cmd, data, len);
    RS485_SERIAL.write(frame, total);
    RS485_SERIAL.flush();
}

/*
 * 阻塞接收一帧合法数据，返回帧总长度。
 * 与主站的 recvResponse() 是同一套"逐字节状态机"思路：找帧头 -> 定长度 ->
 * 收满 -> 校验，只是从站会一直等（阻塞）到收到一帧合法数据为止。
 * 噪声、发给别人的半截数据都会被自动跳过。
 */
uint8_t recvFrame(uint8_t *frame) {
    uint8_t idx = 0, totalLen = 0;

    while (true) {                          // 无限循环，直到收到合法帧
        while (!RS485_SERIAL.available()) { /* 等待数据到来 */ }

        uint8_t b = RS485_SERIAL.read();

        // ---- 帧头同步 ----
        if (idx == 0) {
            if (b != FRAME_HEAD1) continue; // 不是 0xA5，丢弃
        } else if (idx == 1) {
            if (b != FRAME_HEAD2) { idx = 0; continue; } // 不是 0x5A，重来
        }

        frame[idx++] = b;

        // 读到 LEN 后确定整帧长度
        if (idx == 5) {
            if (frame[4] > FRAME_DATA_MAX) { idx = 0; continue; } // LEN 非法，重同步
            totalLen = 5 + frame[4] + 1;
        }

        // 收满一帧，校验合法性
        if (totalLen && idx == totalLen) {
            if (verifyFrame(frame, totalLen)) {
                return totalLen;            // 合法帧，返回
            }
            idx = 0; totalLen = 0;          // 校验失败，重新同步
        }

        if (idx >= FRAME_MAX_LEN) { idx = 0; totalLen = 0; }  // 防溢出
    }
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    applyLed(false);                        // 上电初始：关灯

    motorInit();                            // 初始化电机

    DEBUG_SERIAL.begin(DEBUG_BAUD);
    RS485_SERIAL.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    DEBUG_SERIAL.println("[Slave] RS485 slave started.");
    DEBUG_SERIAL.printf("[Slave] addr=0x%02X, RS485 baud=%d\r\n", SLAVE_ADDR, RS485_BAUD);
}

void loop() {
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t totalLen = recvFrame(frame);    // 阻塞等到一帧

    // 按帧格式拆出各字段
    uint8_t addr = frame[2];    // 地址
    uint8_t cmd  = frame[3];    // 命令
    uint8_t len  = frame[4];    // 数据长度
    uint8_t *data = &frame[5];  // 数据指针（指向 frame 第 6 字节）

    // 地址匹配：只处理发给本机的帧，其余直接忽略（不回话）
    if (addr != SLAVE_ADDR) {
        return;
    }

    switch (cmd) {
        case CMD_WRITE_LED: {               // 主站要求写 LED
            bool on = (len > 0 && data[0] == LED_ON);  // 数据第 1 字节 0x01=开
            applyLed(on);

            // 回一帧应答，把实际状态回显给主站，方便主站确认
            uint8_t ackData[1] = { (uint8_t)(on ? LED_ON : LED_OFF) };
            sendResponse(CMD_WRITE_LED | RESP_FLAG, ackData, 1);

            DEBUG_SERIAL.printf("[Slave] write LED -> %s\r\n", on ? "ON" : "OFF");
            break;
        }

        case CMD_READ_LED: {                // 主站查询 LED 状态
            uint8_t ackData[1] = { (uint8_t)(ledState ? LED_ON : LED_OFF) };
            sendResponse(CMD_READ_LED | RESP_FLAG, ackData, 1);

            DEBUG_SERIAL.printf("[Slave] read LED -> %s\r\n", ledState ? "ON" : "OFF");
            break;
        }


                // ---------- 新增：电机正转 ----------
        case CMD_MOTOR_FWD: {
            int speed = (len > 0) ? data[0] : 100;
            int pwmValue = map(speed, 0, 100, 0, 255);
            motorControl(MODE_PWM, pwmValue);
            uint8_t ack[1] = { (uint8_t)speed };
            sendResponse(CMD_MOTOR_FWD | RESP_FLAG, ack, 1);
            break;
        }

        // ---------- 新增：电机反转 ----------
        case CMD_MOTOR_REV: {
            int speed = (len > 0) ? data[0] : 100;
            int pwmValue = map(speed, 0, 100, 0, 255);
            motorControl(MODE_PWM, -pwmValue);
            uint8_t ack[1] = { (uint8_t)speed };
            sendResponse(CMD_MOTOR_REV | RESP_FLAG, ack, 1);
            break;
        }

        // ---------- 新增：电机停止 ----------
        case CMD_MOTOR_STOP: {
            motorControl(MODE_STOP, 0);
            sendResponse(CMD_MOTOR_STOP | RESP_FLAG, nullptr, 0);
            break;
        }


        default:                            // 未知命令，忽略（只打印提示）
            DEBUG_SERIAL.printf("[Slave] unknown cmd 0x%02X\r\n", cmd);
            break;
    }
}
