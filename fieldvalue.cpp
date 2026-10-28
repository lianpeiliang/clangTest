#include "fieldvalue.h"

#include <QRegularExpression>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>

// 本模块只检查“某种字段类型能否接受这段文字”，不读写窗口，也不修改头文件。
namespace {

// n 位有符号整数范围是 -2^(n-1) ～ 2^(n-1)-1，无符号整数范围是 0 ～ 2^n-1。
// 64 位单独取标准库边界，避免对 64 位整数执行移位 64 位或移入有符号数的符号位。
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

// 模板让 float、double、long double 共用一份范围检查，T 在调用时确定。
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
        return QStringLiteral("整数：十进制或 0x 前缀十六进制，负数可写 -0x10；范围：%1 ～ %2")
            .arg(signedMinimum(field.valueBits)).arg(signedMaximum(field.valueBits));
    case FieldValueKind::UnsignedInteger:
        return QStringLiteral("无符号整数：十进制或 0x 前缀十六进制（如 0xFF）；范围：0 ～ %1")
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

// 输入是原始文字，输出是“是否合法 + 规范数值文字 + 出错说明”。
// 整数一直用 64 位整数和字符串处理，避免先经过 double 造成大整数精度丢失。
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
        // 正则表达式先检查整段文字的格式，^ 和 $ 分别要求匹配开头和结尾。
        static const QRegularExpression integer(QStringLiteral("^[+-]?(?:0[xX][0-9a-fA-F]+|[0-9]+)$"));
        if (!integer.match(input).hasMatch())
            return result;
        // 显式指定进制，保证 010 仍按十进制处理；先去除 0x，保留数值的正负号。
        const int signLength = input.startsWith(QLatin1Char('+')) || input.startsWith(QLatin1Char('-')) ? 1 : 0;
        const bool hexadecimal = input.mid(signLength, 2).compare(QLatin1String("0x"), Qt::CaseInsensitive) == 0;
        const int base = hexadecimal ? 16 : 10;
        QString number = input;
        if (hexadecimal)
            number.remove(signLength, 2);
        // Qt 的字符串转换通过 ok 告知是否成功，再按字段实际位宽检查范围。
        bool ok = false;
        if (field.valueKind == FieldValueKind::SignedInteger) {
            const qint64 value = number.toLongLong(&ok, base);
            result.valid = ok && value >= signedMinimum(field.valueBits)
                && value <= signedMaximum(field.valueBits);
            result.value = QString::number(value);
        } else {
            const quint64 value = number.toULongLong(&ok, base);
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
