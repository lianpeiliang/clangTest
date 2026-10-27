#include "fieldvalue.h"
#include "mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTreeWidget>

#define CHECK(condition) do { if (!(condition)) { \
    QTextStream(stderr) << "FAIL at line " << __LINE__ << ": " << #condition << '\n'; return 1; \
} } while (false)

static bool writeSource(const QString &path, const QByteArray &source)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(source) == source.size();
}

static const FieldInfo &field(const QVector<FieldInfo> &fields, const QString &path)
{
    const QString name = path.section('.', 0, 0);
    for (const FieldInfo &info : fields) {
        if (info.name == name)
            return path.contains('.') ? field(info.children, path.section('.', 1)) : info;
    }
    qFatal("Missing fixture field");
}

static QTreeWidgetItem *item(QTreeWidgetItem *parent, const QString &path)
{
    const QString name = path.section('.', 0, 0);
    for (int index = 0; index < parent->childCount(); ++index) {
        QTreeWidgetItem *child = parent->child(index);
        if (child->text(0) == name)
            return path.contains('.') ? item(child, path.section('.', 1)) : child;
    }
    qFatal("Missing tree item");
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    if (QApplication::platformName() == "offscreen") {
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    }
#endif
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    const QString sourcePath = temporary.path() + "/Values.h";
    const QByteArray source =
        "typedef unsigned short Code;\n"
        "struct Address { Code code; bool active; }; struct Empty {};\n"
        "struct Values { signed char small; unsigned char byte; long large; "
        "long long signedWide; unsigned long long wide; bool enabled; float gain; "
        "double precise; long double extended; Address address; Address destination; "
        "Values *next; int &reference; int array[2]; const short fixed; const Address locked; "
        "Empty empty; unsigned mask:3; signed signedBits:4; const unsigned constBits:3; "
        "wchar_t character; enum Mode { A, B } mode; };";
    CHECK(writeSource(sourcePath, source));
    const ParseResult parsed = parseHeader(sourcePath, "Values");
    CHECK(parsed.success);
    const QVector<FieldInfo> &fields = parsed.structure.fields;
    CHECK(field(fields, "small").valueKind == FieldValueKind::SignedInteger);
    CHECK(field(fields, "large").valueBits == 32);
    CHECK(field(fields, "wide").valueBits == 64);
    CHECK(field(fields, "mask").valueBits == 3);
    CHECK(field(fields, "signedBits").valueBits == 4);
    CHECK(field(fields, "address.code").valueKind == FieldValueKind::UnsignedInteger);
    CHECK(field(fields, "character").valueKind == FieldValueKind::UnsignedInteger);
    for (const QString &path : {"next", "reference", "array", "fixed", "locked.code", "empty", "mode", "constBits"})
        CHECK(!isEditableField(field(fields, path)));

    auto accepts = [&fields](const QString &path, const QString &text) {
        return validateFieldValue(field(fields, path), text).valid;
    };
    CHECK(accepts("small", "-128") && accepts("small", "127"));
    CHECK(!accepts("small", "-129") && !accepts("small", "128"));
    CHECK(accepts("byte", "255") && !accepts("byte", "256") && !accepts("byte", "-1"));
    CHECK(accepts("large", "2147483647") && !accepts("large", "2147483648"));
    CHECK(accepts("signedWide", "-9223372036854775808") && accepts("signedWide", "9223372036854775807"));
    CHECK(!accepts("signedWide", "-9223372036854775809") && !accepts("signedWide", "9223372036854775808"));
    CHECK(accepts("wide", "18446744073709551615") && !accepts("wide", "18446744073709551616"));
    CHECK(validateFieldValue(field(fields, "wide"), "18446744073709551615").value == "18446744073709551615");
    CHECK(accepts("mask", "7") && !accepts("mask", "8"));
    CHECK(accepts("signedBits", "-8") && accepts("signedBits", "7"));
    CHECK(!accepts("signedBits", "-9") && !accepts("signedBits", "8"));
    CHECK(accepts("character", "65535") && !accepts("character", "65536"));
    CHECK(accepts("enabled", "true") && accepts("enabled", "false") && !accepts("enabled", "2"));
    CHECK(validateFieldValue(field(fields, "small"), " +0012 ").value == "12");
    CHECK(accepts("address.code", "0xFFFF") && !accepts("address.code", "0x10000"));
    CHECK(accepts("small", "-0x80") && accepts("small", "0x7f") && !accepts("small", "0x80"));
    CHECK(accepts("large", "0x7FFFFFFF") && !accepts("large", "0x80000000"));
    CHECK(!accepts("large", "0xFFFFFFFF")); // 按数值解释，不自动按补码转换成负数。
    CHECK(accepts("large", "-0x80000000") && !accepts("large", "-0x80000001"));
    CHECK(accepts("signedWide", "0x7FFFFFFFFFFFFFFF") && accepts("signedWide", "-0x8000000000000000"));
    CHECK(!accepts("signedWide", "0x8000000000000000") && !accepts("signedWide", "-0x8000000000000001"));
    CHECK(accepts("wide", "0xFFFFFFFFFFFFFFFF") && !accepts("wide", "0x10000000000000000"));
    CHECK(validateFieldValue(field(fields, "wide"), "0xFFFFFFFFFFFFFFFF").value == "18446744073709551615");
    CHECK(validateFieldValue(field(fields, "signedWide"), "-0x8000000000000000").value == "-9223372036854775808");
    CHECK(validateFieldValue(field(fields, "byte"), " +0XfF ").value == "255");
    CHECK(validateFieldValue(field(fields, "byte"), "010").value == "10");
    CHECK(!accepts("byte", "-0x1") && !accepts("byte", "-0x0"));
    CHECK(accepts("mask", "0x7") && !accepts("mask", "0x8"));
    CHECK(accepts("signedBits", "-0x8") && !accepts("signedBits", "-0x9"));
    CHECK(accepts("character", "0xFFFF") && !accepts("character", "0x10000"));
    CHECK(!accepts("enabled", "0x1") && !accepts("gain", "0x1p2"));
    for (const QString &bad : {"FF", "0x", "-0x", "+0X", "0xG1", "0x1.5", "0x1p2", "0x1e+2",
                              "0b10", "0xFFu", "0x FF", "0x-1", "0x+1"})
        CHECK(!accepts("address.code", bad));
    for (const QString &bad : {"", " ", "1.5", "1e2", "0xff", "12abc"})
        CHECK(!accepts("small", bad));
    for (const QString &good : {"1", "-1.25", ".5", "1.", "+2e-3", "0e9999"})
        CHECK(accepts("gain", good));
    for (const QString &bad : {"", "1e", "nan", "inf", "1,5", "1.2.3", "3.5e38", "1e-46"})
        CHECK(!accepts("gain", bad));
    CHECK(accepts("precise", "1e308") && !accepts("precise", "1e309") && !accepts("precise", "1e-400"));
    CHECK(accepts("extended", "1e4000") && !accepts("extended", "1e5000"));

    MainWindow window(sourcePath);
    QLineEdit *name = window.findChild<QLineEdit *>("structNameInput");
    QLineEdit *filePath = window.findChild<QLineEdit *>("headerFilePath");
    QTreeWidget *tree = window.findChild<QTreeWidget *>("fieldsTree");
    name->setText("Values");
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    CHECK(tree->columnCount() == 10 && tree->topLevelItemCount() == 1);
    QTreeWidgetItem *root = tree->topLevelItem(0);
    auto editor = [tree, &root](const QString &path) {
        return qobject_cast<QLineEdit *>(tree->itemWidget(item(root, path), 1));
    };
    CHECK(editor("byte") && editor("byte")->text() == "0");
    CHECK(window.fieldValues().value("address.code") == "0");
    for (const QString &path : {"next", "reference", "array", "fixed", "locked.code", "empty", "mode", "address"})
        CHECK(!tree->itemWidget(item(root, path), 1));
    QComboBox *enabled = qobject_cast<QComboBox *>(tree->itemWidget(item(root, "enabled"), 1));
    CHECK(enabled && enabled->count() == 2 && !enabled->isEditable());
    enabled->setCurrentText("true");
    CHECK(window.fieldValues().value("enabled") == "true");
    editor("byte")->setText("255");
    CHECK(window.fieldValues().value("byte") == "255");
    editor("byte")->setText("0xFF");
    CHECK(window.fieldValues().value("byte") == "0xFF" && !window.hasInvalidFieldValues());
    editor("byte")->setText("0x100");
    CHECK(window.fieldValues().value("byte") == "0xFF" && window.hasInvalidFieldValues());
    CHECK(!editor("byte")->styleSheet().isEmpty());
    CHECK(window.findChild<QLabel *>("fieldValueStatus")->text().contains("byte"));
    editor("gain")->setText("nan");
    editor("byte")->setText("12");
    CHECK(window.hasInvalidFieldValues()); // 其他字段合法时仍保留未解决的错误。
    editor("gain")->setText("1.25e-3");
    CHECK(!window.hasInvalidFieldValues());
    editor("wide")->setText("0xFFFFFFFFFFFFFFFF");
    editor("address.code")->setText("0x7B");
    editor("destination.code")->setText("456");
    CHECK(window.fieldValues().value("address.code") == "0x7B");
    CHECK(window.fieldValues().value("destination.code") == "456");
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    root = tree->topLevelItem(0);
    CHECK(window.fieldValues().value("enabled") == "true");
    CHECK(editor("wide")->text() == "0xFFFFFFFFFFFFFFFF");
    CHECK(editor("address.code")->text() == "0x7B");
    CHECK(editor("byte")->text() == "12");
    CHECK(writeSource(sourcePath, QByteArray(source).replace("unsigned char byte;", "unsigned byte:2;")));
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    root = tree->topLevelItem(0);
    CHECK(editor("byte")->text() == "0"); // 类型变化后重新校验，旧值超范围则初始化。
    CHECK(editor("address.code")->text() == "0x7B");
    editor("byte")->setText("3");
    window.show();
    app.processEvents();
    if (app.arguments().size() > 1)
        CHECK(window.grab().save(app.arguments().at(1)));

    name->setText("Address");
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    CHECK(!window.fieldValues().contains("address.code") && window.fieldValues().value("code") == "0");
    const QString otherPath = temporary.path() + "/Other.h";
    CHECK(writeSource(otherPath, "struct Address { short code; };"));
    filePath->setText(otherPath);
    CHECK(QMetaObject::invokeMethod(filePath, "returnPressed"));
    CHECK(window.fieldValues().value("code") == "0");
    name->setText("Missing");
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    CHECK(tree->topLevelItemCount() == 0 && window.fieldValues().isEmpty());
    QTextStream(stdout) << "PASS: decimal/hex integer and bitfield limits, 64-bit precision, floating formats/ranges,"
                          " bool dropdown, readonly types, nested paths, validation feedback and reload\n";
    return 0;
}
