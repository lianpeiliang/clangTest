#include <cstdint>

// 保存原有打包设置，将成员对齐上限设为 1 字节，避免成员之间的填充。
#pragma pack(push, 1)

struct MsgHeader
{
    // 原有的固定宽度整数，用来和下面的基础类型对比。
    uint16_t msgId;
    uint8_t msgType;
    uint32_t dataLength;

    // 字符与布尔类型。char 的有无符号由目标平台决定，因此单独列出 signed/unsigned char。
    char charValue;
    signed char signedCharValue;
    unsigned char unsignedCharValue;
    bool boolValue;

    // 各种有符号和无符号整数；short、int、long 默认都是有符号的。
    short shortValue;
    unsigned short unsignedShortValue;
    int intValue;
    unsigned int unsignedIntValue;
    long longValue;
    unsigned long unsignedLongValue;
    long long longLongValue;
    unsigned long long unsignedLongLongValue;

    // 浮点类型；long double 的大小可能随目标平台和工具链变化。
    float floatValue;
    double doubleValue;
    long double longDoubleValue;

    // 宽字符与 C++11 字符类型；这里只存放单个字符，不是字符串。
    wchar_t wcharValue;
    char16_t char16Value;
    char32_t char32Value;
};

// 恢复之前的打包设置，避免影响后续声明的结构体。
#pragma pack(pop)
