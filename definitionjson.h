#ifndef DEFINITIONJSON_H
#define DEFINITIONJSON_H

#include "headerparser.h"
#include <QJsonDocument>

// 仅接受解析元数据，不接受输入值，避免导出时混入实例数据。
QJsonDocument structureDefinitionJson(const StructInfo &structure);
bool saveStructureDefinitionJson(const StructInfo &structure, const QString &filePath,
                                 QString *error = nullptr);
// 仅解析本程序 schemaVersion=1 的结构定义；此路径不调用 libclang。
ParseResult parseStructureDefinitionJson(const QByteArray &bytes);
ParseResult loadStructureDefinitionJson(const QString &filePath);

#endif
