#include <Arduino.h>
#include "rs485_protocol.h"

/*
 * ============================================================
 *  RS485 主站程序（Master）—— 串口命令手动控制从站 LED
 * ============================================================
 *
 *  硬件连接（自动收发 TTL <-> RS485 模块）：
 *    ESP32 3V3           -> 模块 VCC
 *    ESP32 GND           -> 模块 GND
 *    ESP32 GPIO17 (TX2)  -> 模块 RX / DI
 *    ESP32 GPIO16 (RX2)  -> 模块 RO / TX
 *
 *  串口分配：
 *    Serial2 (GPIO16/17) = RS485 总线，波特率 9600
 *    Serial  (USB/UART0) = 控制台，波特率 115200（串口监视器选 115200）
 *
 *  控制命令（在串口监视器输入，回车发送）：
 *    on  <addr>    开灯，如 on 1 / on 0x02
 *    off <addr>    关灯，如 off 1
 *    read <addr>   查询 LED 状态
 *    on/off/read all    对所有已配置从站操作
 *    help          显示帮助
 *
 *  从站地址在 SLAVE_ADDRS[] 中维护，新增从站在这里加地址即可。
 * ============================================================
 */

// ---- RS485 串口（Serial2）----
#define RS485_SERIAL    Serial2
#define RS485_BAUD      9600
#define RS485_RX_PIN    16
#define RS485_TX_PIN    17

// ---- 控制台串口（USB / UART0）----
#define CONSOLE_SERIAL  Serial
#define CONSOLE_BAUD    115200

// ---- 通信参数 ----
#define RESP_TIMEOUT_MS 200         // 单从站应答超时（毫秒）

// ---- 从站地址列表（新增从站：在这里加地址即可）----
const uint8_t SLAVE_ADDRS[] = { 0x01, 0x02 };
const uint8_t SLAVE_COUNT = sizeof(SLAVE_ADDRS) / sizeof(SLAVE_ADDRS[0]);

// 函数声明
bool sendCommand(uint8_t addr, uint8_t cmd, const uint8_t *data, uint8_t len);
bool recvResponse(uint8_t addr, uint8_t expectCmd, uint8_t *data, uint8_t *len,
                  uint32_t timeoutMs);
bool writeLed(uint8_t addr, bool on);
bool readLed(uint8_t addr, bool &on);
void doWrite(uint8_t addr, bool on);
void doRead(uint8_t addr);
void handleLine(String &line);
bool parseAddr(const String &s, uint8_t &addr);
void printHelp();

void setup() {
    CONSOLE_SERIAL.begin(CONSOLE_BAUD);
    RS485_SERIAL.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    CONSOLE_SERIAL.println("[Master] RS485 master started.");
    CONSOLE_SERIAL.printf("[Master] RS485 baud=%d, console baud=%d\r\n",
                          RS485_BAUD, CONSOLE_BAUD);
    printHelp();
    CONSOLE_SERIAL.print("> ");
}

void loop() {
    if (CONSOLE_SERIAL.available()) {
        String line = CONSOLE_SERIAL.readStringUntil('\n');
        handleLine(line);
        CONSOLE_SERIAL.print("> ");
    }
}

// ---- 命令解析 ----
void handleLine(String &line) {
    line.trim();                         // 去掉首尾空白和 '\r'
    if (line.length() == 0) return;

    int sp = line.indexOf(' ');          // 拆成「命令」和「参数」
    String cmd = (sp < 0) ? line : line.substring(0, sp);
    String arg = (sp < 0) ? "" : line.substring(sp + 1);
    cmd.toLowerCase();
    arg.trim();
    arg.toLowerCase();

    if (cmd == "help" || cmd == "?") {
        printHelp();
        return;
    }

    if (cmd == "on" || cmd == "off") {
        bool on = (cmd == "on");
        if (arg == "all") {
            for (uint8_t i = 0; i < SLAVE_COUNT; i++) {
                doWrite(SLAVE_ADDRS[i], on);
            }
        } else {
            uint8_t addr;
            if (!parseAddr(arg, addr)) {
                CONSOLE_SERIAL.println("[Master] usage: on/off <addr|all>");
                return;
            }
            doWrite(addr, on);
        }
        return;
    }

    if (cmd == "read") {
        if (arg == "all") {
            for (uint8_t i = 0; i < SLAVE_COUNT; i++) {
                doRead(SLAVE_ADDRS[i]);
            }
        } else {
            uint8_t addr;
            if (!parseAddr(arg, addr)) {
                CONSOLE_SERIAL.println("[Master] usage: read <addr|all>");
                return;
            }
            doRead(addr);
        }
        return;
    }

    CONSOLE_SERIAL.printf("[Master] unknown cmd \"%s\", type help\r\n", cmd.c_str());
}

// 解析地址：支持十进制(1,2)和 0x 前缀十六进制(0x02)
bool parseAddr(const String &s, uint8_t &addr) {
    if (s.length() == 0) return false;

    int base = 10;
    const char *p = s.c_str();
    if (s.length() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        p += 2;
    }

    char *end;
    long v = strtol(p, &end, base);
    if (end == p || *end != '\0' || v <= 0 || v > 0xFE) return false;
    addr = (uint8_t)v;
    return true;
}

// 对一块从站执行写 LED，并打印结果
void doWrite(uint8_t addr, bool on) {
    CONSOLE_SERIAL.printf("  -> set slave 0x%02X LED %s ... ", addr, on ? "ON" : "OFF");
    if (writeLed(addr, on)) {
        CONSOLE_SERIAL.println("OK");
    } else {
        CONSOLE_SERIAL.println("no response");
    }
}

// 对一块从站执行读 LED，并打印结果
void doRead(uint8_t addr) {
    bool on;
    if (readLed(addr, on)) {
        CONSOLE_SERIAL.printf("  slave 0x%02X LED = %s\r\n", addr, on ? "ON" : "OFF");
    } else {
        CONSOLE_SERIAL.printf("  slave 0x%02X: no response\r\n", addr);
    }
}

// 写 LED：发命令 + 收应答
bool writeLed(uint8_t addr, bool on) {
    uint8_t data[1] = { (uint8_t)(on ? LED_ON : LED_OFF) };
    if (!sendCommand(addr, CMD_WRITE_LED, data, 1)) return false;

    uint8_t rsp[FRAME_DATA_MAX];
    uint8_t rspLen = 0;
    return recvResponse(addr, CMD_WRITE_LED | RESP_FLAG, rsp, &rspLen, RESP_TIMEOUT_MS);
}

// 读 LED：发命令 + 收应答，结果写回 on
bool readLed(uint8_t addr, bool &on) {
    if (!sendCommand(addr, CMD_READ_LED, nullptr, 0)) return false;

    uint8_t rsp[FRAME_DATA_MAX];
    uint8_t rspLen = 0;
    if (!recvResponse(addr, CMD_READ_LED | RESP_FLAG, rsp, &rspLen, RESP_TIMEOUT_MS)) {
        return false;
    }
    on = (rspLen > 0 && rsp[0] == LED_ON);
    return true;
}

void printHelp() {
    CONSOLE_SERIAL.println("==============================");
    CONSOLE_SERIAL.println(" RS485 Master 命令");
    CONSOLE_SERIAL.println("  on  <addr>   开灯 (如 on 1 / on 0x02)");
    CONSOLE_SERIAL.println("  off <addr>   关灯 (如 off 1)");
    CONSOLE_SERIAL.println("  read <addr>  查询状态 (如 read 2)");
    CONSOLE_SERIAL.println("  on/off/read all   对所有已配置从站操作");
    CONSOLE_SERIAL.println("  help         显示本帮助");
    CONSOLE_SERIAL.print("  已配置从站: ");
    for (uint8_t i = 0; i < SLAVE_COUNT; i++) {
        CONSOLE_SERIAL.printf("0x%02X ", SLAVE_ADDRS[i]);
    }
    CONSOLE_SERIAL.println();
    CONSOLE_SERIAL.println("==============================");
}

// 发送一帧命令（自动收发模块内部切换方向，主站直接写串口即可）
bool sendCommand(uint8_t addr, uint8_t cmd, const uint8_t *data, uint8_t len) {
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t total = buildFrame(frame, addr, cmd, data, len);
    size_t written = RS485_SERIAL.write(frame, total);
    RS485_SERIAL.flush();
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
            delay(1);
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

        // 读满 5 字节头部后确定整帧长度
        if (idx == 5) {
            if (frame[4] > FRAME_DATA_MAX) { idx = 0; continue; }
            totalLen = 5 + frame[4] + 1;
        }

        // 收满一帧，做校验与匹配
        if (totalLen && idx == totalLen) {
            if (!verifyFrame(frame, totalLen) ||
                frame[2] != addr || frame[3] != expectCmd) {
                idx = 0; totalLen = 0;
                continue;
            }

            uint8_t dataLen = frame[4];
            if (data && len) {
                *len = dataLen;
                for (uint8_t i = 0; i < dataLen; i++) {
                    data[i] = frame[5 + i];
                }
            }
            return true;
        }

        if (idx >= FRAME_MAX_LEN) { idx = 0; totalLen = 0; }
    }

    return false;
}
