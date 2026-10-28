#ifndef DEFINITIONJSON_H
#define DEFINITIONJSON_H

#include "headerparser.h"
#include <QJsonDocument>

// JSON 用于保存“结构体长什么样”：字段类型、大小、偏移及嵌套关系。
// 这些接口都使用 StructInfo，所以从头文件加载和从 JSON 加载可共用后续功能。

// 把 structure 转成内存中的 JSON 文档，不写磁盘；不包含用户填写的字段值。
QJsonDocument structureDefinitionJson(const StructInfo &structure);

// 把结构定义保存到 filePath，使用 QSaveFile 写临时文件后提交；返回 true 表示保存成功。
// error 是可选的错误输出指针：传入时，成功会清空它，失败会写入原因；传 nullptr 则不接收说明。
bool saveStructureDefinitionJson(const StructInfo &structure, const QString &filePath,
                                 QString *error = nullptr);

// 仅解析本程序 schemaVersion=1 的结构定义；此路径不调用 libclang。
// bytes 是 JSON 文件内容；读取后检查格式、目标平台、字段类型及布局，结果通过 ParseResult 返回。
ParseResult parseStructureDefinitionJson(const QByteArray &bytes);

// 先从 filePath 读取 JSON 文件，再调用上面的内容解析接口；无法读取时返回失败诊断。
ParseResult loadStructureDefinitionJson(const QString &filePath);

#endif
