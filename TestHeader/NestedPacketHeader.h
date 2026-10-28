#ifndef NESTEDPACKETHEADER_H
#define NESTEDPACKETHEADER_H

// 嵌套结构体样例：打开后自动发现两个结构体，下拉选择 NestedPacketHeader。
// 也可选择 PacketAddress，单独查看地址结构体的字段。
#pragma pack(push, 1)

struct PacketAddress
{
    unsigned short source;      // 源地址：2 字节，偏移 0。
    unsigned short destination; // 目标地址：2 字节，偏移 2。
};

struct NestedPacketHeader
{
    unsigned int sequence;        // 序列号：4 字节，偏移 0。
    PacketAddress address;        // 嵌套成员：4 字节，偏移 4。
    unsigned short payloadLength; // 负载长度：2 字节，偏移 8。
};

#pragma pack(pop)

// 当前目标环境下：PacketAddress 大小 4 字节，NestedPacketHeader 大小 10 字节。
// 两个结构体均按 1 字节对齐；内部字段的偏移相对于各自所属结构体的起点。
// 在树形界面展开 address，可查看 source 和 destination。
// destination 的相对偏移为 2 字节，相对最外层结构体的绝对偏移为 6 字节。
#endif
