#ifndef BINARYENCODER_H
#define BINARYENCODER_H

#include "headerparser.h"
#include <QByteArray>
#include <QHash>

struct BinaryResult
{
    bool success = false;
    QByteArray bytes;
    QStringList diagnostics;
};

// 按当前固定的 Windows x64 / MinGW 布局编码；小端序，填充字节补 0。
BinaryResult encodeStructure(const StructInfo &structure, const QHash<QString, QString> &values);
QString formatHexDump(const QByteArray &bytes);

#endif
