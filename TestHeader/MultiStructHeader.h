#ifndef MULTISTRUCTHEADER_H
#define MULTISTRUCTHEADER_H

// 自动发现样例：打开后应列出 20 个结构体，无需输入名称。
// 每个结构体都有一个序列号，另一个字段用于对比不同类型；unsigned 是 unsigned int 的简写。
// 使用默认对齐，可以同时观察不同字段类型带来的结构体大小和填充变化。
struct Packet01
{
    unsigned sequence;   // 序列号示例。
    unsigned char flags; // 标志数值示例，8 位无符号整数。
};

struct Packet02
{
    unsigned sequence;     // 序列号示例。
    unsigned short length; // 长度数值示例，无符号短整数。
};

struct Packet03
{
    unsigned sequence; // 序列号示例。
    int value;         // 有符号整数值示例，可输入负数。
};

struct Packet04
{
    unsigned sequence; // 序列号示例。
    float value;       // 单精度浮点值示例。
};

struct Packet05
{
    unsigned sequence; // 序列号示例。
    double value;      // 双精度浮点值示例。
};

struct Packet06
{
    unsigned sequence; // 序列号示例。
    bool enabled;      // 是否启用，界面选择 false 或 true。
};

struct Packet07
{
    unsigned sequence;     // 序列号示例。
    unsigned char data[8]; // 8 个字节的数组；当前只读，二进制预览中数组区域补 0。
};

struct Packet08
{
    unsigned sequence;   // 序列号示例。
    unsigned short code; // 编号数值示例，无符号短整数。
};

struct Packet09
{
    unsigned sequence; // 序列号示例。
    unsigned status;   // 状态数值示例，无符号整数。
};

struct Packet10
{
    unsigned sequence;   // 序列号示例。
    long long timestamp; // 时间戳数值示例，用于观察 long long 类型的大小和对齐。
};

struct Packet11
{
    unsigned sequence; // 序列号示例。
    short temperature; // 温度数值示例，有符号短整数，可表示负值。
};

struct Packet12
{
    unsigned sequence;      // 序列号示例。
    unsigned short voltage; // 电压数值示例，无符号短整数。
};

struct Packet13
{
    unsigned sequence; // 序列号示例。
    int current;       // 电流数值示例，有符号整数。
};

struct Packet14
{
    unsigned sequence; // 序列号示例。
    float pressure;    // 压力数值示例，单精度浮点数。
};

struct Packet15
{
    unsigned sequence; // 序列号示例。
    double distance;   // 距离数值示例，双精度浮点数。
};

struct Packet16
{
    unsigned sequence; // 序列号示例。
    bool active;       // 是否处于活动状态，布尔值。
};

struct Packet17
{
    unsigned sequence;      // 序列号示例。
    unsigned char data[16]; // 16 个字节的数组，用于与 Packet07 的数组大小对比。
};

struct Packet18
{
    unsigned sequence;      // 序列号示例。
    unsigned short channel; // 通道编号示例，无符号短整数。
};

struct Packet19
{
    unsigned sequence; // 序列号示例。
    unsigned counter;  // 计数值示例，无符号整数。
};

struct Packet20
{
    unsigned sequence;        // 序列号示例。
    unsigned long long total; // 累计值示例，可用来观察 64 位无符号整数。
};

#endif
