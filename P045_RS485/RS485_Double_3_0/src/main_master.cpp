#include <Arduino.h>
#include "rs485_protocol.h"

/*
 * ============================================================
 *  RS485 主站程序（Master）—— 串口命令手动控制从站 LED
 * ============================================================
 *
 *  【主站的职责】
 *  主站是总线上"主动发问"的一方：它把用户在串口监视器里输入的
 *  命令，翻译成一帧数据发到 RS485 总线，然后等待对应从站回话。
 *  一条总线上只有一个主站（避免大家同时说话打架）。
 *
 *  【硬件连接】（自动收发 TTL <-> RS485 模块）
 *    ESP32 3V3           -> 模块 VCC
 *    ESP32 GND           -> 模块 GND
 *    ESP32 GPIO17 (TX2)  -> 模块 RX / DI   （ESP32 发送 -> 模块 DI）
 *    ESP32 GPIO16 (RX2)  -> 模块 RO / TX   （模块 RO -> ESP32 接收）
 *
 *  【串口分配】—— 为什么分两个串口？
 *  如果 RS485 和 USB 控制台共用 UART0，那么"你在监视器打的字"和
 *  "从站回传的数据"会挤在同一个接收引脚上互相干扰。所以把 RS485 单独
 *  放到 Serial2（GPIO16/17），UART0 专门留给 USB 控制台，互不打扰。
 *    Serial2 (GPIO16/17) = RS485 总线，波特率 9600
 *    Serial  (USB/UART0) = 控制台，波特率 115200
 *
 *  【控制命令】（在串口监视器输入，回车发送）
 *    on  <addr>    开灯，如 on 1 / on 0x02
 *    off <addr>    关灯，如 off 1
 *    read <addr>   查询 LED 状态
 *    on/off/read all    对所有已配置从站操作
 *    help          显示帮助
 *
 *  从站地址在下方 SLAVE_ADDRS[] 中维护，新增从站在这里加地址即可。
 * ============================================================
 */

// ---- RS485 串口（Serial2）----
#define RS485_SERIAL    Serial2
#define RS485_BAUD      9600        // RS485 常用波特率；主从必须一致
#define RS485_RX_PIN    16          // ESP32 接收脚（接模块 RO/TX）
#define RS485_TX_PIN    17          // ESP32 发送脚（接模块 DI/RX）

// ---- 控制台串口（USB / UART0）----
#define CONSOLE_SERIAL  Serial
#define CONSOLE_BAUD    115200      // 串口监视器波特率要选成 115200

// ---- 通信参数 ----
#define RESP_TIMEOUT_MS 200         // 等从站应答的超时时间；超过就认为"无应答"

// ---- 从站地址列表（新增从站：在这里加一个地址即可）----
const uint8_t SLAVE_ADDRS[] = { 0x01, 0x02 };
const uint8_t SLAVE_COUNT = sizeof(SLAVE_ADDRS) / sizeof(SLAVE_ADDRS[0]);  // 自动算出数组长度

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

/*
 * setup()：上电只执行一次，做三件事——
 * 1. 初始化两个串口（控制台 + RS485）；
 * 2. 打印启动信息；
 * 3. 打印帮助和提示符，告诉用户现在可以输入命令了。
 */
void setup() {
    CONSOLE_SERIAL.begin(CONSOLE_BAUD);
    // Serial2.begin(波特率, 数据格式, 接收脚, 发送脚)：显式指定 GPIO16/17
    RS485_SERIAL.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    CONSOLE_SERIAL.println("[Master] RS485 master started.");
    CONSOLE_SERIAL.printf("[Master] RS485 baud=%d, console baud=%d\r\n",
                          RS485_BAUD, CONSOLE_BAUD);
    printHelp();
    CONSOLE_SERIAL.print("> ");     // 提示符，提示"可以输入了"
}

/*
 * loop()：反复执行。这里只做一件事——检查控制台有没有收到用户输入。
 * 如果用户敲了一行并回车，就把这行文字交给 handleLine() 去解析执行。
 * 平时没有输入时 loop() 几乎什么也不做，不占 CPU。
 */
void loop() {
    if (CONSOLE_SERIAL.available()) {
        // readStringUntil('\n')：把"从当前位置到换行符之前"的字符读成一个字符串
        String line = CONSOLE_SERIAL.readStringUntil('\n');
        handleLine(line);
        CONSOLE_SERIAL.print("> "); // 处理完再显示一次提示符
    }
}

/*
 * 解析一行用户命令，例如 "on 1"、"off all"、"read 0x02"、"help"。
 * 思路：把整行拆成「命令」和「参数」两部分，再按命令分派到对应处理。
 */
void handleLine(String &line) {
    line.trim();                        // 去掉首尾空格和回车('\r')
    if (line.length() == 0) return;     // 空行，直接忽略

    // 按第一个空格把 "on 1" 拆成 cmd="on"、arg="1"
    int sp = line.indexOf(' ');
    String cmd = (sp < 0) ? line : line.substring(0, sp);   // 没有空格则整行都是命令
    String arg = (sp < 0) ? "" : line.substring(sp + 1);
    cmd.toLowerCase();                  // 转小写，使 ON/On 都能识别
    arg.trim();
    arg.toLowerCase();

    if (cmd == "help" || cmd == "?") {
        printHelp();
        return;
    }

    if (cmd == "on" || cmd == "off") {
        bool on = (cmd == "on");
        if (arg == "all") {             // 对"所有已配置从站"操作
            for (uint8_t i = 0; i < SLAVE_COUNT; i++) {
                doWrite(SLAVE_ADDRS[i], on);
            }
        } else {
            uint8_t addr;
            if (!parseAddr(arg, addr)) {   // 参数不是合法地址
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

/*
 * 把字符串解析成从站地址。
 * 支持两种写法：十进制（"1"、"2"），以及 0x 开头的十六进制（"0x02"）。
 * 合法范围 1~0xFE。解析失败返回 false。
 */
bool parseAddr(const String &s, uint8_t &addr) {
    if (s.length() == 0) return false;

    int base = 10;                       // 默认十进制
    const char *p = s.c_str();
    if (s.length() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;                       // 发现 0x 前缀，按十六进制
        p += 2;
    }

    char *end;
    long v = strtol(p, &end, base);      // 标准库函数：字符串转整数
    if (end == p || *end != '\0' || v <= 0 || v > 0xFE) return false;  // 解析失败/越界
    addr = (uint8_t)v;
    return true;
}

// 对一块从站执行"写 LED"，并把结果打印到控制台
void doWrite(uint8_t addr, bool on) {
    CONSOLE_SERIAL.printf("  -> set slave 0x%02X LED %s ... ", addr, on ? "ON" : "OFF");
    if (writeLed(addr, on)) {
        CONSOLE_SERIAL.println("OK");
    } else {
        CONSOLE_SERIAL.println("no response");
    }
}

// 对一块从站执行"读 LED"，并把结果打印到控制台
void doRead(uint8_t addr) {
    bool on;
    if (readLed(addr, on)) {
        CONSOLE_SERIAL.printf("  slave 0x%02X LED = %s\r\n", addr, on ? "ON" : "OFF");
    } else {
        CONSOLE_SERIAL.printf("  slave 0x%02X: no response\r\n", addr);
    }
}

// 写 LED：发一条 CMD_WRITE_LED 命令，并等从站应答。返回是否成功。
bool writeLed(uint8_t addr, bool on) {
    uint8_t data[1] = { (uint8_t)(on ? LED_ON : LED_OFF) };  // 数据 = 0x01 开 / 0x00 关
    if (!sendCommand(addr, CMD_WRITE_LED, data, 1)) return false;   // 发送失败

    uint8_t rsp[FRAME_DATA_MAX];
    uint8_t rspLen = 0;
    return recvResponse(addr, CMD_WRITE_LED | RESP_FLAG, rsp, &rspLen, RESP_TIMEOUT_MS);
}

// 读 LED：发一条 CMD_READ_LED 命令，把收到的状态写回 on。返回是否成功。
bool readLed(uint8_t addr, bool &on) {
    if (!sendCommand(addr, CMD_READ_LED, nullptr, 0)) return false; // 读命令无数据，len=0

    uint8_t rsp[FRAME_DATA_MAX];
    uint8_t rspLen = 0;
    if (!recvResponse(addr, CMD_READ_LED | RESP_FLAG, rsp, &rspLen, RESP_TIMEOUT_MS)) {
        return false;
    }
    on = (rspLen > 0 && rsp[0] == LED_ON);   // 应答第 1 字节就是状态
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

/*
 * 发送一帧命令。
 * 自动收发模块内部会自己切换"发/收"方向，所以主站只需把字节写到串口即可，
 * 不需要像普通 MAX485 那样手动控制 DE/RE 引脚。
 * flush()：等所有字节真正从 UART 发出去（否则函数一返回，后续立即读应答，
 * 数据可能还没发完，导致从站收不完整）。
 */
bool sendCommand(uint8_t addr, uint8_t cmd, const uint8_t *data, uint8_t len) {
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t total = buildFrame(frame, addr, cmd, data, len);
    size_t written = RS485_SERIAL.write(frame, total);
    RS485_SERIAL.flush();
    return written == total;   // 校验"实际写出字节数"等于"应发字节数"
}

/*
 * 接收一帧应答——这是整个程序里最核心的一段，按"状态机"逐字节解析。
 *
 * 因为串口是"一个字节一个字节"到达的，而且可能夹着噪声，所以不能
 * 简单读固定长度，而是边读边判断：
 *
 *   1. 先找帧头：读到的字节必须是 0xA5，下一个是 0x5A，否则丢弃重来；
 *   2. 收够 5 字节头部后，从第 5 字节(LEN)算出整帧应有多长；
 *   3. 收满整帧后，用 verifyFrame() 校验（帧头/长度/校验和）；
 *   4. 再核对"地址"和"命令字"是否正是我们等的那个应答；
 *   5. 全程都在 timeoutMs 时间内，超时即返回失败。
 */
bool recvResponse(uint8_t addr, uint8_t expectCmd, uint8_t *data, uint8_t *len,
                  uint32_t timeoutMs) {
    uint8_t frame[FRAME_MAX_LEN];
    uint8_t idx      = 0;       // 当前已收进 frame 的字节数
    uint8_t totalLen = 0;       // 整帧应有的总长度（读到 LEN 后才确定）
    uint32_t start   = millis();

    while (millis() - start < timeoutMs) {
        if (!RS485_SERIAL.available()) {
            delay(1);           // 暂时没数据，睡 1ms 再查，避免死循环空转
            continue;
        }

        uint8_t b = RS485_SERIAL.read();

        // ---- 第 1 步：帧头同步 ----
        if (idx == 0) {                     // 等待第一个帧头字节 0xA5
            if (b != FRAME_HEAD1) continue; // 不是 0xA5，丢弃，继续等
        } else if (idx == 1) {              // 等待第二个帧头字节 0x5A
            if (b != FRAME_HEAD2) { idx = 0; continue; } // 不是 0x5A，从头再来
        }

        frame[idx++] = b;                   // 存下这个字节

        // ---- 第 2 步：读到 LEN 字段后，算出整帧长度 ----
        if (idx == 5) {
            if (frame[4] > FRAME_DATA_MAX) { idx = 0; continue; } // LEN 非法，重新同步
            totalLen = 5 + frame[4] + 1;    // 头5 + 数据 + 校验1
        }

        // ---- 第 3 步：收满一帧，做校验与匹配 ----
        if (totalLen && idx == totalLen) {
            if (!verifyFrame(frame, totalLen) ||           // 校验失败
                frame[2] != addr || frame[3] != expectCmd) { // 地址/命令不匹配
                idx = 0; totalLen = 0;   // 丢弃这帧，重新同步等下一帧
                continue;
            }

            // 第 4 步：成功，把 DATA 部分拷贝给调用者
            uint8_t dataLen = frame[4];
            if (data && len) {
                *len = dataLen;
                for (uint8_t i = 0; i < dataLen; i++) {
                    data[i] = frame[5 + i];
                }
            }
            return true;
        }

        // 防御：万一 idx 越界（异常情况），复位避免数组溢出
        if (idx >= FRAME_MAX_LEN) { idx = 0; totalLen = 0; }
    }

    return false;   // 超时
}
