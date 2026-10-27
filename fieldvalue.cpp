#include "fieldvalue.h"

#include <QRegularExpression>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>

namespace {

qint64 signedMinimum(int bits)
{
    return bits == 64 ? std::numeric_limits<qint64>::min() : -(qint64(1) << (bits - 1));
}

qint64 signedMaximum(int bits)
{
    return bits == 64 ? std::numeric_limits<qint64>::max() : (qint64(1) << (bits - 1)) - 1;
}

quint64 unsignedMaximum(int bits)
{
    return bits == 64 ? std::numeric_limits<quint64>::max() : (quint64(1) << bits) - 1;
}

template<typename T>
bool inFloatingRange(long double value)
{
    const long double magnitude = std::fabs(value);
    return magnitude <= std::numeric_limits<T>::max()
        && (magnitude == 0 || magnitude >= std::numeric_limits<T>::denorm_min());
}

} // namespace

bool isEditableField(const FieldInfo &field)
{
    return field.valueKind != FieldValueKind::ReadOnly && field.valueKind != FieldValueKind::Record;
}

QString defaultFieldValue(const FieldInfo &field)
{
    return field.valueKind == FieldValueKind::Boolean ? QStringLiteral("false") : QStringLiteral("0");
}

QString fieldValueHint(const FieldInfo &field)
{
    switch (field.valueKind) {
    case FieldValueKind::SignedInteger:
        return QStringLiteral("十进制整数，范围：%1 ～ %2")
            .arg(signedMinimum(field.valueBits)).arg(signedMaximum(field.valueBits));
    case FieldValueKind::UnsignedInteger:
        return QStringLiteral("十进制无符号整数，范围：0 ～ %1")
            .arg(unsignedMaximum(field.valueBits));
    case FieldValueKind::Boolean:
        return QStringLiteral("请选择 false 或 true。");
    case FieldValueKind::Float: case FieldValueKind::Double: case FieldValueKind::LongDouble:
        return QStringLiteral("使用小数点或科学计数法，例如 -1.25 或 1e-3；须在 %1 的有限值范围内。")
            .arg(field.canonicalTypeName);
    default:
        return field.readOnlyReason;
    }
}

FieldValueValidation validateFieldValue(const FieldInfo &field, const QString &text)
{
    FieldValueValidation result;
    const QString input = text.trimmed();
    result.error = fieldValueHint(field);
    if (!isEditableField(field))
        return result;
    if (input.isEmpty()) {
        result.error = QStringLiteral("值不能为空。") + result.error;
        return result;
    }
    if (field.valueKind == FieldValueKind::Boolean) {
        result.valid = input == QLatin1String("true") || input == QLatin1String("false");
        result.value = input;
    } else if (field.valueKind == FieldValueKind::SignedInteger
               || field.valueKind == FieldValueKind::UnsignedInteger) {
        static const QRegularExpression integer(QStringLiteral("^[+-]?[0-9]+$"));
        if (!integer.match(input).hasMatch())
            return result;
        bool ok = false;
        if (field.valueKind == FieldValueKind::SignedInteger) {
            const qint64 value = input.toLongLong(&ok, 10);
            result.valid = ok && value >= signedMinimum(field.valueBits)
                && value <= signedMaximum(field.valueBits);
            result.value = QString::number(value);
        } else {
            const quint64 value = input.toULongLong(&ok, 10);
            result.valid = ok && !input.startsWith(QLatin1Char('-'))
                && value <= unsignedMaximum(field.valueBits);
            result.value = QString::number(value);
        }
    } else {
        static const QRegularExpression floating(QStringLiteral(
            "^[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?$"));
        if (!floating.match(input).hasMatch())
            return result;
        // 使用经典 locale 和 long double，避免经过 double 丢失扩展精度的范围。
        std::istringstream stream(input.toStdString());
        stream.imbue(std::locale::classic());
        long double value = 0;
        stream >> value;
        if (stream.fail() || !std::isfinite(value))
            return result;
        const QString mantissa = input.section(QRegularExpression(QStringLiteral("[eE]")), 0, 0);
        if (value == 0 && mantissa.contains(QRegularExpression(QStringLiteral("[1-9]"))))
            return result; // 非零输入下溢成 0。
        if (field.valueKind == FieldValueKind::Float)
            result.valid = inFloatingRange<float>(value);
        else if (field.valueKind == FieldValueKind::Double)
            result.valid = inFloatingRange<double>(value);
        else
            result.valid = inFloatingRange<long double>(value);
        result.value = input;
    }
    if (result.valid)
        result.error.clear();
    return result;
}
