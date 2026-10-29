#include "definitionjson.h"
#include "binaryencoder.h"
#include "UI/mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>

#define CHECK(condition) do { if (!(condition)) { \
    QTextStream(stderr) << "FAIL at line " << __LINE__ << ": " << #condition << '\n'; return 1; \
} } while (false)

static bool write(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

static bool rejects(const QJsonObject &object)
{
    const ParseResult result = parseStructureDefinitionJson(QJsonDocument(object).toJson());
    return !result.success && !result.diagnostics.isEmpty() && result.structure.fields.isEmpty();
}

static QJsonObject changeField(QJsonObject root, int index, const QString &key, const QJsonValue &value)
{
    QJsonArray fields = root.value("fields").toArray();
    QJsonObject field = fields.at(index).toObject();
    field.insert(key, value);
    fields.replace(index, field);
    root.insert("fields", fields);
    return root;
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
    for (const QString &file : {"MsgHeader.h", "SimpleMsgHeader.h", "MinimalMsgHeader.h", "PacketHeader.h", "NestedPacketHeader.h"}) {
        const QString name = file == "PacketHeader.h" ? "PacketHeader"
            : file == "NestedPacketHeader.h" ? "NestedPacketHeader" : "MsgHeader";
        const ParseResult parsed = parseHeader(testHeaders + '/' + file, name);
        CHECK(parsed.success);
        const QJsonDocument definition = structureDefinitionJson(parsed.structure);
        const ParseResult imported = parseStructureDefinitionJson(definition.toJson());
        CHECK(imported.success);
        CHECK(structureDefinitionJson(imported.structure) == definition);
    }
    const ParseResult nested = parseHeader(testHeaders + "/NestedPacketHeader.h", "NestedPacketHeader");
    const QJsonObject root = structureDefinitionJson(nested.structure).object();
    CHECK(!parseStructureDefinitionJson("{").success);
    CHECK(!parseStructureDefinitionJson("[]").success);
    CHECK(rejects(QJsonObject{{"name", "ordinary JSON"}}));
    for (const auto &pair : QList<QPair<QString, QJsonValue>>{
         {"schemaVersion", 2}, {"target", "aarch64-linux-gnu"}, {"byteOrder", "big"},
         {"size", -1}, {"size", 0}, {"size", 1.5}, {"size", 9007199254740992.0},
         {"alignment", 3}, {"alignment", 16}, {"fields", "not-array"}, {"values", QJsonObject()}}) {
        QJsonObject changed = root;
        changed.insert(pair.first, pair.second);
        CHECK(rejects(changed));
    }
    for (const auto &pair : QList<QPair<QString, QJsonValue>>{
         {"kind", "unknown"}, {"integerBits", 0}, {"integerBits", 65}, {"integerBits", 8},
         {"editable", false}, {"editable", "true"}, {"size", "bad"}, {"size", -1},
         {"offsetBits", -1}, {"offsetBits", 1}, {"absoluteOffsetBits", 1}, {"size", 100},
         {"name", "address"}, {"name", "a.b"}, {"isArray", true}, {"fields", QJsonValue()},
         {"value", 123}})
        CHECK(rejects(changeField(root, 0, pair.first, pair.second)));

    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    const QString fixture = temporary.path() + "/Fixture.h";
    CHECK(write(fixture,
        "typedef unsigned char Bytes[3]; struct Child { bool enabled:1; unsigned bits:3; };"
        "struct Fixture { Child child; Bytes data; const Child locked; int *pointer; };"));
    const ParseResult fixtureResult = parseHeader(fixture, "Fixture");
    CHECK(fixtureResult.success);
    const QJsonDocument fixtureJson = structureDefinitionJson(fixtureResult.structure);
    const ParseResult fixtureImport = parseStructureDefinitionJson(fixtureJson.toJson());
    CHECK(fixtureImport.success && structureDefinitionJson(fixtureImport.structure) == fixtureJson);
    CHECK(fixtureImport.structure.fields.at(1).isArray && fixtureImport.structure.fields.at(1).children.isEmpty());
    CHECK(fixtureImport.structure.fields.first().children.first().valueBits == 1);
    QJsonObject badNested = root;
    QJsonArray rootFields = badNested.value("fields").toArray();
    QJsonObject address = rootFields.at(1).toObject();
    QJsonArray children = address.value("fields").toArray();
    QJsonObject child = children.at(1).toObject();
    child.insert("absoluteOffsetBits", 16); // 相对偏移不能直接当作绝对偏移。
    children.replace(1, child);
    address.insert("fields", children);
    rootFields.replace(1, address);
    badNested.insert("fields", rootFields);
    CHECK(rejects(badNested));
    StructInfo huge;
    huge.name = "Huge";
    huge.size = 9007199254740993LL;
    huge.alignment = 1;
    FieldInfo largeArray;
    largeArray.name = "data";
    largeArray.typeName = largeArray.canonicalTypeName = "unsigned char[9007199254740993]";
    largeArray.size = huge.size;
    largeArray.alignment = 1;
    largeArray.offsetBits = largeArray.absoluteOffsetBits = 0;
    largeArray.isArray = true;
    huge.fields.append(largeArray);
    const ParseResult hugeImport = parseStructureDefinitionJson(structureDefinitionJson(huge).toJson());
    CHECK(hugeImport.success && hugeImport.structure.size == huge.size);
    CHECK(hugeImport.structure.fields.first().size == huge.size);
    CHECK(!loadStructureDefinitionJson(temporary.path() + "/missing.json").success);

    const QString jsonPath = temporary.path() + "/Nested.JSON";
    QString error;
    CHECK(saveStructureDefinitionJson(nested.structure, jsonPath, &error));
    const ParseResult loaded = loadStructureDefinitionJson(jsonPath);
    CHECK(loaded.success);
    const QHash<QString, QString> values{{"sequence", "0x12345678"}, {"address.source", "0x1122"},
        {"address.destination", "0x3344"}, {"payloadLength", "0x5566"}};
    const BinaryResult binary = encodeStructure(loaded.structure, values);
    CHECK(binary.success && binary.bytes.toHex() == "78563412221144336655");

    MainWindow window(jsonPath);
    auto *path = window.findChild<QLineEdit *>("headerFilePath");
    auto *name = window.findChild<QComboBox *>("structSelector");
    auto *tree = window.findChild<QTreeWidget *>("fieldsTree");
    auto *reload = window.findChild<QPushButton *>("reloadDefinitionButton");
    auto *source = window.findChild<QLabel *>("definitionSourceInfo");
    CHECK(!name->isEnabled() && name->currentText() == "NestedPacketHeader");
    CHECK(source->text().contains("JSON") && tree->topLevelItemCount() == 1);
    auto *sequence = qobject_cast<QLineEdit *>(tree->itemWidget(tree->topLevelItem(0)->child(0), 1));
    sequence->setText("0x12345678");
    reload->click();
    CHECK(window.fieldValues().value("sequence") == "0x12345678");
    CHECK(!name->isEnabled());
    window.findChild<QPushButton *>("viewBinaryButton")->click();
    auto *preview = window.findChild<QDialog *>("binaryPreviewDialog");
    CHECK(preview && preview->findChild<QPlainTextEdit *>("binaryHexView")->toPlainText().startsWith("00000000  78 56 34 12"));
    preview->close();
    path->setText(testHeaders + "/MsgHeader.h");
    CHECK(QMetaObject::invokeMethod(path, "returnPressed"));
    CHECK(!name->isEnabled() && name->currentText() == "MsgHeader" && name->count() == 1);
    CHECK(source->text().contains(QStringLiteral("头文件")) && tree->topLevelItem(0)->childCount() == 21);
    path->setText(testHeaders + "/NestedPacketHeader.h");
    reload->click();
    name->setCurrentText("NestedPacketHeader");
    CHECK(tree->topLevelItemCount() == 1);
    QTimer::singleShot(0, [jsonPath] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *dialog = qobject_cast<QFileDialog *>(widget);
            if (dialog && dialog->isVisible()) {
                dialog->selectFile(jsonPath);
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                return;
            }
        }
        qFatal("Open dialog not found");
    });
    window.findChild<QPushButton *>("openDefinitionButton")->click();
    CHECK(!name->isEnabled() && name->currentText() == "NestedPacketHeader");
    window.show();
    app.processEvents();
    if (app.arguments().size() > 1)
        CHECK(window.grab().save(app.arguments().at(1)));
    CHECK(write(jsonPath, "{broken"));
    reload->click();
    CHECK(tree->topLevelItemCount() == 0 && window.fieldValues().isEmpty() && name->count() == 0);
    CHECK(!window.findChild<QPushButton *>("exportDefinitionButton")->isEnabled());
    CHECK(!window.findChild<QPushButton *>("viewBinaryButton")->isEnabled());
    CHECK(!window.findChild<QPlainTextEdit *>()->toPlainText().isEmpty());
    path->setText(testHeaders + "/NestedPacketHeader.h");
    reload->click();
    CHECK(name->isEnabled() && name->currentText() == "NestedPacketHeader" && tree->topLevelItemCount() == 1);
    QTextStream(stdout) << "PASS: JSON roundtrip/import validation, aliases/arrays/bitfields, binary output,"
                          " unified open dialog, source display, readonly name, reload, failure and header-mode restoration\n";
    return 0;
}
