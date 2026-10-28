#ifndef HEADERPARSER_H
#define HEADERPARSER_H

#include <QString>
#include <QStringList>
#include <QVector>

// 用普通 Qt 数据保存查询结果，界面无需持有 libclang 的 Cursor 或 Type。
enum class FieldValueKind
{
    ReadOnly, Record, SignedInteger, UnsignedInteger, Boolean, Float, Double, LongDouble
};

struct FieldInfo
{
    QString name;
    QString typeName;
    QString canonicalTypeName;
    qint64 size = -1;
    qint64 alignment = -1;
    qint64 offsetBits = -1;         // 相对直接所属结构体的偏移。
    qint64 absoluteOffsetBits = -1; // 相对最外层结构体的偏移。
    QVector<FieldInfo> children;   // 按值嵌套的记录类型成员；指针、引用、数组不展开。
    FieldValueKind valueKind = FieldValueKind::ReadOnly;
    int valueBits = 0;             // 整数的有效位数；位域使用声明的宽度。
    QString readOnlyReason;
    bool isBitField = false;
    bool isArray = false; // 数组保持只读，二进制预览将整个数组占用的字节补 0。
    QString binaryEncodingIssue;
};

struct StructInfo
{
    QString name;
    qint64 size = -1;
    qint64 alignment = -1;
    QVector<FieldInfo> fields;
    QString binaryEncodingIssue;
};

struct ParseResult
{
    bool success = false;
    StructInfo structure;
    QStringList diagnostics;
};

struct HeaderParseResult
{
    bool success = false; // 文件解析成功且发现至少一个结构体；各结构体的布局状态单独保存。
    QVector<ParseResult> structures; // 按声明顺序，名称包含命名空间/外层类型限定。
    QStringList diagnostics; // 文件级诊断。
};

// 一次解析，自动发现主文件中的 struct 定义，并递归读取按值嵌套的成员。
HeaderParseResult parseHeaderStructures(const QString &filePath);

// 名称为空时自动选择唯一结构体；有多个时返回候选名称，避免猜测调用方的目标。
// 可传完整限定名，或没有歧义的短名称，兼容原有按名称解析的调用。
ParseResult parseHeader(const QString &filePath,
                        const QString &structName = QString());

#endif
