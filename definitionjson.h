#ifndef DEFINITIONJSON_H
#define DEFINITIONJSON_H

#include "headerparser.h"
#include <QJsonDocument>

// JSON 用于保存“结构体长什么样”：字段类型、大小、偏移及嵌套关系。
// 这些接口都使用 StructInfo，所以从头文件加载和从 JSON 加载可共用后续功能。
// 仅接受解析元数据，不接受输入值，避免导出时混入实例数据。
QJsonDocument structureDefinitionJson(const StructInfo &structure);
bool saveStructureDefinitionJson(const StructInfo &structure, const QString &filePath,
                                 QString *error = nullptr);
// 仅解析本程序 schemaVersion=1 的结构定义；此路径不调用 libclang。
ParseResult parseStructureDefinitionJson(const QByteArray &bytes);
ParseResult loadStructureDefinitionJson(const QString &filePath);

#endif
