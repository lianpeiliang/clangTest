#include "definitionjson.h"
#include "fieldvalue.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
#include <limits>

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

class DefinitionReader
{
public:
    QString error;
    int fieldCount = 0;

    bool fail(const QString &path, const QString &message)
    {
        error = path + QStringLiteral("：") + message;
        return false;
    }

    bool text(const QJsonObject &object, const QString &key, QString &output,
              const QString &path, bool allowEmpty = false)
    {
        const QJsonValue value = object.value(key);
        if (!value.isString() || (!allowEmpty && value.toString().trimmed().isEmpty()))
            return fail(path + QLatin1Char('.') + key, QStringLiteral("必须是%1字符串。").arg(allowEmpty ? QString() : QStringLiteral("非空")));
        output = value.toString();
        return true;
    }

    bool number(const QJsonObject &object, const QString &key, qint64 &output, const QString &path)
    {
        const QJsonValue value = object.value(key);
        if (value.isDouble()) {
            const double n = value.toDouble();
            if (std::isfinite(n) && std::floor(n) == n && std::fabs(n) <= 9007199254740991.0) {
                output = qint64(n);
                return true;
            }
        } else if (value.isString()) {
            static const QRegularExpression digits(QStringLiteral("^-?(?:0|[1-9][0-9]*)$"));
            bool ok = false;
            output = value.toString().toLongLong(&ok, 10);
            if (ok && digits.match(value.toString()).hasMatch())
                return true;
        }
        return fail(path + QLatin1Char('.') + key, QStringLiteral("必须是精确整数；大整数请用十进制字符串。"));
    }

    bool boolean(const QJsonObject &object, const QString &key, bool &output, const QString &path)
    {
        if (!object.value(key).isBool())
            return fail(path + QLatin1Char('.') + key, QStringLiteral("必须是布尔值。"));
        output = object.value(key).toBool();
        return true;
    }

    bool optionalText(const QJsonObject &object, const QString &key, QString &output, const QString &path)
    {
        return !object.contains(key) || text(object, key, output, path, true);
    }

    bool layout(qint64 size, qint64 alignment, const QString &path, bool allowZero)
    {
        if (size < (allowZero ? 0 : 1) || size > std::numeric_limits<qint64>::max() / 8)
            return fail(path, QStringLiteral("大小无效或超出支持范围。"));
        if (alignment <= 0 || (alignment & (alignment - 1)) != 0)
            return fail(path, QStringLiteral("对齐必须是正的 2 的幂。"));
        if (size % alignment != 0)
            return fail(path, QStringLiteral("大小必须是类型对齐的整数倍。"));
        return true;
    }

    bool fields(const QJsonValue &value, QVector<FieldInfo> &output, qint64 parentSize,
                qint64 baseOffset, const QString &path, int depth = 0)
    {
        if (!value.isArray() || depth > 64)
            return fail(path, QStringLiteral("fields 必须是数组，嵌套深度不能超过 64。"));
        QSet<QString> names;
        const QJsonArray array = value.toArray();
        for (int i = 0; i < array.size(); ++i) {
            const QString itemPath = path + QStringLiteral("[%1]").arg(i);
            if (!array.at(i).isObject() || ++fieldCount > 10000)
                return fail(itemPath, QStringLiteral("字段必须是对象，总字段数不能超过 10000。"));
            const QJsonObject object = array.at(i).toObject();
            if (object.contains("value") || object.contains("values"))
                return fail(itemPath, QStringLiteral("结构定义不能包含实例值。"));
            FieldInfo field;
            QString kind;
            bool editable = false;
            if (!text(object, "name", field.name, itemPath, true)
                || !text(object, "type", field.typeName, itemPath)
                || !text(object, "canonicalType", field.canonicalTypeName, itemPath)
                || !text(object, "kind", kind, itemPath)
                || !number(object, "size", field.size, itemPath)
                || !number(object, "alignment", field.alignment, itemPath)
                || !number(object, "offsetBits", field.offsetBits, itemPath)
                || !number(object, "absoluteOffsetBits", field.absoluteOffsetBits, itemPath)
                || !boolean(object, "isArray", field.isArray, itemPath)
                || !boolean(object, "isBitField", field.isBitField, itemPath)
                || !boolean(object, "editable", editable, itemPath)
                || !optionalText(object, "readOnlyReason", field.readOnlyReason, itemPath)
                || !optionalText(object, "binaryEncodingIssue", field.binaryEncodingIssue, itemPath)
                || !layout(field.size, field.alignment, itemPath, true))
                return false;
            bool known = false;
            for (FieldValueKind candidate : {FieldValueKind::ReadOnly, FieldValueKind::Record,
                 FieldValueKind::SignedInteger, FieldValueKind::UnsignedInteger, FieldValueKind::Boolean,
                 FieldValueKind::Float, FieldValueKind::Double, FieldValueKind::LongDouble}) {
                if (kindName(candidate) == kind) {
                    field.valueKind = candidate;
                    known = true;
                    break;
                }
            }
            if (!known || editable != isEditableField(field))
                return fail(itemPath, QStringLiteral("kind 未知或与 editable 不一致。"));
            static const QRegularExpression invalidName(QStringLiteral("[.\\x00-\\x1f]"));
            if (invalidName.match(field.name).hasMatch()
                || (field.name.isEmpty() && (!field.isBitField || editable))
                || (!field.name.isEmpty() && names.contains(field.name)))
                return fail(itemPath, QStringLiteral("字段名称无效或重复。"));
            names.insert(field.name);
            const bool integerKind = field.valueKind == FieldValueKind::SignedInteger
                || field.valueKind == FieldValueKind::UnsignedInteger;
            qint64 occupiedBits = field.size * 8;
            qint64 width = 0;
            if (integerKind) {
                if (!number(object, "integerBits", width, itemPath))
                    return false;
                if (width < 1 || width > 64 || (!field.isBitField && width != occupiedBits))
                    return fail(itemPath, QStringLiteral("整数有效位数必须为 1～64，并与布局一致。"));
                field.valueBits = int(width);
            }
            if (field.isBitField) {
                if (!number(object, "bitWidth", width, itemPath))
                    return false;
                if (width < 0 || width > std::numeric_limits<int>::max()
                    || (editable && (width < 1 || width > field.size * 8))
                    || (integerKind && width != field.valueBits)
                    || field.isArray || (editable && !integerKind && field.valueKind != FieldValueKind::Boolean))
                    return fail(itemPath, QStringLiteral("位域宽度或类型无效。"));
                field.valueBits = int(width);
                occupiedBits = width;
            } else if (field.offsetBits % 8 != 0) {
                return fail(itemPath, QStringLiteral("非位域字段必须按字节定位。"));
            }
            if (field.valueKind == FieldValueKind::Boolean) {
                if (field.size != 1)
                    return fail(itemPath, QStringLiteral("bool 大小必须为 1 字节。"));
                if (!field.isBitField)
                    field.valueBits = 8;
            }
            if ((field.valueKind == FieldValueKind::Float && field.size != 4)
                || (field.valueKind == FieldValueKind::Double && field.size != 8)
                || (field.valueKind == FieldValueKind::LongDouble && field.size != 16))
                return fail(itemPath, QStringLiteral("浮点大小不符合当前目标平台。"));
            if (field.offsetBits < 0 || field.offsetBits > parentSize * 8
                || occupiedBits > parentSize * 8 - field.offsetBits
                || baseOffset > std::numeric_limits<qint64>::max() - field.offsetBits
                || field.absoluteOffsetBits != baseOffset + field.offsetBits)
                return fail(itemPath, QStringLiteral("字段越界或相对/绝对偏移不一致。"));
            if (!object.value("fields").isArray())
                return fail(itemPath, QStringLiteral("fields 必须是数组。"));
            const bool hasChildren = !object.value("fields").toArray().isEmpty();
            if ((field.isArray && (editable || field.isBitField || field.valueKind != FieldValueKind::ReadOnly))
                || (hasChildren && (editable || field.isArray || field.isBitField)))
                return fail(itemPath, QStringLiteral("叶子类型和嵌套字段不一致。"));
            if (!fields(object.value("fields"), field.children, field.size, field.absoluteOffsetBits,
                        itemPath + ".fields", depth + 1))
                return false;
            output.append(field);
        }
        return true;
    }
};

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

ParseResult parseStructureDefinitionJson(const QByteArray &bytes)
{
    ParseResult result;
    if (bytes.size() > 16 * 1024 * 1024) {
        result.diagnostics.append(QStringLiteral("结构定义 JSON 不能超过 16 MiB。"));
        return result;
    }
    QJsonParseError jsonError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !document.isObject()) {
        result.diagnostics.append(QStringLiteral("JSON 格式错误（字节位置 %1）：%2")
            .arg(jsonError.offset).arg(jsonError.error != QJsonParseError::NoError
                                     ? jsonError.errorString() : QStringLiteral("顶层必须是对象。")));
        return result;
    }
    const QJsonObject root = document.object();
    DefinitionReader reader;
    StructInfo structure;
    if (!root.value("schemaVersion").isDouble() || root.value("schemaVersion").toDouble() != 1
        || root.value("target") != QJsonValue(QStringLiteral("x86_64-w64-windows-gnu"))
        || root.value("byteOrder") != QJsonValue(QStringLiteral("little"))) {
        result.diagnostics.append(QStringLiteral("不支持的结构定义版本或目标平台；需要 schemaVersion=1、x86_64-w64-windows-gnu、little。"));
        return result;
    }
    if (root.contains("value") || root.contains("values")) {
        result.diagnostics.append(QStringLiteral("需要不含实例值的结构定义 JSON。"));
        return result;
    }
    if (!reader.text(root, "name", structure.name, "$")
        || !reader.number(root, "size", structure.size, "$")
        || !reader.number(root, "alignment", structure.alignment, "$")
        || !reader.optionalText(root, "binaryEncodingIssue", structure.binaryEncodingIssue, "$")
        || !reader.layout(structure.size, structure.alignment, "$", false)
        || !reader.fields(root.value("fields"), structure.fields, structure.size, 0, "$.fields")) {
        result.diagnostics.append(reader.error);
        return result;
    }
    result.structure = structure;
    result.success = true;
    return result;
}

ParseResult loadStructureDefinitionJson(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ParseResult result;
        result.diagnostics.append(QStringLiteral("无法读取结构定义 JSON：%1：%2").arg(filePath, file.errorString()));
        return result;
    }
    return parseStructureDefinitionJson(file.read(16 * 1024 * 1024 + 1));
}
