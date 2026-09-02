#ifndef RS485_PROTOCOL_H
#define RS485_PROTOCOL_H

#include <Arduino.h>

/*
 * ============================================================
 *  RS485 自定义简单帧协议（主站 / 从站 共用）
 * ============================================================
 *
 *  一帧格式（按字节顺序发送）：
 *  +--------+--------+--------+--------+--------+----------------+-----------+
 *  | SOI1   | SOI2   | ADDR   | CMD    | LEN    | DATA[0..LEN-1] | CHECKSUM  |
 *  | 0xA5   | 0x5A   | 1 字节 | 1 字节 | 1 字节 | LEN 字节       | 1 字节    |
 *  +--------+--------+--------+--------+--------+----------------+-----------+
 *
 *  - SOI1 / SOI2 : 帧起始标识，用于接收端同步
 *  - ADDR        : 从站地址（0x01 ~ 0xFE），0xFF 为广播地址
 *  - CMD         : 命令字；从站应答时 = 请求命令字 | RESP_FLAG(0x80)
 *  - LEN         : DATA 字段的字节数（0 ~ FRAME_DATA_MAX）
 *  - DATA        : 数据内容
 *  - CHECKSUM    : 从 SOI1 累加到 DATA 最后一个字节，取低 8 位
 *
 *  完整帧长度 = 5 + LEN + 1（头2 + 地址1 + 命令1 + 长度1 + 数据 + 校验1）
 */

// ---- 帧起始标识 ----
#define FRAME_HEAD1             0xA5
#define FRAME_HEAD2             0x5A

// ---- 从站地址 ----
#define SLAVE_ADDR_BROADCAST    0xFF   // 广播地址

// ---- 命令字 ----
#define CMD_WRITE_LED           0x01   // 写 LED：DATA[0] = 0x00 关 / 0x01 开
#define CMD_READ_LED            0x02   // 读 LED：无数据；应答 DATA[0] = 当前状态

// ---- 应答标志（从站应答帧的命令字 = 请求命令字 | RESP_FLAG）----
#define RESP_FLAG               0x80

// ---- LED 状态定义 ----
#define LED_OFF                 0x00
#define LED_ON                  0x01

// ---- 帧参数 ----
#define FRAME_DATA_MAX          8                         // DATA 字段最大长度
#define FRAME_MAX_LEN           (FRAME_DATA_MAX + 6)      // 完整帧最大长度 = 14

// 计算校验和：累加 frame[0..len-1]，返回低 8 位
inline uint8_t calcChecksum(const uint8_t *frame, uint8_t len) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += frame[i];
    }
    return sum;
}

// 组装一帧（frame 缓冲区需足够大，至少 FRAME_MAX_LEN 字节），返回帧总长度
inline uint8_t buildFrame(uint8_t *frame, uint8_t addr, uint8_t cmd,
                          const uint8_t *data, uint8_t len) {
    frame[0] = FRAME_HEAD1;
    frame[1] = FRAME_HEAD2;
    frame[2] = addr;
    frame[3] = cmd;
    frame[4] = len;
    for (uint8_t i = 0; i < len; i++) {
        frame[5 + i] = data[i];
    }
    uint8_t total = 5 + len;                 // 校验和之前的字节数
    frame[total] = calcChecksum(frame, total);
    return total + 1;
}

// 校验一帧是否合法（帧头、长度、校验和），返回 true/false
inline bool verifyFrame(const uint8_t *frame, uint8_t totalLen) {
    if (totalLen < 6) return false;                       // 至少 头2+地址1+命令1+长度1+校验1
    if (frame[0] != FRAME_HEAD1 || frame[1] != FRAME_HEAD2) return false;
    uint8_t len = frame[4];
    if (len > FRAME_DATA_MAX) return false;
    if (totalLen != (uint8_t)(5 + len + 1)) return false;
    return frame[totalLen - 1] == calcChecksum(frame, totalLen - 1);
}

#endif // RS485_PROTOCOL_H
