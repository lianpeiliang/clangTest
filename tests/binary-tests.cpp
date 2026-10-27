#include "binaryencoder.h"
#include "fieldvalue.h"
#include "mainwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFontDatabase>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTreeWidget>
#include <cstddef>
#include <cstring>

#define CHECK(condition) do { if (!(condition)) { \
    QTextStream(stderr) << "FAIL at line " << __LINE__ << ": " << #condition << '\n'; return 1; \
} } while (false)

#pragma pack(push, 1)
struct NativeAddress { unsigned short source; unsigned short destination; };
struct NativePacked
{
    unsigned short tag;
    NativeAddress address;
    signed char delta;
    bool active;
    float gain;
    double ratio;
    long double extended;
    unsigned int flags:3;
    signed int signedBits:5;
    unsigned int :4;
    unsigned int tail:4;
    char16_t code;
};
#pragma pack(pop)

static bool writeSource(const QString &path, const QByteArray &source)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(source) == source.size();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    if (QApplication::platformName() == "offscreen") {
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/consola.ttf");
        app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    }
#endif
    const QString project = QStringLiteral(MSG_HEADER_PATH).section('/', 0, -2);
    const ParseResult flat = parseHeader(project + "/MsgHeader.h");
    CHECK(flat.success);
    QHash<QString, QString> zeroValues;
    for (const FieldInfo &field : flat.structure.fields)
        zeroValues.insert(field.name, defaultFieldValue(field));
    const BinaryResult zeroBytes = encodeStructure(flat.structure, zeroValues);
    CHECK(zeroBytes.success && zeroBytes.bytes == QByteArray(83, '\0'));
    const ParseResult nested = parseHeader(project + "/NestedPacketHeader.h", "NestedPacketHeader");
    CHECK(nested.success);
    const QHash<QString, QString> nestedValues{
        {"sequence", "305419896"}, {"address.source", "4386"},
        {"address.destination", "13124"}, {"payloadLength", "21862"}
    };
    const BinaryResult nestedBytes = encodeStructure(nested.structure, nestedValues);
    CHECK(nestedBytes.success && nestedBytes.bytes.toHex() == "78563412221144336655");
    CHECK(formatHexDump(nestedBytes.bytes) == "00000000  78 56 34 12 22 11 44 33 66 55");
    const QHash<QString, QString> nestedHexValues{
        {"sequence", "0X12345678"}, {"address.source", "0x1122"},
        {"address.destination", "0x3344"}, {"payloadLength", "0x5566"}
    };
    const BinaryResult nestedHexBytes = encodeStructure(nested.structure, nestedHexValues);
    CHECK(nestedHexBytes.success && nestedHexBytes.bytes == nestedBytes.bytes);

    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    const QString path = temporary.path() + "/NativePacked.h";
    CHECK(writeSource(path,
        "#pragma pack(push, 1)\n"
        "struct NativeAddress { unsigned short source; unsigned short destination; };\n"
        "struct NativePacked { unsigned short tag; NativeAddress address; signed char delta; "
        "bool active; float gain; double ratio; long double extended; unsigned int flags:3; "
        "signed int signedBits:5; unsigned int :4; unsigned int tail:4; char16_t code; };\n"
        "#pragma pack(pop)\n"));
    const ParseResult packed = parseHeader(path, "NativePacked");
    CHECK(packed.success && packed.structure.size == sizeof(NativePacked));
    const QHash<QString, QString> values{
        {"tag", "4660"}, {"address.source", "1"}, {"address.destination", "65535"},
        {"delta", "-2"}, {"active", "true"}, {"gain", "1.5"}, {"ratio", "-2.5"},
        {"extended", "1.25"}, {"flags", "5"}, {"signedBits", "-3"}, {"tail", "10"}, {"code", "20013"}
    };
    NativePacked native;
    std::memset(&native, 0, sizeof(native));
    native.tag = 4660;
    native.address.source = 1;
    native.address.destination = 65535;
    native.delta = -2;
    native.active = true;
    native.gain = 1.5f;
    native.ratio = -2.5;
    native.extended = 1.25L;
    native.flags = 5;
    native.signedBits = -3;
    native.tail = 10;
    native.code = 20013;
    QByteArray expected(reinterpret_cast<const char *>(&native), sizeof(native));
    // long double 的对象填充不是数值的一部分；预览统一补 0。
    for (int i = 10; i < 16; ++i)
        expected[int(offsetof(NativePacked, extended)) + i] = '\0';
    const BinaryResult actual = encodeStructure(packed.structure, values);
    CHECK(actual.success && actual.bytes == expected);
    QHash<QString, QString> hexValues = values;
    hexValues["tag"] = "0x1234";
    hexValues["delta"] = "-0x2";
    hexValues["flags"] = "0x5";
    hexValues["signedBits"] = "-0x3";
    hexValues["tail"] = "0xA";
    hexValues["code"] = "0x4E2D";
    const BinaryResult hexActual = encodeStructure(packed.structure, hexValues);
    CHECK(hexActual.success && hexActual.bytes == expected);
    CHECK(formatHexDump(actual.bytes).contains("\n00000010  "));
    CHECK(actual.bytes.mid(int(offsetof(NativePacked, gain)), 4).toHex() == "0000c03f");
    CHECK(actual.bytes.mid(int(offsetof(NativePacked, extended)), 16).toHex() == "00000000000000a0ff3f000000000000");

    const QString simplePath = temporary.path() + "/Simple.h";
    CHECK(writeSource(simplePath,
        "struct Padded { unsigned char marker; unsigned int value; };"
        "struct Wide { long long low; unsigned long long high; };"
        "struct FloatOnly { float gain; double ratio; };"
        "struct Inline { unsigned char before; struct { unsigned short x; } inner; };"));
    const ParseResult inlineNested = parseHeader(simplePath, "Inline");
    const BinaryResult inlineBytes = encodeStructure(inlineNested.structure, {{"before", "1"}, {"inner.x", "2"}});
    CHECK(inlineBytes.success && inlineBytes.bytes.toHex() == "01000200");
    const ParseResult padded = parseHeader(simplePath, "Padded");
    const BinaryResult padding = encodeStructure(padded.structure, {{"marker", "1"}, {"value", "2"}});
    CHECK(padding.success && padding.bytes.toHex() == "0100000002000000");
    const ParseResult wide = parseHeader(simplePath, "Wide");
    const BinaryResult limits = encodeStructure(wide.structure,
        {{"low", "-9223372036854775808"}, {"high", "18446744073709551615"}});
    CHECK(limits.success && limits.bytes.toHex() == "0000000000000080ffffffffffffffff");
    const BinaryResult hexLimits = encodeStructure(wide.structure,
        {{"low", "-0x8000000000000000"}, {"high", "0xFFFFFFFFFFFFFFFF"}});
    CHECK(hexLimits.success && hexLimits.bytes == limits.bytes);
    const ParseResult floatOnly = parseHeader(simplePath, "FloatOnly");
    const BinaryResult negativeZero = encodeStructure(floatOnly.structure, {{"gain", "-0"}, {"ratio", "-0.0"}});
    CHECK(negativeZero.success && negativeZero.bytes.left(4).toHex() == "00000080");
    CHECK(negativeZero.bytes.mid(8, 8).toHex() == "0000000000000080");
    CHECK(encodeStructure(floatOnly.structure, {{"gain", "1e-40"}, {"ratio", "1e-310"}}).success);
    const BinaryResult rounding = encodeStructure(floatOnly.structure,
        {{"gain", "1.0000000596046447753906250001"},
         {"ratio", "1.0000000000000001110223024625156540423631668090820312501"}});
    CHECK(rounding.success && rounding.bytes.left(4).toHex() == "0100803f");
    CHECK(rounding.bytes.mid(8, 8).toHex() == "010000000000f03f");
    CHECK(!encodeStructure(nested.structure, {{"sequence", "1"}}).success);
    QHash<QString, QString> invalidValues = nestedValues;
    invalidValues["payloadLength"] = "65536";
    CHECK(!encodeStructure(nested.structure, invalidValues).success);
    StructInfo badLayout = nested.structure;
    badLayout.fields.first().absoluteOffsetBits = badLayout.size * 8;
    CHECK(!encodeStructure(badLayout, nestedValues).success);
    badLayout = nested.structure;
    FieldInfo duplicate = badLayout.fields.first();
    duplicate.name = "duplicate";
    badLayout.fields.append(duplicate);
    invalidValues = nestedValues;
    invalidValues.insert("duplicate", "1");
    CHECK(!encodeStructure(badLayout, invalidValues).success);

    CHECK(writeSource(simplePath,
        "struct Pointer { int *next; }; struct Ref { int &value; };"
        "struct Array { int items[2]; }; struct Const { const int fixed; };"
        "struct Choice { union Data { int a; float b; } value; };"
        "struct Base { int x; }; struct Derived : Base { int y; };"
        "struct Virtual { virtual ~Virtual(); int x; };"
        "struct Anonymous { union { int a; float b; }; };"));
    for (const QString &name : {"Pointer", "Ref", "Array", "Const", "Choice", "Derived", "Virtual", "Anonymous"}) {
        const ParseResult unsupported = parseHeader(simplePath, name);
        CHECK(unsupported.success);
        const BinaryResult failure = encodeStructure(unsupported.structure, {});
        CHECK(!failure.success && failure.bytes.isEmpty() && !failure.diagnostics.isEmpty());
    }

    MainWindow window(project + "/NestedPacketHeader.h");
    QLineEdit *name = window.findChild<QLineEdit *>("structNameInput");
    QTreeWidget *tree = window.findChild<QTreeWidget *>("fieldsTree");
    QPushButton *button = window.findChild<QPushButton *>("viewBinaryButton");
    CHECK(button && !button->isEnabled());
    name->setText("NestedPacketHeader");
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    CHECK(button->isEnabled());
    QTreeWidgetItem *root = tree->topLevelItem(0);
    auto *sequence = qobject_cast<QLineEdit *>(tree->itemWidget(root->child(0), 1));
    CHECK(sequence);
    sequence->setText("0x100000000");
    CHECK(!button->isEnabled());
    sequence->setText("0x");
    CHECK(!button->isEnabled());
    sequence->setText(nestedHexValues.value("sequence"));
    auto *source = qobject_cast<QLineEdit *>(tree->itemWidget(root->child(1)->child(0), 1));
    auto *destination = qobject_cast<QLineEdit *>(tree->itemWidget(root->child(1)->child(1), 1));
    auto *length = qobject_cast<QLineEdit *>(tree->itemWidget(root->child(2), 1));
    source->setText(nestedHexValues.value("address.source"));
    destination->setText(nestedHexValues.value("address.destination"));
    length->setText(nestedHexValues.value("payloadLength"));
    CHECK(button->isEnabled());
    button->click();
    QDialog *dialog = window.findChild<QDialog *>("binaryPreviewDialog");
    CHECK(dialog);
    QPlainTextEdit *hex = dialog->findChild<QPlainTextEdit *>("binaryHexView");
    CHECK(hex && hex->isReadOnly() && hex->toPlainText() == formatHexDump(nestedBytes.bytes));
    dialog->findChild<QPushButton *>("copyHexButton")->click();
    CHECK(QApplication::clipboard()->text() == "78 56 34 12 22 11 44 33 66 55");
    app.processEvents();
    if (app.arguments().size() > 1)
        CHECK(dialog->grab().save(app.arguments().at(1)));
    dialog->close();
    sequence->setText("1");
    button->click();
    app.sendPostedEvents(nullptr, QEvent::DeferredDelete);
    dialog = window.findChild<QDialog *>("binaryPreviewDialog");
    CHECK(dialog && dialog->findChild<QPlainTextEdit *>("binaryHexView")->toPlainText().startsWith("00000000  01 00 00 00"));
    dialog->close();
    auto *file = window.findChild<QLineEdit *>("headerFilePath");
    file->setText(simplePath);
    name->setText("Pointer");
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    button->click();
    CHECK(window.findChild<QPlainTextEdit *>()->toPlainText().contains("next"));
    QTextStream(stdout) << "PASS: known hex, native packed layout, nested fields, integer limits,"
                          " signed bitfields, floats/long double, padding, unsupported layouts and preview/copy UI\n";
    return 0;
}
