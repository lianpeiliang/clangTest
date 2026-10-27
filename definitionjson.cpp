#include "definitionjson.h"
#include "fieldvalue.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QJsonValue integer(qint64 value)
{
    // Qt 5 的 JSON 数字通过 double 保存，极大的布局整数用十进制字符串保留精度。
    const qint64 maximumExactInteger = 9007199254740991LL;
    return value > maximumExactInteger || value < -maximumExactInteger
        ? QJsonValue(QString::number(value)) : QJsonValue(double(value));
}

QString kindName(FieldValueKind kind)
{
    switch (kind) {
    case FieldValueKind::SignedInteger: return QStringLiteral("signedInteger");
    case FieldValueKind::UnsignedInteger: return QStringLiteral("unsignedInteger");
    case FieldValueKind::Boolean: return QStringLiteral("boolean");
    case FieldValueKind::Float: return QStringLiteral("float");
    case FieldValueKind::Double: return QStringLiteral("double");
    case FieldValueKind::LongDouble: return QStringLiteral("longDouble");
    case FieldValueKind::Record: return QStringLiteral("record");
    default: return QStringLiteral("readOnly");
    }
}

QJsonArray fieldsJson(const QVector<FieldInfo> &fields)
{
    QJsonArray result;
    for (const FieldInfo &field : fields) {
        QJsonObject object{
            {QStringLiteral("name"), field.name},
            {QStringLiteral("type"), field.typeName},
            {QStringLiteral("canonicalType"), field.canonicalTypeName},
            {QStringLiteral("kind"), kindName(field.valueKind)},
            {QStringLiteral("size"), integer(field.size)},
            {QStringLiteral("alignment"), integer(field.alignment)},
            {QStringLiteral("offsetBits"), integer(field.offsetBits)},
            {QStringLiteral("absoluteOffsetBits"), integer(field.absoluteOffsetBits)},
            {QStringLiteral("isArray"), field.isArray},
            {QStringLiteral("isBitField"), field.isBitField},
            {QStringLiteral("editable"), isEditableField(field)},
            {QStringLiteral("fields"), fieldsJson(field.children)}
        };
        if (field.valueKind == FieldValueKind::SignedInteger || field.valueKind == FieldValueKind::UnsignedInteger)
            object.insert(QStringLiteral("integerBits"), field.valueBits);
        if (field.isBitField)
            object.insert(QStringLiteral("bitWidth"), field.valueBits);
        if (!field.readOnlyReason.isEmpty())
            object.insert(QStringLiteral("readOnlyReason"), field.readOnlyReason);
        if (!field.binaryEncodingIssue.isEmpty())
            object.insert(QStringLiteral("binaryEncodingIssue"), field.binaryEncodingIssue);
        result.append(object);
    }
    return result;
}

} // namespace

QJsonDocument structureDefinitionJson(const StructInfo &structure)
{
    QJsonObject root{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("target"), QStringLiteral("x86_64-w64-windows-gnu")},
        {QStringLiteral("byteOrder"), QStringLiteral("little")},
        {QStringLiteral("name"), structure.name},
        {QStringLiteral("size"), integer(structure.size)},
        {QStringLiteral("alignment"), integer(structure.alignment)},
        {QStringLiteral("fields"), fieldsJson(structure.fields)}
    };
    if (!structure.binaryEncodingIssue.isEmpty())
        root.insert(QStringLiteral("binaryEncodingIssue"), structure.binaryEncodingIssue);
    return QJsonDocument(root);
}

bool saveStructureDefinitionJson(const StructInfo &structure, const QString &filePath, QString *error)
{
    if (error)
        error->clear();
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray bytes = structureDefinitionJson(structure).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        if (error)
            *error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}
