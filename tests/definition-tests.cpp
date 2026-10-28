#include "definitionjson.h"
#include "mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>

#define CHECK(condition) do { if (!(condition)) { \
    QTextStream(stderr) << "FAIL at line " << __LINE__ << ": " << #condition << '\n'; return 1; \
} } while (false)

static bool hasNoValues(const QJsonValue &value)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (object.contains("value") || object.contains("values"))
            return false;
        for (auto it = object.begin(); it != object.end(); ++it)
            if (!hasNoValues(it.value()))
                return false;
    } else if (value.isArray()) {
        for (const QJsonValue &child : value.toArray())
            if (!hasNoValues(child))
                return false;
    }
    return true;
}

static QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

static void answerSaveDialog(const QString &path)
{
    QTimer::singleShot(0, [path] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *dialog = qobject_cast<QFileDialog *>(widget);
            if (!dialog || !dialog->isVisible())
                continue;
            if (path.isEmpty()) {
                dialog->reject();
            } else {
                dialog->selectFile(path);
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
            }
            return;
        }
        qFatal("Save dialog not found");
    });
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    if (QApplication::platformName() == "offscreen") {
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    }
#endif
    const QString testHeaders = QStringLiteral(MSG_HEADER_PATH).section('/', 0, -2);
    const ParseResult nested = parseHeader(testHeaders + "/NestedPacketHeader.h", "NestedPacketHeader");
    CHECK(nested.success);
    const QJsonObject root = structureDefinitionJson(nested.structure).object();
    CHECK(root.value("name") == "NestedPacketHeader" && root.value("size").toInt() == 10);
    CHECK(root.value("alignment").toInt() == 1 && root.value("schemaVersion").toInt() == 1);
    CHECK(root.value("target") == "x86_64-w64-windows-gnu" && root.value("byteOrder") == "little");
    const QJsonArray fields = root.value("fields").toArray();
    CHECK(fields.size() == 3);
    const QJsonObject address = fields.at(1).toObject();
    CHECK(address.value("name") == "address" && address.value("offsetBits").toInt() == 32);
    const QJsonObject destination = address.value("fields").toArray().at(1).toObject();
    CHECK(destination.value("offsetBits").toInt() == 16 && destination.value("absoluteOffsetBits").toInt() == 48);
    CHECK(hasNoValues(root));
    const ParseResult flat = parseHeader(testHeaders + "/MsgHeader.h");
    CHECK(flat.success && structureDefinitionJson(flat.structure).object().value("fields").toArray().size() == 21);

    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    const QString fixturePath = temporary.path() + "/Definition.h";
    QFile fixture(fixturePath);
    CHECK(fixture.open(QIODevice::WriteOnly));
    fixture.write("struct Leaf { unsigned short code; }; typedef unsigned char Bytes[3]; "
                  "struct Definition { unsigned count; Leaf leaf; Bytes data; unsigned mode:3; "
                  "const int fixed; int *pointer; };");
    fixture.close();
    const ParseResult parsed = parseHeader(fixturePath, "Definition");
    CHECK(parsed.success);
    const QJsonDocument expected = structureDefinitionJson(parsed.structure);
    const QJsonArray fixtureFields = expected.object().value("fields").toArray();
    CHECK(fixtureFields.size() == 6);
    CHECK(fixtureFields.at(2).toObject().value("isArray").toBool());
    CHECK(fixtureFields.at(2).toObject().value("size").toInt() == 3);
    CHECK(!fixtureFields.at(2).toObject().value("editable").toBool());
    CHECK(fixtureFields.at(2).toObject().value("fields").toArray().isEmpty());
    CHECK(fixtureFields.at(2).toObject().value("type") == "Bytes");
    CHECK(fixtureFields.at(2).toObject().value("canonicalType").toString().contains("unsigned char"));
    CHECK(fixtureFields.at(3).toObject().value("bitWidth").toInt() == 3);
    CHECK(!fixtureFields.at(4).toObject().value("editable").toBool());
    CHECK(!fixtureFields.at(5).toObject().value("editable").toBool());
    CHECK(hasNoValues(expected.object()));

    const QString output = temporary.path() + QStringLiteral("/结构定义.json");
    QString error;
    CHECK(saveStructureDefinitionJson(parsed.structure, output, &error) && error.isEmpty());
    QJsonParseError parseError;
    CHECK(QJsonDocument::fromJson(readFile(output), &parseError) == expected);
    CHECK(parseError.error == QJsonParseError::NoError);
    CHECK(!saveStructureDefinitionJson(parsed.structure, temporary.path() + "/missing/out.json", &error));
    CHECK(!error.isEmpty());
    StructInfo large;
    large.name = QStringLiteral("Large");
    large.size = 9007199254740993LL;
    CHECK(structureDefinitionJson(large).object().value("size").toString() == "9007199254740993");

    MainWindow window(fixturePath);
    auto *name = window.findChild<QComboBox *>("structSelector");
    auto *tree = window.findChild<QTreeWidget *>("fieldsTree");
    auto *button = window.findChild<QPushButton *>("exportDefinitionButton");
    CHECK(button && button->isEnabled());
    name->setCurrentText("Definition");
    CHECK(button->isEnabled());
    auto *count = qobject_cast<QLineEdit *>(tree->itemWidget(tree->topLevelItem(0)->child(0), 1));
    count->setText("0xDEADBEEF");
    const QString uiOutput = temporary.path() + "/ui.json";
    answerSaveDialog(uiOutput);
    button->click();
    CHECK(QJsonDocument::fromJson(readFile(uiOutput)) == expected);
    count->setText("invalid-input");
    CHECK(window.hasInvalidFieldValues() && button->isEnabled());
    const QString noExtension = temporary.path() + "/without-extension";
    answerSaveDialog(noExtension);
    button->click();
    CHECK(QJsonDocument::fromJson(readFile(noExtension + ".json")) == expected);
    answerSaveDialog(QString());
    button->click();
    CHECK(QJsonDocument::fromJson(readFile(uiOutput)) == expected);
    CHECK(fixture.open(QIODevice::WriteOnly | QIODevice::Truncate));
    fixture.write("struct Forward;");
    fixture.close();
    window.findChild<QPushButton *>("reloadDefinitionButton")->click();
    CHECK(!button->isEnabled());
    if (app.arguments().size() > 1)
        CHECK(saveStructureDefinitionJson(nested.structure, app.arguments().at(1), &error));
    QTextStream(stdout) << "PASS: nested/flat JSON metadata, arrays, aliases, bitfields, no values,"
                          " exact large layout integers, UTF-8 save, export button/dialog, cancel and errors\n";
    return 0;
}
