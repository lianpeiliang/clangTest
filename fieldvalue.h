#ifndef FIELDVALUE_H
#define FIELDVALUE_H

#include "headerparser.h"

struct FieldValueValidation
{
    bool valid = false;
    QString value; // 已校验的数值文本；整数统一转换为十进制，供编码等逻辑使用。
    QString error;
};

bool isEditableField(const FieldInfo &field);
QString defaultFieldValue(const FieldInfo &field);
QString fieldValueHint(const FieldInfo &field);
FieldValueValidation validateFieldValue(const FieldInfo &field, const QString &text);

#endif
