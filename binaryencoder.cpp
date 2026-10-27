#include "binaryencoder.h"
#include "fieldvalue.h"

#include <QSysInfo>
#include <cmath>
#include <cstring>
#include <limits>
#include <locale>
#include <sstream>

namespace {

// 预览会生成较长的文本，限制内存和界面开销。
const qint64 maximumPreviewBytes = 1024 * 1024;

bool writeBits(BinaryResult &result, QByteArray &written, const QString &path,
               qint64 offset, int count, const QByteArray &source)
{
    const qint64 totalBits = qint64(result.bytes.size()) * 8;
    if (offset < 0 || count <= 0 || offset > totalBits || count > totalBits - offset
        || count > qint64(source.size()) * 8) {
        result.diagnostics.append(QStringLiteral("%1：字段布局超出结构体边界或编码宽度不匹配。").arg(path));
        return false;
    }
    for (int bit = 0; bit < count; ++bit) {
        const int byte = int((offset + bit) / 8);
        const unsigned mask = 1u << ((offset + bit) % 8);
        if (static_cast<unsigned char>(written.at(byte)) & mask) {
            result.diagnostics.append(QStringLiteral("%1：字段与其他字段重叠，无法确定编码。").arg(path));
            return false;
        }
    }
    for (int bit = 0; bit < count; ++bit) {
        const int byte = int((offset + bit) / 8);
        const unsigned mask = 1u << ((offset + bit) % 8);
        written[byte] = char(static_cast<unsigned char>(written.at(byte)) | mask);
        if (static_cast<unsigned char>(source.at(bit / 8)) & (1u << (bit % 8)))
            result.bytes[byte] = char(static_cast<unsigned char>(result.bytes.at(byte)) | mask);
    }
    return true;
}

template<typename T, typename UInt>
QByteArray floatingBytes(const QString &text, long double validatedValue)
{
    // 直接转换到目标浮点类型，避免先转 long double 再缩窄时发生二次舍入。
    std::istringstream stream(text.toStdString());
    stream.imbue(std::locale::classic());
    T value = 0;
    stream >> value;
    // 输入已通过范围校验；部分标准库转换非零次正规数时会设置 failbit，
    // 但保留有效的转换结果。真正下溢成 0 或非有限值仍然拒绝。
    if (stream.bad() || !std::isfinite(value) || (value == 0 && validatedValue != 0))
        return QByteArray();
    UInt bits = 0;
    static_assert(sizeof(value) == sizeof(bits), "Unexpected floating point width");
    std::memcpy(&bits, &value, sizeof(bits));
    QByteArray bytes(sizeof(bits), '\0');
    for (unsigned i = 0; i < sizeof(bits); ++i)
        bytes[int(i)] = char((bits >> (i * 8)) & 0xff);
    return bytes;
}

QByteArray encodeFloating(const FieldInfo &field, const QString &text)
{
    std::istringstream stream(text.toStdString());
    stream.imbue(std::locale::classic());
    long double value = 0;
    stream >> value;
    if (stream.fail() || !std::isfinite(value))
        return QByteArray();
    if (field.valueKind == FieldValueKind::Float && field.size == 4
        && std::numeric_limits<float>::is_iec559)
        return floatingBytes<float, quint32>(text, value);
    if (field.valueKind == FieldValueKind::Double && field.size == 8
        && std::numeric_limits<double>::is_iec559)
        return floatingBytes<double, quint64>(text, value);
#if defined(_WIN32) && defined(__x86_64__)
    // Windows x64 / MinGW 的 long double 是 80 位 x87 数值，占用 16 字节。
    // 只复制有效的 10 字节，剩余 6 字节补 0，避免复制宿主栈上的填充内容。
    if (field.valueKind == FieldValueKind::LongDouble && field.size == 16
        && sizeof(long double) == 16 && std::numeric_limits<long double>::digits == 64
        && std::numeric_limits<long double>::max_exponent == 16384
        && QSysInfo::ByteOrder == QSysInfo::LittleEndian) {
        QByteArray bytes(16, '\0');
        std::memcpy(bytes.data(), &value, 10);
        return bytes;
    }
#endif
    return QByteArray();
}

void encodeFields(BinaryResult &result, QByteArray &written, const QVector<FieldInfo> &fields,
                  const QHash<QString, QString> &values, const QString &parentPath)
{
    for (const FieldInfo &field : fields) {
        const QString path = parentPath.isEmpty() ? field.name : parentPath + QLatin1Char('.') + field.name;
        if (field.name.isEmpty() && field.isBitField)
            continue; // 未命名位域是填充，保持 0。
        if (!field.binaryEncodingIssue.isEmpty()) {
            result.diagnostics.append(path + QStringLiteral("：") + field.binaryEncodingIssue);
            continue;
        }
        if (field.valueKind == FieldValueKind::Record) {
            encodeFields(result, written, field.children, values, path);
            continue;
        }
        if (!isEditableField(field)) {
            result.diagnostics.append(path + QStringLiteral("：无法编码只读字段。") + field.readOnlyReason);
            continue;
        }
        if (!values.contains(path)) {
            result.diagnostics.append(QStringLiteral("%1：缺少字段值。").arg(path));
            continue;
        }
        const FieldValueValidation validation = validateFieldValue(field, values.value(path));
        if (!validation.valid) {
            result.diagnostics.append(path + QStringLiteral("：") + validation.error);
            continue;
        }
        if (field.valueKind == FieldValueKind::SignedInteger
            || field.valueKind == FieldValueKind::UnsignedInteger
            || field.valueKind == FieldValueKind::Boolean) {
            const quint64 value = field.valueKind == FieldValueKind::Boolean
                ? quint64(validation.value == QLatin1String("true"))
                : field.valueKind == FieldValueKind::SignedInteger
                    ? static_cast<quint64>(validation.value.toLongLong()) : validation.value.toULongLong();
            QByteArray bytes(8, '\0');
            for (int i = 0; i < 8; ++i)
                bytes[i] = char((value >> (i * 8)) & 0xff);
            writeBits(result, written, path, field.absoluteOffsetBits, field.valueBits, bytes);
        } else {
            const QByteArray bytes = encodeFloating(field, validation.value);
            if (bytes.isEmpty() || field.absoluteOffsetBits % 8 != 0) {
                result.diagnostics.append(QStringLiteral("%1：当前宿主不支持此浮点格式的目标编码。").arg(path));
                continue;
            }
            writeBits(result, written, path, field.absoluteOffsetBits, bytes.size() * 8, bytes);
        }
    }
}

} // namespace

BinaryResult encodeStructure(const StructInfo &structure, const QHash<QString, QString> &values)
{
    BinaryResult result;
    if (!structure.binaryEncodingIssue.isEmpty()) {
        result.diagnostics.append(structure.name + QStringLiteral("：") + structure.binaryEncodingIssue);
        return result;
    }
    if (structure.size <= 0 || structure.size > maximumPreviewBytes) {
        result.diagnostics.append(QStringLiteral("结构体大小无效或超过预览上限（1 MiB）。"));
        return result;
    }
    result.bytes = QByteArray(int(structure.size), '\0');
    QByteArray written(int(structure.size), '\0');
    encodeFields(result, written, structure.fields, values, QString());
    result.success = result.diagnostics.isEmpty();
    if (!result.success)
        result.bytes.clear(); // 出错时不展示不完整的数据。
    return result;
}

QString formatHexDump(const QByteArray &bytes)
{
    QStringList lines;
    for (int offset = 0; offset < bytes.size(); offset += 16) {
        lines.append(QStringLiteral("%1  %2")
            .arg(offset, 8, 16, QLatin1Char('0'))
            .arg(QString::fromLatin1(bytes.mid(offset, 16).toHex(' ').toUpper())));
    }
    return lines.join(QLatin1Char('\n'));
}
