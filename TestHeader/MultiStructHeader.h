#ifndef MULTISTRUCTHEADER_H
#define MULTISTRUCTHEADER_H

// 自动发现样例：打开后应列出 20 个结构体，无需输入名称。
struct Packet01 { unsigned sequence; unsigned char flags; };
struct Packet02 { unsigned sequence; unsigned short length; };
struct Packet03 { unsigned sequence; int value; };
struct Packet04 { unsigned sequence; float value; };
struct Packet05 { unsigned sequence; double value; };
struct Packet06 { unsigned sequence; bool enabled; };
struct Packet07 { unsigned sequence; unsigned char data[8]; };
struct Packet08 { unsigned sequence; unsigned short code; };
struct Packet09 { unsigned sequence; unsigned status; };
struct Packet10 { unsigned sequence; long long timestamp; };
struct Packet11 { unsigned sequence; short temperature; };
struct Packet12 { unsigned sequence; unsigned short voltage; };
struct Packet13 { unsigned sequence; int current; };
struct Packet14 { unsigned sequence; float pressure; };
struct Packet15 { unsigned sequence; double distance; };
struct Packet16 { unsigned sequence; bool active; };
struct Packet17 { unsigned sequence; unsigned char data[16]; };
struct Packet18 { unsigned sequence; unsigned short channel; };
struct Packet19 { unsigned sequence; unsigned counter; };
struct Packet20 { unsigned sequence; unsigned long long total; };

#endif
