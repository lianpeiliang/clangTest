#ifndef BINARYDECODER_H
#define BINARYDECODER_H

#include "binaryencoder.h"

enum class BinaryByteOrder { LittleEndian, BigEndian };

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
    QHash<QString, QString> editableValues;
    QStringList diagnostics;
};

// 严格校验，不使用 QByteArray::fromHex 对非法字符的静默忽略行为。
// 支持连续字节、空白/逗号分隔的字节，以及逐字节的 0x 前缀。
BinaryResult parseHexBytes(const QString &text);
DecodeResult decodeStructure(const StructInfo &structure, const QByteArray &bytes,
                             BinaryByteOrder order = BinaryByteOrder::LittleEndian);

#endif
