// 头文件保护：同一次编译中即使多处包含本文件，也只处理一次这些类型和函数声明。
#ifndef HEADERPARSER_H
#define HEADERPARSER_H

#include <QString>
#include <QStringList>
#include <QVector>

// 本文件是各模块共用的数据定义：解析器填写它，界面、JSON 和二进制模块读取它。
// 用普通 Qt 数据保存查询结果，界面无需持有 libclang 的 Cursor 或 Type。
// enum class 表示一组可选分类，决定界面如何编辑字段，以及如何编码、解码。
enum class FieldValueKind
{
    ReadOnly, Record, SignedInteger, UnsignedInteger, Boolean, Float, Double, LongDouble
};

// 一个成员字段的信息。嵌套结构体的成员仍然用 FieldInfo 表示，保存在 children 中。
struct FieldInfo
{
    QString name;                // 字段名，例如 sequence。
    QString typeName;            // 声明中的类型名，例如 uint16_t。
    QString canonicalTypeName;   // 展开类型别名后的名称，例如 unsigned short。
    qint64 size = -1;            // 占用字节数；-1 是尚未取得有效结果的初始值。
    qint64 alignment = -1;       // 类型的自然对齐，单位也是字节。
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

// 一个完整结构体：总大小包含填充，fields 按声明顺序保存直接成员。
struct StructInfo
{
    QString name;
    qint64 size = -1;
    qint64 alignment = -1;
    QVector<FieldInfo> fields;
    QString binaryEncodingIssue;
};

// 单个结构体的解析结果。调用方先检查 success，再使用 structure；诊断可能包含警告。
struct ParseResult
{
    bool success = false;
    StructInfo structure;
    QStringList diagnostics;
};

// 整个头文件的解析结果；QVector 是可增长的顺序容器，QStringList 是字符串列表。
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
