#include <Arduino.h>
#include "rs485_protocol.h"

/*
 * ============================================================
 *  RS485 从站程序（Slave）—— 控制 ESP32 板载 LED
 * ============================================================
 *
 *  硬件连接（与主站相同，自动收发 TTL <-> RS485 模块）：
 *    ESP32 3V3          -> 模块 VCC
 *    ESP32 GND          -> 模块 GND
 *    ESP32 TX0 (GPIO1)  -> 模块 RX / DI
 *    ESP32 RX0 (GPIO3)  -> 模块 TX / RO
 *
 *  板载 LED：LED_BUILTIN = GPIO2，低电平点亮（active-low）。
 *
 *  注意：TX0/RX0 即 UART0（Serial），调试输出与 RS485 共用同一串口，
 *  打印内容会同时进入 RS485 总线，但协议靠帧头+校验和会自动忽略垃圾数据。
 * ============================================================
 */

// ---- 串口配置 ----
#define RS485_SERIAL   Serial      // UART0: TX0=GPIO1, RX0=GPIO3
#define RS485_BAUD     9600        // 与主站保持一致

// ---- 从站配置 ----
// 本从站地址：默认 0x01，可在 platformio.ini 里用 build_flags 覆盖（如 -DSLAVE_ADDR=0x02）。
// 新增从站时不要改这里，在 platformio.ini 里加一个 [env:slave_XX] 环境即可。
#ifndef SLAVE_ADDR
#define SLAVE_ADDR     0x01
#endif

// ---- LED 配置 ----
#define LED_PIN        2           // ESP32 板载 LED = GPIO2
#define LED_ACTIVE_LOW 1           // 板载 LED 低电平点亮

static bool ledState = false;      // LED 逻辑状态（true=开, false=关）

// 应用 LED 状态（处理低电平点亮的极性）
void applyLed(bool on) {
    ledState = on;
#if LED_ACTIVE_LOW
    digitalWrite(LED_PIN, on ? LOW : HIGH);
#else
    digitalWrite(LED_PIN, on ? HIGH : LOW);
#endif
}

// 发送一帧应答
void sendResponse(uint8_t cmd, const uint8_t *data, uint8_t len) {
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t total = buildFrame(frame, SLAVE_ADDR, cmd, data, len);
    RS485_SERIAL.write(frame, total);
    RS485_SERIAL.flush();
}

// 阻塞接收一帧合法数据（同步帧头 -> 校验长度/校验和），返回帧总长度
uint8_t recvFrame(uint8_t *frame) {
    uint8_t idx = 0, totalLen = 0;

    while (true) {
        while (!RS485_SERIAL.available()) { /* 等待数据 */ }

        uint8_t b = RS485_SERIAL.read();

        // ---- 帧头同步 ----
        if (idx == 0) {
            if (b != FRAME_HEAD1) continue;
        } else if (idx == 1) {
            if (b != FRAME_HEAD2) { idx = 0; continue; }
        }

        frame[idx++] = b;

        // 读满 5 字节头部后确定整帧长度
        if (idx == 5) {
            if (frame[4] > FRAME_DATA_MAX) { idx = 0; continue; }
            totalLen = 5 + frame[4] + 1;
        }

        // 收满一帧，校验合法性
        if (totalLen && idx == totalLen) {
            if (verifyFrame(frame, totalLen)) {
                return totalLen;
            }
            idx = 0; totalLen = 0;   // 校验失败，重新同步
        }

        if (idx >= FRAME_MAX_LEN) { idx = 0; totalLen = 0; }  // 防止溢出
    }
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    applyLed(false);                        // 初始关灯

    RS485_SERIAL.begin(RS485_BAUD);
    RS485_SERIAL.println("[Slave] RS485 slave started.");
    RS485_SERIAL.printf("[Slave] addr=0x%02X, baud=%d\r\n", SLAVE_ADDR, RS485_BAUD);
}

void loop() {
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t totalLen = recvFrame(frame);

    uint8_t addr = frame[2];
    uint8_t cmd  = frame[3];
    uint8_t len  = frame[4];
    uint8_t *data = &frame[5];

    // 地址匹配：仅处理发给本机的帧，其余忽略
    if (addr != SLAVE_ADDR) {
        return;
    }

    switch (cmd) {
        case CMD_WRITE_LED: {
            bool on = (len > 0 && data[0] == LED_ON);
            applyLed(on);

            // 先发应答帧，再打印调试（避免调试文字插到应答之前）
            uint8_t ackData[1] = { (uint8_t)(on ? LED_ON : LED_OFF) };
            sendResponse(CMD_WRITE_LED | RESP_FLAG, ackData, 1);

            RS485_SERIAL.printf("[Slave] write LED -> %s\r\n", on ? "ON" : "OFF");
            break;
        }

        case CMD_READ_LED: {
            uint8_t ackData[1] = { (uint8_t)(ledState ? LED_ON : LED_OFF) };
            sendResponse(CMD_READ_LED | RESP_FLAG, ackData, 1);

            RS485_SERIAL.printf("[Slave] read LED -> %s\r\n", ledState ? "ON" : "OFF");
            break;
        }

        default:
            RS485_SERIAL.printf("[Slave] unknown cmd 0x%02X\r\n", cmd);
            break;
    }
}
