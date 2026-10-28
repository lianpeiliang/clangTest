#include "binarydecoder.h"
#include "definitionjson.h"
#include "mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTreeWidget>

#define CHECK(condition) do { if (!(condition)) { \
    QTextStream(stderr) << "FAIL at line " << __LINE__ << ": " << #condition << '\n'; return 1; \
} } while (false)

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
    QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/consola.ttf");
    app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    const QByteArray known = QByteArray::fromHex("78563412221144336655");
    CHECK(parseHexBytes("78563412221144336655").bytes == known);
    CHECK(parseHexBytes("78 56 34 12\r\n22\t11,44,33 66 55").bytes == known);
    CHECK(parseHexBytes("0x78, 0X56 0x34 0x12 0x22 0x11 0x44 0x33 0x66 0x55").bytes == known);
    for (const QString &text : {"", " ", "0", "0x", "123", "7G", "AA ! BB", "0x1", "AA:BB", "00000000  78:56"}) {
        const BinaryResult invalid = parseHexBytes(text);
        CHECK(!invalid.success && invalid.bytes.isEmpty() && !invalid.diagnostics.isEmpty());
    }
    CHECK(!parseHexBytes(QString(2 * 1024 * 1024 + 2, 'A')).success);

    const QString testHeaders = QStringLiteral(MSG_HEADER_PATH).section('/', 0, -2);
    const ParseResult nested = parseHeader(testHeaders + "/NestedPacketHeader.h", "NestedPacketHeader");
    CHECK(nested.success);
    const DecodeResult decoded = decodeStructure(nested.structure, known);
    const QHash<QString, QString> expected{
        {"sequence", "305419896"}, {"address.source", "4386"},
        {"address.destination", "13124"}, {"payloadLength", "21862"}
    };
    CHECK(decoded.success && decoded.editableValues == expected);
    CHECK(decoded.fields.at(1).children.at(1).offsetBits == 48);
    CHECK(decoded.fields.at(1).children.at(1).bytes.toHex() == "4433");
    CHECK(encodeStructure(nested.structure, decoded.editableValues).bytes == known);
    const ParseResult imported = parseStructureDefinitionJson(structureDefinitionJson(nested.structure).toJson());
    CHECK(imported.success && decodeStructure(imported.structure, known).editableValues == expected);
    CHECK(decodeStructure(nested.structure, QByteArray::fromHex("12345678112233445566"),
                          BinaryByteOrder::BigEndian).editableValues == expected);
    for (int size : {0, 9, 11}) {
        const DecodeResult mismatch = decodeStructure(nested.structure, QByteArray(size, '\0'));
        CHECK(!mismatch.success && mismatch.fields.isEmpty() && mismatch.editableValues.isEmpty());
        CHECK(mismatch.diagnostics.join(' ').contains(QString::number(size)));
    }
    StructInfo bad = nested.structure;
    bad.fields.first().absoluteOffsetBits = 80;
    CHECK(!decodeStructure(bad, known).success);
    bad = nested.structure;
    bad.fields.last().offsetBits = 0;
    bad.fields.last().absoluteOffsetBits = 0;
    CHECK(!decodeStructure(bad, known).success);
    bad = nested.structure;
    bad.fields[1].children[0].offsetBits = 100;
    CHECK(!decodeStructure(bad, known).success);
    bad.size = 1024 * 1024 + 1;
    CHECK(!decodeStructure(bad, known).success);

    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    const QString path = temporary.path() + "/Types.h";
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly));
    CHECK(file.write(
        "struct Padded { unsigned char tag; unsigned int count; };\n"
        "#pragma pack(push, 1)\n"
        "struct Values { long long low; unsigned long long high; signed char delta; bool active; "
        "float gain; double ratio; long double extended; char16_t code; };\n"
        "struct Bits { unsigned int flags:3; signed int delta:5; unsigned int :4; unsigned int tail:4; bool active:1; };\n"
        "struct Raw { unsigned char before; unsigned short items[2]; const char label[2]; int *pointer; unsigned char after; };\n"
        "struct Floats { float single; double real; };\n"
        "#pragma pack(pop)\n"
        "struct Base { int x; }; struct Derived : Base { int y; };\n"
        "struct Choice { union { int x; float y; }; };\n") > 0);
    file.close();
    const ParseResult padded = parseHeader(path, "Padded");
    CHECK(padded.success);
    const DecodeResult padding = decodeStructure(padded.structure, QByteArray::fromHex("12aabbcc78563412"));
    CHECK(padding.success && padding.editableValues.value("tag") == "18");
    CHECK(padding.editableValues.value("count") == "305419896");
    CHECK(padding.fields.last().offsetBits == 32);
    const ParseResult values = parseHeader(path, "Values");
    CHECK(values.success);
    const QHash<QString, QString> limits{
        {"low", "-9223372036854775808"}, {"high", "18446744073709551615"}, {"delta", "-128"},
        {"active", "true"}, {"gain", "1.5"}, {"ratio", "-2.5"}, {"extended", "1.25"}, {"code", "20013"}
    };
    const BinaryResult encoded = encodeStructure(values.structure, limits);
    CHECK(encoded.success);
    const DecodeResult limitsDecoded = decodeStructure(values.structure, encoded.bytes);
    CHECK(limitsDecoded.success && limitsDecoded.editableValues == limits);
    CHECK(encodeStructure(values.structure, limitsDecoded.editableValues).bytes == encoded.bytes);
    const ParseResult bits = parseHeader(path, "Bits");
    CHECK(bits.success);
    // MinGW 目标的 unsigned int 位域占用完整的 4 字节分配单元；bool 另起分配单元。
    const DecodeResult bitValues = decodeStructure(bits.structure, QByteArray::fromHex("eda0000001"));
    CHECK(bitValues.success && bitValues.editableValues.value("flags") == "5");
    CHECK(bitValues.editableValues.value("delta") == "-3");
    CHECK(bitValues.editableValues.value("tail") == "10" && bitValues.editableValues.value("active") == "true");
    CHECK(!decodeStructure(bits.structure, QByteArray::fromHex("eda0000001"), BinaryByteOrder::BigEndian).success);

    const ParseResult raw = parseHeader(path, "Raw");
    CHECK(raw.success && raw.structure.size == 16);
    const DecodeResult rawValues = decodeStructure(raw.structure, QByteArray::fromHex("112233445566778899aabbccddeeff99"));
    CHECK(rawValues.success && rawValues.fields.at(1).bytes.toHex() == "22334455");
    CHECK(rawValues.fields.at(2).bytes.toHex() == "6677");
    CHECK(rawValues.fields.at(3).bytes.toHex() == "8899aabbccddeeff");
    CHECK(rawValues.editableValues.size() == 2 && rawValues.editableValues.value("after") == "153");
    for (const QString &name : {"Derived", "Choice"}) {
        const ParseResult unsupported = parseHeader(path, name);
        CHECK(unsupported.success);
        CHECK(!decodeStructure(unsupported.structure, QByteArray(int(unsupported.structure.size), '\0')).success);
    }
    const ParseResult floats = parseHeader(path, "Floats");
    CHECK(floats.success);
    const DecodeResult special = decodeStructure(floats.structure, QByteArray::fromHex("0000c07f000000000000f07f"));
    CHECK(special.success && special.fields.first().value == "NaN" && special.fields.last().value == "Infinity");
    CHECK(special.editableValues.isEmpty());
    const DecodeResult negativeZero = decodeStructure(floats.structure, QByteArray::fromHex("000000800000000000000080"));
    CHECK(negativeZero.success && negativeZero.editableValues.size() == 2);
    CHECK(encodeStructure(floats.structure, negativeZero.editableValues).bytes.toHex() == "000000800000000000000080");
    const DecodeResult bigFloats = decodeStructure(floats.structure, QByteArray::fromHex("3fc00000c004000000000000"), BinaryByteOrder::BigEndian);
    CHECK(bigFloats.success && bigFloats.editableValues.value("single") == "1.5" && bigFloats.editableValues.value("real") == "-2.5");
    const DecodeResult subnormal = decodeStructure(floats.structure, QByteArray::fromHex("010000000100000000000000"));
    CHECK(subnormal.success);
    // 非零次正规数必须显示数值，不能误解成 0。
    CHECK(subnormal.fields.first().value != "0" && subnormal.fields.last().value != "0");

    const QString jsonPath = temporary.path() + "/Nested.json";
    CHECK(saveStructureDefinitionJson(nested.structure, jsonPath));
    MainWindow window(jsonPath);
    window.show();
    auto *open = window.findChild<QPushButton *>("decodeHexButton");
    CHECK(open && open->isEnabled());
    auto *sequence = window.findChild<QLineEdit *>("fieldValue:sequence");
    sequence->setText("invalid");
    CHECK(window.hasInvalidFieldValues() && open->isEnabled());
    open->click();
    auto *dialog = window.findChild<QDialog *>("hexDecodeDialog");
    CHECK(dialog && dialog->windowModality() == Qt::WindowModal);
    auto *input = dialog->findChild<QPlainTextEdit *>("hexDecodeInput");
    auto *parse = dialog->findChild<QPushButton *>("parseHexButton");
    auto *apply = dialog->findChild<QPushButton *>("applyDecodedValuesButton");
    auto *tree = dialog->findChild<QTreeWidget *>("hexDecodedFields");
    auto *order = dialog->findChild<QComboBox *>("hexByteOrder");
    auto *status = dialog->findChild<QLabel *>("hexDecodeStatus");
    CHECK(input && parse && apply && tree && order && status && !apply->isEnabled());
    input->setPlainText("78 56");
    parse->click();
    CHECK(status->text().contains(QStringLiteral("缺少")) && !apply->isEnabled());
    CHECK(window.fieldValues().value("sequence") == "0");
    input->setPlainText("78 56 34 12 22 11 44 33 66 55");
    parse->click();
    CHECK(apply->isEnabled() && tree->topLevelItemCount() == 1);
    CHECK(tree->topLevelItem(0)->child(1)->child(1)->text(4) == "13124");
    apply->click();
    CHECK(window.fieldValues() == expected && !window.hasInvalidFieldValues());
    order->setCurrentIndex(1);
    CHECK(!apply->isEnabled() && tree->topLevelItemCount() == 0);
    input->setPlainText("12 34 56 78 11 22 33 44 55 66");
    parse->click();
    apply->click();
    CHECK(window.fieldValues() == expected);
    app.processEvents();
    if (app.arguments().size() > 1)
        CHECK(dialog->grab().save(app.arguments().at(1)));
    input->setPlainText("GG");
    CHECK(!apply->isEnabled() && tree->topLevelItemCount() == 0);
    parse->click();
    CHECK(!apply->isEnabled() && window.fieldValues() == expected);
    dialog->close();
    app.sendPostedEvents(nullptr, QEvent::DeferredDelete);
    open->click();
    dialog = window.findChild<QDialog *>("hexDecodeDialog");
    CHECK(dialog && dialog->findChild<QPlainTextEdit *>("hexDecodeInput")->toPlainText() == "GG");
    dialog->close();
    auto *source = window.findChild<QLineEdit *>("headerFilePath");
    source->setText(temporary.path() + "/missing.json");
    CHECK(QMetaObject::invokeMethod(source, "returnPressed"));
    CHECK(!open->isEnabled());
    QTextStream(stdout) << "PASS: strict hex, endian, nested/JSON, padding, integer limits, signed bitfields, "
                          "floating values, raw arrays/pointers, layout errors and decode/apply UI\n";
    return 0;
}
