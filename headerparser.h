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
    qint64 offsetBits = -1;
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

// 每次调用都重新读取磁盘文件；当前查找指定名称的 struct 及其直接成员。
ParseResult parseHeader(const QString &filePath,
                        const QString &structName = QStringLiteral("MsgHeader"));

#endif
