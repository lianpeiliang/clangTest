#ifndef FIELDVALUE_H
#define FIELDVALUE_H

#include "headerparser.h"

struct FieldValueValidation
{
    bool valid = false;
    QString value;
    QString error;
};

bool isEditableField(const FieldInfo &field);
QString defaultFieldValue(const FieldInfo &field);
QString fieldValueHint(const FieldInfo &field);
FieldValueValidation validateFieldValue(const FieldInfo &field, const QString &text);

#endif
