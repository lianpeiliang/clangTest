#ifndef FIELDVALUE_H
#define FIELDVALUE_H

#include "headerparser.h"

// 字段值校验接口：输入字段的类型信息和文字，返回能否接受该输入及其原因。
// 校验与界面分开，二进制编码和解码填回也能复用同一套数值范围规则。
struct FieldValueValidation
{
    bool valid = false; // 输入是否通过格式和数值范围校验；为 true 时才使用 value。
    QString value;      // 通过校验的数值文本；整数转为十进制，浮点数和 bool 保留合法文字形式。
    QString error;      // 校验失败的说明或输入要求；成功时为空，用于界面提示。
};

// field 提供字段类型；返回是否属于可编辑的基础类型，记录分组和只读字段返回 false。
bool isEditableField(const FieldInfo &field);

// 取得可编辑字段的初始输入：bool 为 "false"，其余可编辑数值类型为 "0"。
QString defaultFieldValue(const FieldInfo &field);

// 生成给用户看的输入要求，如整数范围、进制写法，或字段只读的原因。
QString fieldValueHint(const FieldInfo &field);

// 检查 text 是否适合 field；先去除首尾空白，再按类型检查格式、转换是否成功及数值范围。
// 不修改字段定义和界面；调用方读取返回值中的 valid、value、error 决定后续处理。
FieldValueValidation validateFieldValue(const FieldInfo &field, const QString &text);

#endif
