#ifndef BINARYENCODER_H
#define BINARYENCODER_H

#include "headerparser.h"
#include <QByteArray>
#include <QHash>

// bytes 保存实际字节，不是十六进制文本；显示时再转换成字符串。
struct BinaryResult
{
    bool success = false;
    QByteArray bytes;
    QStringList diagnostics;
};

// 按当前固定的 Windows x64 / MinGW 布局编码；小端序，填充字节补 0。
BinaryResult encodeStructure(const StructInfo &structure, const QHash<QString, QString> &values);
// 只负责排版：每行最多 16 字节，左边是偏移，右边是十六进制字节。
QString formatHexDump(const QByteArray &bytes);

#endif
