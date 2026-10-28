#ifndef FIELDVALUE_H
#define FIELDVALUE_H

#include "headerparser.h"

// 校验与界面分开，二进制编码和解码填回也能复用同一套数值范围规则。
struct FieldValueValidation
{
    bool valid = false;
    QString value; // 已校验的数值文本；整数统一转换为十进制，供编码等逻辑使用。
    QString error;
};

// 分别用于：判断能否编辑、生成初始值、生成输入提示、检查用户输入。
bool isEditableField(const FieldInfo &field);
QString defaultFieldValue(const FieldInfo &field);
QString fieldValueHint(const FieldInfo &field);
FieldValueValidation validateFieldValue(const FieldInfo &field, const QString &text);

#endif
