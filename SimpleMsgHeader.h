#ifndef SIMPLEMSGHEADER_H
#define SIMPLEMSGHEADER_H

// 简单对比样例：保留 MsgHeader 名称，可直接用现有界面单独打开解析。
// 与复杂样例一样使用 1 字节打包，便于对比字段数量和总大小。
#pragma pack(push, 1)

struct MsgHeader
{
    unsigned short msgId;   // 消息编号：2 字节，偏移 0。
    unsigned char msgType;  // 消息类型：1 字节，偏移 2。
    unsigned int dataLength; // 数据长度：4 字节，偏移 3。
};

#pragma pack(pop)

// 当前目标环境下：总大小 7 字节，结构体对齐 1 字节。
#endif
