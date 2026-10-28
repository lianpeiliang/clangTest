#include "binarydecoder.h"
#include "fieldvalue.h"

#include <QRegularExpression>
#include <QSysInfo>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace {
const int maximumBytes = 1024 * 1024;

template<typename T>
QString floatingText(T value)
{
    if (std::isnan(value))
        return QStringLiteral("NaN");
    if (std::isinf(value))
        return std::signbit(value) ? QStringLiteral("-Infinity") : QStringLiteral("Infinity");
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
    return QString::fromStdString(stream.str());
}

QString decodeFloating(const FieldInfo &field, QByteArray bytes, BinaryByteOrder order)
{
    const bool inputLittle = order == BinaryByteOrder::LittleEndian;
    if ((QSysInfo::ByteOrder == QSysInfo::LittleEndian) != inputLittle)
        std::reverse(bytes.begin(), bytes.end());
    if (field.valueKind == FieldValueKind::Float && bytes.size() == sizeof(float)
        && std::numeric_limits<float>::is_iec559) {
        float value;
        std::memcpy(&value, bytes.constData(), sizeof(value));
        return floatingText(value);
    }
    if (field.valueKind == FieldValueKind::Double && bytes.size() == sizeof(double)
        && std::numeric_limits<double>::is_iec559) {
        double value;
        std::memcpy(&value, bytes.constData(), sizeof(value));
        return floatingText(value);
    }
#if defined(_WIN32) && defined(__x86_64__)
    if (field.valueKind == FieldValueKind::LongDouble && bytes.size() == 16 && inputLittle
        && sizeof(long double) == 16 && std::numeric_limits<long double>::digits == 64
        && std::numeric_limits<long double>::max_exponent == 16384
        && QSysInfo::ByteOrder == QSysInfo::LittleEndian) {
        // 拒绝 x87 未规范化的编码，避免将无效对象表示装入浮点寄存器。
        const unsigned exponent = (static_cast<unsigned char>(bytes.at(8))
            | (static_cast<unsigned char>(bytes.at(9)) << 8)) & 0x7fff;
        const bool integerBit = static_cast<unsigned char>(bytes.at(7)) & 0x80;
        if ((exponent != 0) != integerBit)
            return QString();
        long double value = 0;
        std::memcpy(&value, bytes.constData(), 10);
        return floatingText(value);
    }
#endif
    return QString();
}

struct Decoder
{
    const QByteArray &bytes;
    BinaryByteOrder order;
    DecodeResult result;
    int count = 0;

    Decoder(const QByteArray &input, BinaryByteOrder byteOrder) : bytes(input), order(byteOrder) {}

    bool fail(const QString &path, const QString &reason)
    {
        result.diagnostics.append(path + QStringLiteral("：") + reason);
        return false;
    }

    bool fields(const QVector<FieldInfo> &input, QVector<DecodedField> &output,
                const QString &parent, qint64 base, qint64 size, int depth)
    {
        if (depth > 64)
            return fail(parent, QStringLiteral("嵌套层级超过 64。"));
        QByteArray occupied(int(size), '\0');
        for (const FieldInfo &field : input) {
            const QString path = parent.isEmpty() ? field.name : parent + QLatin1Char('.') + field.name;
            if (++count > 10000)
                return fail(path, QStringLiteral("字段总数超过 10000。"));
            if (!field.binaryEncodingIssue.isEmpty())
                return fail(path, field.binaryEncodingIssue);
            if (field.size < 0 || field.size > maximumBytes)
                return fail(path, QStringLiteral("字段大小无效。"));
            const qint64 width = field.isBitField ? field.valueBits : field.size * 8;
            if (width < 0 || field.offsetBits < 0 || field.offsetBits > size * 8
                || width > size * 8 - field.offsetBits
                || field.absoluteOffsetBits != base + field.offsetBits
                || (!field.isBitField && field.absoluteOffsetBits % 8 != 0))
                return fail(path, QStringLiteral("字段越界或相对/绝对偏移不一致。"));
            for (qint64 bit = field.offsetBits; bit < field.offsetBits + width; ++bit) {
                const unsigned mask = 1u << (bit % 8);
                const int byte = int(bit / 8);
                if (static_cast<unsigned char>(occupied.at(byte)) & mask)
                    return fail(path, QStringLiteral("字段与其他字段重叠，无法确定解析结果。"));
                occupied[byte] = char(static_cast<unsigned char>(occupied.at(byte)) | mask);
            }
            if (field.name.isEmpty() && field.isBitField)
                continue;
            DecodedField decoded;
            decoded.name = field.name;
            decoded.path = path;
            decoded.typeName = field.typeName;
            decoded.offsetBits = field.absoluteOffsetBits;
            decoded.widthBits = width;
            const int first = int(field.absoluteOffsetBits / 8);
            const int length = width == 0 ? 0 : int((field.absoluteOffsetBits % 8 + width + 7) / 8);
            decoded.bytes = bytes.mid(first, length);
            if (!field.children.isEmpty() || field.valueKind == FieldValueKind::Record) {
                decoded.value = QStringLiteral("分组");
                if (!fields(field.children, decoded.children, path, field.absoluteOffsetBits, field.size, depth + 1))
                    return false;
            } else if (field.isArray || !isEditableField(field)) {
                decoded.value = QStringLiteral("原始字节");
                decoded.note = field.isArray
                    ? QStringLiteral("数组元素信息未保存，暂不逐元素解析。")
                    : QStringLiteral("此只读类型暂不解析数值。") + field.readOnlyReason;
            } else if (field.valueKind == FieldValueKind::SignedInteger
                       || field.valueKind == FieldValueKind::UnsignedInteger
                       || field.valueKind == FieldValueKind::Boolean) {
                if (field.valueBits < 1 || field.valueBits > 64
                    || field.valueBits > width || (!field.isBitField && field.valueBits != width))
                    return fail(path, QStringLiteral("整数或布尔字段的位宽无效。"));
                if (field.isBitField && order == BinaryByteOrder::BigEndian)
                    return fail(path, QStringLiteral("位域布局来自小端目标，暂不支持按大端解析位域。"));
                quint64 value = 0;
                for (int bit = 0; bit < field.valueBits; ++bit) {
                    const qint64 absolute = field.absoluteOffsetBits + bit;
                    const int byte = order == BinaryByteOrder::LittleEndian
                        ? int(absolute / 8) : first + length - 1 - bit / 8;
                    if (static_cast<unsigned char>(bytes.at(byte)) & (1u << (absolute % 8)))
                        value |= quint64(1) << bit;
                }
                if (field.valueKind == FieldValueKind::Boolean) {
                    decoded.value = value == 0 ? QStringLiteral("false") : QStringLiteral("true");
                    if (value > 1)
                        decoded.note = QStringLiteral("非规范 bool 字节：按非零为 true 解释，填回后规范为 1。");
                } else if (field.valueKind == FieldValueKind::SignedInteger
                           && (value & (quint64(1) << (field.valueBits - 1)))) {
                    const quint64 magnitude = field.valueBits == 64 ? (~value + 1)
                        : (quint64(1) << field.valueBits) - value;
                    decoded.value = QLatin1Char('-') + QString::number(magnitude);
                } else {
                    decoded.value = QString::number(value);
                }
            } else {
                if (field.isBitField)
                    return fail(path, QStringLiteral("浮点字段不能是位域。"));
                decoded.value = decodeFloating(field, decoded.bytes, order);
                if (decoded.value.isEmpty())
                    return fail(path, QStringLiteral("当前宿主不支持此浮点格式/字节序，或浮点编码无效。"));
            }
            if (isEditableField(field)) {
                if (validateFieldValue(field, decoded.value).valid)
                    result.editableValues.insert(path, decoded.value);
                else
                    decoded.note += QStringLiteral("此值不符合字段编辑器范围（如 NaN/无穷大），仅显示，不填回。");
            }
            output.append(decoded);
        }
        return true;
    }
};
} // namespace

BinaryResult parseHexBytes(const QString &text)
{
    BinaryResult result;
    // 一个字节最多写为“0xFF, ”；限制分词前的输入体积。
    if (text.size() > maximumBytes * 8) {
        result.diagnostics.append(QStringLiteral("十六进制文本过大（上限 8 MiB 字符，数据上限 1 MiB）。"));
        return result;
    }
    const QStringList tokens = text.split(QRegularExpression(QStringLiteral("[\\s,]+")), QString::SkipEmptyParts);
    static const QRegularExpression hex(QStringLiteral("^[0-9a-fA-F]+$"));
    for (int i = 0; i < tokens.size(); ++i) {
        QString token = tokens.at(i);
        if (token.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
            token.remove(0, 2);
        if (token.isEmpty() || token.size() % 2 || !hex.match(token).hasMatch()) {
            result.diagnostics.append(QStringLiteral("第 %1 段不是完整的十六进制字节；每字节须两位，不能包含偏移地址或非十六进制字符。")
                                      .arg(i + 1));
            result.bytes.clear();
            return result;
        }
        result.bytes.append(QByteArray::fromHex(token.toLatin1()));
        if (result.bytes.size() > maximumBytes) {
            result.diagnostics.append(QStringLiteral("数据超过 1 MiB 上限。"));
            result.bytes.clear();
            return result;
        }
    }
    result.success = !result.bytes.isEmpty();
    if (!result.success)
        result.diagnostics.append(QStringLiteral("请输入十六进制数据。"));
    return result;
}

DecodeResult decodeStructure(const StructInfo &structure, const QByteArray &bytes, BinaryByteOrder order)
{
    Decoder decoder(bytes, order);
    if (!structure.binaryEncodingIssue.isEmpty()) {
        decoder.fail(structure.name, structure.binaryEncodingIssue);
    } else if (structure.size <= 0 || structure.size > maximumBytes) {
        decoder.fail(structure.name, QStringLiteral("结构体大小无效或超过 1 MiB 上限。"));
    } else if (bytes.size() != structure.size) {
        decoder.fail(structure.name, QStringLiteral("数据长度不匹配：需要 %1 字节，实际 %2 字节（%3 %4 字节）。")
            .arg(structure.size).arg(bytes.size())
            .arg(bytes.size() < structure.size ? QStringLiteral("缺少") : QStringLiteral("多出"))
            .arg(qAbs(structure.size - bytes.size())));
    } else {
        decoder.result.success = decoder.fields(structure.fields, decoder.result.fields,
                                                 QString(), 0, structure.size, 0);
    }
    if (!decoder.result.success) {
        decoder.result.fields.clear();
        decoder.result.editableValues.clear();
    }
    return decoder.result;
}
