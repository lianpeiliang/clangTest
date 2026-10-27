#include "headerparser.h"
#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QFontDatabase>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTreeWidget>

#define CHECK(condition) do { if (!(condition)) { \
    qCritical() << "FAIL at line" << __LINE__ << #condition; return 1; \
} } while (false)

static bool writeSource(const QString &path, const QByteArray &source)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(source) == source.size();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    // Windows 的 offscreen 插件不自动枚举系统字体，截图检查时显式加载。
    if (QApplication::platformName() == "offscreen") {
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    }
#endif
    const QString project = QStringLiteral(MSG_HEADER_PATH).section('/', 0, -2);
    const ParseResult flat = parseHeader(project + "/MsgHeader.h");
    CHECK(flat.success && flat.structure.fields.size() == 21);
    CHECK(flat.structure.size == 83 && flat.structure.alignment == 1);
    CHECK(flat.structure.fields.last().absoluteOffsetBits == 632);
    CHECK(flat.structure.fields.last().children.isEmpty());

    const QString nestedPath = project + "/NestedPacketHeader.h";
    const ParseResult nested = parseHeader(nestedPath, "NestedPacketHeader");
    CHECK(nested.success && nested.structure.fields.size() == 3);
    CHECK(nested.structure.size == 10 && nested.structure.alignment == 1);
    const FieldInfo &address = nested.structure.fields.at(1);
    CHECK(address.name == "address" && address.offsetBits == 32);
    CHECK(address.children.size() == 2);
    CHECK(address.children.at(1).name == "destination");
    CHECK(address.children.at(1).offsetBits == 16);
    CHECK(address.children.at(1).absoluteOffsetBits == 48);

    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    const QString fixture = temporary.path() + "/fixture.h";
    CHECK(writeSource(fixture,
        "#pragma pack(push, 1)\n"
        "struct Leaf { unsigned char a; unsigned short b; };\n"
        "using Alias = Leaf;\n"
        "struct Middle { unsigned char marker; Alias payload; Middle *next; "
        "Leaf &reference; Leaf array[2]; };\n"
        "struct Deep { unsigned short prefix; Middle middle; Alias repeated; };\n"
        "struct BitLeaf { unsigned a:3; unsigned b:5; };\n"
        "struct BitRoot { unsigned char prefix; BitLeaf bits; };\n"
        "#pragma pack(pop)\n"));
    const ParseResult deep = parseHeader(fixture, "Deep");
    CHECK(deep.success && deep.structure.fields.size() == 3);
    CHECK(deep.structure.size == 31);
    const QVector<FieldInfo> &middle = deep.structure.fields.at(1).children;
    CHECK(middle.size() == 5 && middle.at(1).children.size() == 2);
    CHECK(middle.at(1).children.at(1).offsetBits == 8);
    CHECK(middle.at(1).children.at(1).absoluteOffsetBits == 32);
    CHECK(middle.at(2).children.isEmpty()); // 自引用指针。
    CHECK(middle.at(3).children.isEmpty()); // 引用。
    CHECK(middle.at(4).children.isEmpty()); // 数组。
    CHECK(deep.structure.fields.at(2).children.size() == 2); // 同一类型重复出现。

    const ParseResult bits = parseHeader(fixture, "BitRoot");
    CHECK(bits.success && bits.structure.fields.at(1).children.size() == 2);
    CHECK(bits.structure.fields.at(1).children.at(1).offsetBits == 3);
    CHECK(bits.structure.fields.at(1).children.at(1).absoluteOffsetBits == 11);

    // 主结构体仍从主文件选择，嵌套类型允许定义在被包含的文件中。
    CHECK(writeSource(temporary.path() + "/included.h", "struct Included { short value; };"));
    const QString including = temporary.path() + "/including.h";
    CHECK(writeSource(including,
        "#include \"included.h\"\nnamespace demo { struct MsgHeader; "
        "struct MsgHeader { unsigned char marker; Included data; }; }"));
    const ParseResult included = parseHeader(including);
    CHECK(included.success && included.structure.fields.at(1).children.size() == 1);
    CHECK(included.structure.fields.at(1).children.first().absoluteOffsetBits == 16);
    CHECK(!parseHeader(including, "Included").success);

    MainWindow window(nestedPath);
    QLineEdit *name = window.findChild<QLineEdit *>("structNameInput");
    QLineEdit *path = window.findChild<QLineEdit *>("headerFilePath");
    QTreeWidget *tree = window.findChild<QTreeWidget *>("fieldsTree");
    CHECK(name && path && tree);
    CHECK(name->text() == "MsgHeader" && tree->topLevelItemCount() == 0);
    name->setText("  NestedPacketHeader  ");
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    CHECK(tree->topLevelItemCount() == 1);
    QTreeWidgetItem *root = tree->topLevelItem(0);
    CHECK(root->text(0) == "NestedPacketHeader" && root->childCount() == 3);
    QTreeWidgetItem *addressItem = root->child(1);
    CHECK(addressItem->childCount() == 2 && addressItem->isExpanded());
    CHECK(addressItem->child(1)->text(5) == "2");
    CHECK(addressItem->child(1)->text(6) == "6");
    for (QPushButton *button : window.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("全部折叠")) {
            button->click();
            CHECK(!root->isExpanded() && !addressItem->isExpanded());
        }
    }
    for (QPushButton *button : window.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("全部展开")) {
            button->click();
            CHECK(root->isExpanded() && addressItem->isExpanded());
        }
    }
    window.show();
    app.processEvents();
    if (app.arguments().size() > 1)
        CHECK(window.grab().save(app.arguments().at(1)));

    path->setText(fixture);
    name->setText("BitRoot");
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    QTreeWidgetItem *bitItem = tree->topLevelItem(0)->child(1)->child(1);
    CHECK(bitItem->text(5) == "0 + 3 bit" && bitItem->text(6) == "1 + 3 bit");
    name->setText("Missing");
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    CHECK(tree->topLevelItemCount() == 0);
    name->setText(" ");
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    CHECK(tree->topLevelItemCount() == 0);
    CHECK(!window.findChild<QPlainTextEdit *>()->toPlainText().isEmpty());
    CHECK(writeSource(fixture, "struct MsgHeader { UnknownType broken; };"));
    CHECK(!parseHeader(fixture).success);
    CHECK(!parseHeader(temporary.path() + "/missing.h").success);
    path->setText(project + "/MsgHeader.h");
    name->setText("MsgHeader");
    CHECK(QMetaObject::invokeMethod(name, "returnPressed"));
    CHECK(tree->topLevelItemCount() == 1 && tree->topLevelItem(0)->childCount() == 21);
    QTextStream(stdout) << "PASS: flat/nested layouts, aliases, deep offsets, pointer/reference/array leaves,"
                          " bitfields, included types, tree expansion, reload and errors\n";
    return 0;
}
