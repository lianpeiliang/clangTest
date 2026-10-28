#ifndef MINIMALMSGHEADER_H
#define MINIMALMSGHEADER_H

// 最小对比样例：只保留两个字段，使用默认对齐。
struct MsgHeader
{
    unsigned char msgType; // 消息类型：当前目标中占 1 字节，偏移 0。
    unsigned short msgId;  // 消息编号：当前目标中占 2 字节，偏移 2；前面留出 1 字节对齐填充。
};

// 当前目标环境下：结构体大小 4 字节，对齐 2 字节；可以用来观察默认对齐的效果。
#endif
