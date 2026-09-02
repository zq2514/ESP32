#include <Arduino.h>
#include "rs485_protocol.h"

/*
 * ============================================================
 *  RS485 主站程序（Master）
 * ============================================================
 *
 *  硬件连接（自动收发 TTL <-> RS485 模块，无需 DE/RE 方向控制脚）：
 *    ESP32 3V3          -> 模块 VCC
 *    ESP32 GND          -> 模块 GND
 *    ESP32 TX0 (GPIO1)  -> 模块 RX / DI
 *    ESP32 RX0 (GPIO3)  -> 模块 TX / RO
 *
 *  注意：TX0/RX0 即 UART0（Serial）。标准 ESP32 开发板的 USB 串口
 *  芯片（CP2102/CH340）也接在这两个引脚上，会与 RS485 模块产生
 *  总线竞争。若使用带 USB 串口的开发板，建议改用其它串口，例如
 *  Serial2（TX=GPIO17, RX=GPIO16），只需修改下方 RS485_SERIAL 宏。
 * ============================================================
 */

// ---- 串口配置 ----
#define RS485_SERIAL        Serial    // UART0: TX0=GPIO1, RX0=GPIO3
#define RS485_BAUD          9600      // RS485 波特率（可改为 115200）

// ---- 通信参数 ----
#define SLAVE_ADDR          0x01      // 目标从站地址
#define RESP_TIMEOUT_MS     200       // 等待从站应答的超时时间（毫秒）

// ---- 周期控制 ----
#define POLL_INTERVAL_MS    1000      // 每 1 秒执行一次收发

// 函数声明
bool sendCommand(uint8_t addr, uint8_t cmd, const uint8_t *data, uint8_t len);
bool recvResponse(uint8_t addr, uint8_t expectCmd, uint8_t *data, uint8_t *len,
                  uint32_t timeoutMs);

void setup() {
    RS485_SERIAL.begin(RS485_BAUD);
    // 自动收发模块内部自行切换方向，主站无需操作方向引脚

    RS485_SERIAL.println("[Master] RS485 master started.");
    RS485_SERIAL.printf("[Master] slave addr=0x%02X, baud=%d\r\n", SLAVE_ADDR, RS485_BAUD);
}

void loop() {
    static uint8_t ledState  = LED_OFF;
    static uint32_t lastTick = 0;

    if (millis() - lastTick < POLL_INTERVAL_MS) {
        return;
    }
    lastTick = millis();

    // ---- 1. 写 LED：每次翻转一次，演示开关 ----
    ledState = (ledState == LED_OFF) ? LED_ON : LED_OFF;

    uint8_t wrData[1] = { ledState };
    RS485_SERIAL.printf("[Master] -> write LED = %s ...\r\n",
                        ledState == LED_ON ? "ON" : "OFF");

    if (sendCommand(SLAVE_ADDR, CMD_WRITE_LED, wrData, 1)) {
        uint8_t rspData[FRAME_DATA_MAX];
        uint8_t rspLen = 0;
        if (recvResponse(SLAVE_ADDR, CMD_WRITE_LED | RESP_FLAG, rspData, &rspLen,
                         RESP_TIMEOUT_MS)) {
            RS485_SERIAL.println("[Master] <- slave ACK, write OK.");
        } else {
            RS485_SERIAL.println("[Master] <- no response (write).");
        }
    } else {
        RS485_SERIAL.println("[Master] write send failed.");
    }

    delay(50);  // 两帧之间留出间隔，避免从站来不及处理

    // ---- 2. 读 LED：查询从站当前状态 ----
    RS485_SERIAL.println("[Master] -> read LED ...");

    if (sendCommand(SLAVE_ADDR, CMD_READ_LED, nullptr, 0)) {
        uint8_t rspData[FRAME_DATA_MAX];
        uint8_t rspLen = 0;
        if (recvResponse(SLAVE_ADDR, CMD_READ_LED | RESP_FLAG, rspData, &rspLen,
                         RESP_TIMEOUT_MS)) {
            bool isOn = (rspLen > 0 && rspData[0] == LED_ON);
            RS485_SERIAL.printf("[Master] <- LED state = %s\r\n", isOn ? "ON" : "OFF");
        } else {
            RS485_SERIAL.println("[Master] <- no response (read).");
        }
    } else {
        RS485_SERIAL.println("[Master] read send failed.");
    }
}

// 发送一帧命令（自动收发模块内部切换方向，主站直接写串口即可）
bool sendCommand(uint8_t addr, uint8_t cmd, const uint8_t *data, uint8_t len) {
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t total = buildFrame(frame, addr, cmd, data, len);
    size_t written = RS485_SERIAL.write(frame, total);
    RS485_SERIAL.flush();          // 等待数据从 UART 缓冲全部发出
    return written == total;
}

// 接收一帧应答：同步帧头 -> 校验长度/校验和 -> 匹配地址与命令字
bool recvResponse(uint8_t addr, uint8_t expectCmd, uint8_t *data, uint8_t *len,
                  uint32_t timeoutMs) {
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t idx      = 0;
    uint8_t totalLen = 0;
    uint32_t start   = millis();

    while (millis() - start < timeoutMs) {
        if (!RS485_SERIAL.available()) {
            delay(1);              // 无数据时短暂等待，避免空转
            continue;
        }

        uint8_t b = RS485_SERIAL.read();

        // ---- 帧头同步 ----
        if (idx == 0) {
            if (b != FRAME_HEAD1) continue;
        } else if (idx == 1) {
            if (b != FRAME_HEAD2) { idx = 0; continue; }
        }

        frame[idx++] = b;

        // 读满 5 字节头部（头2+地址1+命令1+长度1）后确定整帧长度
        if (idx == 5) {
            if (frame[4] > FRAME_DATA_MAX) { idx = 0; continue; }  // 长度非法，重同步
            totalLen = 5 + frame[4] + 1;
        }

        // 收满一帧，做校验与匹配
        if (totalLen && idx == totalLen) {
            if (!verifyFrame(frame, totalLen) ||
                frame[2] != addr || frame[3] != expectCmd) {
                idx = 0; totalLen = 0;   // 非法帧或非期望应答，丢弃并重新同步
                continue;
            }

            // 成功：回填数据
            uint8_t dataLen = frame[4];
            if (data && len) {
                *len = dataLen;
                for (uint8_t i = 0; i < dataLen; i++) {
                    data[i] = frame[5 + i];
                }
            }
            return true;
        }

        if (idx >= FRAME_MAX_LEN) { idx = 0; totalLen = 0; }  // 防止缓冲区溢出
    }

    return false;  // 超时
}
