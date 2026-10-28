#ifndef MINIMALMSGHEADER_H
#define MINIMALMSGHEADER_H

// 最小对比样例：只保留两个字段，使用默认对齐。
struct MsgHeader
{
    unsigned char msgType; // 消息类型。
    unsigned short msgId; // 消息编号。
};

#endif
