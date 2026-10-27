#ifndef HEADERPARSER_H
#define HEADERPARSER_H

#include <QString>
#include <QStringList>
#include <QVector>

// 用普通 Qt 数据保存查询结果，界面无需持有 libclang 的 Cursor 或 Type。
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
};

struct StructInfo
{
    QString name;
    qint64 size = -1;
    qint64 alignment = -1;
    QVector<FieldInfo> fields;
};

struct ParseResult
{
    bool success = false;
    StructInfo structure;
    QStringList diagnostics;
};

// 每次调用都重新读取磁盘文件，查找指定名称的 struct 并递归读取按值嵌套的成员。
ParseResult parseHeader(const QString &filePath,
                        const QString &structName = QStringLiteral("MsgHeader"));

#endif
