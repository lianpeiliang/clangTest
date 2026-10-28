#ifndef BINARYENCODER_H
#define BINARYENCODER_H

#include "headerparser.h"
#include <QByteArray>
#include <QHash>

// 字节处理结果：编码结构体和解析十六进制输入都使用这个类型。
// bytes 保存实际字节，不是十六进制文本；显示时再转换成字符串。
struct BinaryResult
{
    bool success = false;    // 此次字节处理是否成功；失败时先查看 diagnostics。
    QByteArray bytes;        // 成功时得到的原始字节；失败时为空，避免使用不完整数据。
    QStringList diagnostics; // 失败原因列表，例如字段值无效、布局不支持或十六进制格式错误。
};

// 按当前固定的 Windows x64 / MinGW 布局编码；小端序，填充字节补 0。
// structure 提供总大小和字段偏移；values 保存“字段完整路径 → 输入文字”，如 address.source → 0x1122。
// 每个可编辑字段都需有合法输入；数组区域补 0，不支持的字段或布局通过 diagnostics 报错。
BinaryResult encodeStructure(const StructInfo &structure, const QHash<QString, QString> &values);

// 只负责排版：每行最多 16 字节，左边是偏移，右边是十六进制字节。
// 例如两个字节 0x12、0x34 显示为 "00000000  12 34"；返回文字，不修改输入字节。
QString formatHexDump(const QByteArray &bytes);

#endif
