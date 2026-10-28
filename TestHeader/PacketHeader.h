#ifndef PACKETHEADER_H
#define PACKETHEADER_H

// 在界面中打开本文件，自动识别并加载唯一的 PacketHeader。
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
