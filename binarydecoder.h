#ifndef BINARYDECODER_H
#define BINARYDECODER_H

#include "binaryencoder.h"

// 小端：低位字节在前，例如 0x1234 存为 34 12；大端则存为 12 34。
enum class BinaryByteOrder
{
    LittleEndian, // 小端解释：先读到的字节保存数值低位，与本程序当前布局目标一致。
    BigEndian     // 大端解释：先读到的字节保存数值高位；当前暂不支持大端位域和 long double。
};

// 解码后的一个字段，同时保留原始字节、显示文本和嵌套结果，供界面展示。
struct DecodedField
{
    QString name;                  // 当前字段名，例如 source。
    QString path;                  // 从最外层结构体成员开始的完整路径，例如 address.source。
    QString typeName;              // 声明中的字段类型名称，用于界面显示。
    qint64 offsetBits = 0;          // 相对最外层结构体起点的绝对偏移，单位 bit。
    qint64 widthBits = 0;           // 字段占用的位数；普通字段为大小乘 8，位域为声明宽度。
    QByteArray bytes;              // 覆盖字段的原始字节；位域可能与相邻字段共享其中的部分位。
    QString value;                 // 显示用的解析值；分组、只读字段也可能显示“分组”或“原始字节”。
    QString note;                  // 补充说明，如数组仅显示字节、非规范 bool 或数值不能填回；无说明时为空。
    QVector<DecodedField> children; // 嵌套记录的解码结果；普通叶子字段没有子项。
};

// 整个结构体的解码结果：fields 用于展示，editableValues 用于点击按钮后填回编辑器。
struct DecodeResult
{
    bool success = false;                 // 是否完成布局检查和解码；失败时 fields 和 editableValues 会清空。
    QVector<DecodedField> fields;         // 按成员顺序保存的结果树，保留嵌套关系及原始字节。
    QHash<QString, QString> editableValues; // 字段完整路径 → 可填回的值；只收集通过校验的可编辑字段。
    QStringList diagnostics;              // 解码失败原因，可包含出错字段的路径。
};

// 严格校验，不使用 QByteArray::fromHex 对非法字符的静默忽略行为。
// text 支持连续字节、空白/逗号分隔以及 0x 前缀，例如 "1234"、"12 34"、"0x12, 0x34"。
// 成功时返回原始字节 0x12、0x34；空输入、非法字符或不完整字节会通过 diagnostics 报错。
BinaryResult parseHexBytes(const QString &text);

// 按已有布局解释字节；字节序选项改变字段数值的解释方式，不重新计算字段位置。
// structure 提供类型和布局；bytes 长度必须与结构体大小一致，包含填充；order 默认小端序。
// 只生成解码结果，不直接修改界面；调用方决定是否使用 editableValues 填回字段。
DecodeResult decodeStructure(const StructInfo &structure, const QByteArray &bytes,
                             BinaryByteOrder order = BinaryByteOrder::LittleEndian);

#endif
