#include <cstdint>

// 基础类型布局样例：使用固定宽度整数和常见 C++ 类型，观察它们的大小、对齐及字段偏移。
// 保存原有打包设置，将成员对齐上限设为 1 字节，避免成员之间的填充。
#pragma pack(push, 1)

struct MsgHeader
{
    // 原有的固定宽度整数，用来和下面的基础类型对比。
    uint16_t msgId;       // 消息编号示例，16 位无符号整数。
    uint8_t msgType;      // 消息类型示例，8 位无符号整数。
    uint32_t dataLength;  // 数据长度示例，32 位无符号整数。

    // 字符与布尔类型。char 的有无符号由目标平台决定，因此单独列出 signed/unsigned char。
    char charValue;                   // 普通 char 样例，编辑器按字符的数值编码填写。
    signed char signedCharValue;      // 显式有符号字符整数，可表示负数。
    unsigned char unsignedCharValue;  // 显式无符号字符整数，只表示非负数。
    bool boolValue;                   // 布尔值样例，界面选择 false 或 true。

    // 各种有符号和无符号整数；short、int、long 默认都是有符号的。
    short shortValue;                       // 有符号短整数样例。
    unsigned short unsignedShortValue;      // 无符号短整数样例。
    int intValue;                           // 有符号 int 整数样例。
    unsigned int unsignedIntValue;          // 无符号 int 整数样例。
    long longValue;                         // 有符号 long 整数样例；long 不保证比 int 占用更多字节。
    unsigned long unsignedLongValue;        // 无符号 long 整数样例。
    long long longLongValue;                // 有符号 long long 整数样例，可用来观察 64 位整数范围。
    unsigned long long unsignedLongLongValue; // 无符号 long long 整数样例，可表示完整的 64 位无符号值。

    // 浮点类型；long double 的大小可能随目标平台和工具链变化。
    float floatValue;            // 单精度浮点样例，可输入小数和科学计数法。
    double doubleValue;          // 双精度浮点样例，通常比 float 的精度更高。
    long double longDoubleValue; // long double 样例，用来观察扩展浮点格式的目标布局。

    // 宽字符与 C++11 字符类型；这里只存放单个字符，不是字符串。
    wchar_t wcharValue;   // 宽字符类型样例，大小由目标平台决定。
    char16_t char16Value; // 用于 UTF-16 编码单元的字符类型样例。
    char32_t char32Value; // 用于 UTF-32 编码单元的字符类型样例。
};

// 恢复之前的打包设置，避免影响后续声明的结构体。
#pragma pack(pop)
