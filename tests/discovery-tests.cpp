#include "headerparser.h"
#include "UI/mainwindow.h"
#include "definitionjson.h"

#include <QApplication>
#include <QComboBox>
#include <QDebug>
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
    qCritical() << "FAIL at line" << __LINE__ << #condition; return 1; \
} } while (false)

static bool write(const QString &path, const QByteArray &source)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(source) == source.size();
}

static QStringList names(const HeaderParseResult &header)
{
    QStringList result;
    for (const ParseResult &structure : header.structures)
        result.append(structure.structure.name);
    return result;
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
    const QString testHeaders = QStringLiteral(MSG_HEADER_PATH).section('/', 0, -2);
    const HeaderParseResult single = parseHeaderStructures(testHeaders + "/PacketHeader.h");
    CHECK(single.success && single.structures.size() == 1);
    CHECK(single.structures.first().success && single.structures.first().structure.name == "PacketHeader");
    CHECK(parseHeader(testHeaders + "/PacketHeader.h").structure.size == 7);
    const QString twentyPath = testHeaders + "/MultiStructHeader.h";
    const HeaderParseResult twenty = parseHeaderStructures(twentyPath);
    CHECK(twenty.success && twenty.structures.size() == 20);
    for (int i = 0; i < 20; ++i) {
        CHECK(twenty.structures.at(i).success);
        CHECK(twenty.structures.at(i).structure.name == QString("Packet%1").arg(i + 1, 2, 10, QLatin1Char('0')));
        CHECK(twenty.structures.at(i).structure.fields.size() == 2);
    }
    const ParseResult multiple = parseHeader(twentyPath);
    CHECK(!multiple.success && multiple.diagnostics.join(' ').contains("Packet20"));

    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    const QString fixture = temporary.path() + "/Discovery.h";
    CHECK(write(temporary.path() + "/Included.h", "struct Included { short value; };"));
    CHECK(write(fixture,
        "#include \"Included.h\"\n"
        "// struct Fake { int ignored; };\n"
        "#if 0\nstruct Disabled { int ignored; };\n#endif\n"
        "struct Forward; struct Forward { Included child; };\n"
        "namespace a { struct Same { unsigned char value; }; }\n"
        "namespace b { struct Same { unsigned short value; }; }\n"
        "struct Outer { struct Inner { int value; }; Inner child; };\n"
        "typedef struct { unsigned code; } TypedefPacket;\n"
        "using UsingPacket = struct { unsigned code; };\n"
        "struct WithAnonymous { struct { int value; } member; };\n"
        "using Alias = Forward; typedef Forward OtherAlias;\n"
        "template<class T> struct Generic { T value; struct Dependent { T value; }; };\n"
        "class ClassOnly {}; union UnionOnly { int value; };\n"
        "inline void f() { struct Local { int value; }; }\n"));
    const HeaderParseResult discovered = parseHeaderStructures(fixture);
    CHECK(discovered.success);
    CHECK(names(discovered) == QStringList({"Forward", "a::Same", "b::Same", "Outer", "Outer::Inner",
                                          "TypedefPacket", "UsingPacket", "WithAnonymous"}));
    CHECK(discovered.structures.first().structure.fields.first().children.size() == 1);
    CHECK(parseHeader(fixture, "a::Same").structure.size == 1);
    CHECK(parseHeader(fixture, "b::Same").structure.size == 2);
    CHECK(parseHeader(fixture, " Inner ").structure.size == 4);
    CHECK(parseHeader(fixture, "TypedefPacket").success);
    CHECK(!parseHeader(fixture, "Same").success);
    CHECK(!parseHeader(fixture, "Included").success);

    // 单个布局不可用时仍保留完整候选列表，其他结构体可以正常选择。
    const QString partial = temporary.path() + "/Partial.h";
    CHECK(write(partial, "struct Good { int value; }; struct Abstract { virtual void f() = 0; }; "
                         "struct Broken { Abstract value; };"));
    // 语法/语义错误会阻止整个文件，不能向用户提供错误布局。
    CHECK(!parseHeaderStructures(partial).success);
    CHECK(write(partial, "struct Good { int value; }; struct Flexible { int count; char data[]; };"));
    const HeaderParseResult flexible = parseHeaderStructures(partial);
    CHECK(flexible.success && flexible.structures.size() == 2);
    CHECK(flexible.structures.first().success && !flexible.structures.last().success);
    CHECK(parseHeader(partial, "Good").success);

    MainWindow window(twentyPath);
    auto *selector = window.findChild<QComboBox *>("structSelector");
    auto *path = window.findChild<QLineEdit *>("headerFilePath");
    auto *tree = window.findChild<QTreeWidget *>("fieldsTree");
    auto *reload = window.findChild<QPushButton *>("reloadDefinitionButton");
    auto *exportButton = window.findChild<QPushButton *>("exportDefinitionButton");
    CHECK(selector && selector->count() == 20 && selector->isEnabled() && !selector->isEditable());
    CHECK(window.findChild<QLabel *>("definitionSourceInfo")->text().contains(QStringLiteral("20 个")));
    for (int i = 0; i < selector->count(); ++i) {
        selector->setCurrentIndex(i);
        CHECK(tree->topLevelItemCount() == 1 && tree->topLevelItem(0)->text(0) == selector->currentText());
    }
    auto *sequence = qobject_cast<QLineEdit *>(tree->itemWidget(tree->topLevelItem(0)->child(0), 1));
    sequence->setText("0x1234");
    reload->click();
    CHECK(selector->currentText() == "Packet20" && window.fieldValues().value("sequence") == "0x1234");
    selector->setCurrentIndex(0);
    CHECK(window.fieldValues().value("sequence") == "0");

    path->setText(testHeaders + "/PacketHeader.h");
    reload->click();
    CHECK(selector->count() == 1 && !selector->isEnabled() && selector->currentText() == "PacketHeader");
    CHECK(tree->topLevelItem(0)->childCount() == 3);
    path->setText(fixture);
    reload->click();
    selector->setCurrentText("b::Same");
    CHECK(tree->topLevelItem(0)->text(4) == "2");
    CHECK(write(fixture, "struct Replacement { bool enabled; };"));
    reload->click();
    CHECK(selector->count() == 1 && selector->currentText() == "Replacement");
    CHECK(!window.fieldValues().contains("value"));

    path->setText(partial);
    reload->click();
    selector->setCurrentText("Flexible");
    CHECK(tree->topLevelItemCount() == 0 && !exportButton->isEnabled());
    selector->setCurrentText("Good");
    CHECK(tree->topLevelItemCount() == 1 && exportButton->isEnabled());

    const QString json = temporary.path() + "/Packet.json";
    CHECK(saveStructureDefinitionJson(single.structures.first().structure, json));
    path->setText(json);
    reload->click();
    CHECK(selector->count() == 1 && !selector->isEnabled() && selector->currentText() == "PacketHeader");
    path->setText(partial);
    reload->click();
    CHECK(selector->currentText() == "Good");

    CHECK(write(fixture, "struct OnlyForward; // struct Fake {};\n"));
    path->setText(fixture);
    reload->click();
    CHECK(selector->count() == 0 && !selector->isEnabled() && tree->topLevelItemCount() == 0);
    CHECK(!exportButton->isEnabled() && window.fieldValues().isEmpty());
    CHECK(!window.findChild<QPlainTextEdit *>()->toPlainText().isEmpty());
    CHECK(write(fixture, "struct SyntaxError { UnknownType value; };"));
    reload->click();
    CHECK(selector->count() == 0 && tree->topLevelItemCount() == 0);
    path->setText(temporary.path() + "/Missing.h");
    reload->click();
    CHECK(selector->count() == 0 && tree->topLevelItemCount() == 0);
    path->setText(twentyPath);
    reload->click();
    window.show();
    app.processEvents();
    if (app.arguments().size() > 1)
        CHECK(window.grab().save(app.arguments().at(1)));
    QTextStream(stdout) << "PASS: single/20 structs, namespaces, nested definitions, typedef, aliases, "
                          "main-file filtering, forward declarations, inactive code, independent layouts, "
                          "selector/reload, values, JSON and failures\n";
    return 0;
}
