#ifndef BINARYDECODER_H
#define BINARYDECODER_H

#include "binaryencoder.h"

// 小端：低位字节在前，例如 0x1234 存为 34 12；大端则存为 12 34。
enum class BinaryByteOrder { LittleEndian, BigEndian };

// 解码后的一个字段，同时保留原始字节、显示文本和嵌套结果，供界面展示。
struct DecodedField
{
    QString name;
    QString path;
    QString typeName;
    qint64 offsetBits = 0;
    qint64 widthBits = 0;
    QByteArray bytes; // 位域保留覆盖它的完整字节，值按位偏移提取。
    QString value;
    QString note;
    QVector<DecodedField> children;
};

struct DecodeResult
{
    bool success = false;
    QVector<DecodedField> fields;
    QHash<QString, QString> editableValues; // 仅收集通过校验、允许填回主窗口的值。
    QStringList diagnostics;
};

// 严格校验，不使用 QByteArray::fromHex 对非法字符的静默忽略行为。
// 支持连续字节、空白/逗号分隔的字节，以及逐字节的 0x 前缀。
BinaryResult parseHexBytes(const QString &text);
// 按已有布局解释字节；字节序选项改变字段数值的解释方式，不重新计算字段位置。
DecodeResult decodeStructure(const StructInfo &structure, const QByteArray &bytes,
                             BinaryByteOrder order = BinaryByteOrder::LittleEndian);

#endif
