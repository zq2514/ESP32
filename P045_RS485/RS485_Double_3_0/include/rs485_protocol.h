#ifndef RS485_PROTOCOL_H
#define RS485_PROTOCOL_H

#include <Arduino.h>

/*
 * ============================================================
 *  RS485 自定义简单帧协议（主站 / 从站 共用）
 * ============================================================
 *
 *  【为什么需要"协议"？】
 *  RS485 本质上只是一根"差分电线"，它只负责把 0/1 电平从一端传到
 *  另一端，并不知道这一串字节是什么意思。协议就是主从双方事先约定好的
 *  "说话格式"：每个字节放在什么位置、代表什么含义、怎么校验对错。
 *  主站按这个格式"打包"发送，从站按同一格式"拆包"理解，才能正确沟通。
 *
 *  【一帧数据长什么样？】（按字节顺序从左到右发送）
 *
 *  +--------+--------+--------+--------+--------+----------------+-----------+
 *  | SOI1   | SOI2   | ADDR   | CMD    | LEN    | DATA[0..LEN-1] | CHECKSUM  |
 *  | 0xA5   | 0x5A   | 1 字节 | 1 字节 | 1 字节 | LEN 字节       | 1 字节    |
 *  +--------+--------+--------+--------+--------+----------------+-----------+
 *
 *  各字段含义：
 *  - SOI1 / SOI2：帧起始标识（固定 0xA5 0x5A）。
 *      作用：接收端靠它"对齐"一帧的起点。总线上可能混有噪声、半截数据，
 *      接收端会先找这两个字节，找到后才认为后面是一帧的开头。
 *  - ADDR：从站地址（0x01 ~ 0xFE），0xFF 为广播地址。
 *      作用：一条总线上挂多个从站，靠地址区分"这帧是发给谁的"。
 *  - CMD：命令字。
 *      作用：告诉从站"要做什么"。从站应答时 = 请求命令字 | RESP_FLAG(0x80)。
 *  - LEN：DATA 字段的字节数（0 ~ FRAME_DATA_MAX）。
 *      作用：接收端靠它知道后面还要收几个字节才算收完一帧。
 *  - DATA：具体数据（共 LEN 字节），可以没有（LEN=0）。
 *  - CHECKSUM：校验和，从 SOI1 累加到 DATA 最后一个字节，取低 8 位。
 *      作用：简单校验数据在传输中是否出错。
 *
 *  完整帧长度 = 5 + LEN + 1（帧头2 + 地址1 + 命令1 + 长度1 + 数据 + 校验1）
 */

// ---- 帧起始标识 ----
// 为什么选 0xA5 0x5A？这两个值落在 ASCII 常见字符范围（0x20~0x7E）之外，
// 不容易和总线上的普通文本/调试输出混淆，便于接收端在噪声里"认出"帧头。
#define FRAME_HEAD1             0xA5
#define FRAME_HEAD2             0x5A

// ---- 从站地址 ----
#define SLAVE_ADDR_BROADCAST    0xFF   // 广播地址：所有从站都接收（当前未使用）

// ---- 命令字 ----
#define CMD_WRITE_LED           0x01   // 写 LED：DATA[0] = 0x00 关 / 0x01 开
#define CMD_READ_LED            0x02   // 读 LED：无数据；应答 DATA[0] = 当前状态

// ---- 应答标志 ----
// 从站应答帧的命令字 = 请求命令字 | RESP_FLAG，即把最高位(bit7)置 1。
// 例：主站发 0x01(写) -> 从站回 0x81(写应答)；主站发 0x02(读) -> 从站回 0x82(读应答)。
// 这样主站能区分"自己发出去的请求"和"从站回过来的应答"，不会把请求当应答。
#define RESP_FLAG               0x80

// ---- LED 状态定义 ----
#define LED_OFF                 0x00
#define LED_ON                  0x01

// ---- 帧参数 ----
#define FRAME_DATA_MAX          8                         // DATA 字段最大长度
#define FRAME_MAX_LEN           (FRAME_DATA_MAX + 6)      // 完整帧最大长度 = 14

/*
 * 计算校验和：把 frame[0..len-1] 逐字节累加，返回低 8 位。
 * 为什么用 uint8_t 累加？因为它只有 8 位，加法结果超过 255 时
 * 会自动丢弃高位（取模 256），正好就是"取低 8 位"，无需显式写 %256。
 */
inline uint8_t calcChecksum(const uint8_t *frame, uint8_t len) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += frame[i];
    }
    return sum;
}

/*
 * 组装一帧：把各个字段填进 frame 缓冲区，并自动算好校验和。
 * 参数：frame = 输出缓冲区（需至少 FRAME_MAX_LEN 字节）；addr/cmd/data/len = 帧内容。
 * 返回：整帧总长度（= 5 + len + 1）。
 */
inline uint8_t buildFrame(uint8_t *frame, uint8_t addr, uint8_t cmd,
                          const uint8_t *data, uint8_t len) {
    frame[0] = FRAME_HEAD1;      // 帧头
    frame[1] = FRAME_HEAD2;
    frame[2] = addr;             // 地址
    frame[3] = cmd;              // 命令
    frame[4] = len;              // 数据长度
    for (uint8_t i = 0; i < len; i++) {
        frame[5 + i] = data[i];  // 数据
    }
    uint8_t total = 5 + len;                 // 校验和之前的字节数（下标 0..total-1）
    frame[total] = calcChecksum(frame, total); // 最后填校验和（不含校验字节自身）
    return total + 1;
}

/*
 * 校验一帧是否合法：帧头正确、长度在范围内、校验和匹配。
 * 返回 true/false。接收端收满一帧后调用它判断这帧"能不能用"。
 */
inline bool verifyFrame(const uint8_t *frame, uint8_t totalLen) {
    if (totalLen < 6) return false;                       // 最短一帧也需 头2+地址1+命令1+长度1+校验1
    if (frame[0] != FRAME_HEAD1 || frame[1] != FRAME_HEAD2) return false; // 帧头不对
    uint8_t len = frame[4];
    if (len > FRAME_DATA_MAX) return false;               // 声明长度超出上限
    if (totalLen != (uint8_t)(5 + len + 1)) return false; // 实际长度与声明不符
    return frame[totalLen - 1] == calcChecksum(frame, totalLen - 1); // 校验和比对
}

#endif // RS485_PROTOCOL_H
