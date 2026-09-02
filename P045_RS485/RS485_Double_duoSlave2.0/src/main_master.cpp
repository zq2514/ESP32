#include <Arduino.h>
#include "rs485_protocol.h"

/*
 * ============================================================
 *  RS485 主站程序（Master）—— 一主多从轮询
 * ============================================================
 *
 *  硬件连接（自动收发 TTL <-> RS485 模块，无需 DE/RE 方向控制脚）：
 *    ESP32 3V3          -> 模块 VCC
 *    ESP32 GND          -> 模块 GND
 *    ESP32 TX0 (GPIO1)  -> 模块 RX / DI
 *    ESP32 RX0 (GPIO3)  -> 模块 TX / RO
 *
 *  注意：TX0/RX0 即 UART0（Serial），与开发板 USB 串口芯片共用，
 *  若有冲突请改用 Serial2（TX=GPIO17, RX=GPIO16），改 RS485_SERIAL 即可。
 *
 *  从站地址在下方 SLAVE_ADDRS[] 数组中维护：
 *    新增一块从站 -> 数组里加一个地址；
 *    同时到 platformio.ini 里加一个 [env:slave_XX] 环境并烧录。
 *  从站程序无需改动。
 * ============================================================
 */

// ---- 串口配置 ----
#define RS485_SERIAL        Serial      // UART0: TX0=GPIO1, RX0=GPIO3
#define RS485_BAUD          9600

// ---- 通信参数 ----
#define RESP_TIMEOUT_MS     200         // 单从站应答超时（毫秒）
#define POLL_INTERVAL_MS    1000        // 一轮轮询结束后的间隔

// ---- 从站地址列表（新增从站：在这里加一个地址即可）----
const uint8_t SLAVE_ADDRS[] = { 0x01, 0x02 };
const uint8_t SLAVE_COUNT = sizeof(SLAVE_ADDRS) / sizeof(SLAVE_ADDRS[0]);

// 每块从站的运行状态
struct SlaveStatus {
    uint8_t addr;      // 从站地址
    bool    online;    // 是否在线（上轮是否应答）
    bool    ledOn;     // 记录的 LED 状态
};

SlaveStatus slaves[SLAVE_COUNT];

// 函数声明
bool sendCommand(uint8_t addr, uint8_t cmd, const uint8_t *data, uint8_t len);
bool recvResponse(uint8_t addr, uint8_t expectCmd, uint8_t *data, uint8_t *len,
                  uint32_t timeoutMs);
void pollSlave(SlaveStatus &s);

void setup() {
    RS485_SERIAL.begin(RS485_BAUD);

    for (uint8_t i = 0; i < SLAVE_COUNT; i++) {
        slaves[i].addr   = SLAVE_ADDRS[i];
        slaves[i].online = false;
        slaves[i].ledOn  = false;
    }

    RS485_SERIAL.printf("[Master] started, slave count=%d, baud=%d\r\n",
                        SLAVE_COUNT, RS485_BAUD);
}

void loop() {
    // 逐一轮询每块从站
    for (uint8_t i = 0; i < SLAVE_COUNT; i++) {
        pollSlave(slaves[i]);
        delay(50);                   // 两从站之间留间隔，避免总线切换太快
    }
    delay(POLL_INTERVAL_MS);         // 一轮轮询结束后的间隔
}

// 轮询一块从站：翻转其 LED（写命令 + 应答回显状态）
void pollSlave(SlaveStatus &s) {
    bool wantOn = !s.ledOn;          // 每轮翻转一次
    uint8_t wrData[1] = { (uint8_t)(wantOn ? LED_ON : LED_OFF) };

    if (!sendCommand(s.addr, CMD_WRITE_LED, wrData, 1)) {
        s.online = false;
        RS485_SERIAL.printf("[Master] slave 0x%02X: send failed\r\n", s.addr);
        return;
    }

    uint8_t rsp[FRAME_DATA_MAX];
    uint8_t rspLen = 0;
    if (recvResponse(s.addr, CMD_WRITE_LED | RESP_FLAG, rsp, &rspLen,
                     RESP_TIMEOUT_MS)) {
        s.ledOn  = (rspLen > 0 && rsp[0] == LED_ON);
        s.online = true;
        RS485_SERIAL.printf("[Master] slave 0x%02X: LED=%s\r\n",
                            s.addr, s.ledOn ? "ON" : "OFF");
    } else {
        s.online = false;
        RS485_SERIAL.printf("[Master] slave 0x%02X: no response\r\n", s.addr);
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
