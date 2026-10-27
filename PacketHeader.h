#ifndef PACKETHEADER_H
#define PACKETHEADER_H

// 在界面中打开本文件，将“结构体名称”改为 PacketHeader 后解析。
#pragma pack(push, 1)

struct PacketHeader
{
    unsigned int sequence;        // 序列号：4 字节，偏移 0。
    unsigned short payloadLength; // 负载长度：2 字节，偏移 4。
    unsigned char flags;          // 标志位：1 字节，偏移 6。
};

#pragma pack(pop)

// 当前目标环境下：总大小 7 字节，结构体对齐 1 字节。
#endif
