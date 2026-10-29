#include "fieldeditor.h"
#include "theme.h"
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QTextStream>
#include <QTreeWidget>

#define CHECK(c) do { if (!(c)) { QTextStream(stderr) << "FAIL " << __LINE__ << ": " << #c << '\n'; return 1; } } while (false)

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    applyApplicationTheme(app);
    // 独立使用编辑区，无需创建主窗口、弹窗管理器或文件选择界面。
    FieldEditor editor;
    bool lastValid = true;
    QObject::connect(&editor, &FieldEditor::validityChanged, [&](bool valid) { lastValid = valid; });
    const ParseResult parsed = parseHeader(QStringLiteral(MSG_HEADER_PATH));
    CHECK(parsed.success);
    editor.setStructure(parsed.structure);
    auto *tree = editor.findChild<QTreeWidget *>("fieldsTree");
    CHECK(tree && tree->topLevelItemCount() == 1);
    auto *id = editor.findChild<QLineEdit *>("fieldValue:msgId");
    auto *active = editor.findChild<QComboBox *>("fieldValue:boolValue");
    CHECK(id && active);
    id->setText("0x1234");
    id->setText("invalid");
    CHECK(editor.hasInvalidValues() && !lastValid);
    CHECK(editor.fieldValues().value("msgId") == "0x1234");
    editor.applyValues({{"msgId", "42"}, {"boolValue", "true"}});
    CHECK(!editor.hasInvalidValues() && lastValid && id->text() == "42");
    CHECK(active->currentText() == "true" && editor.fieldValues().value("boolValue") == "true");
    // 直接传入内部只读引用，重建也应正确保留原值。
    editor.setStructure(parsed.structure, editor.fieldValues());
    CHECK(editor.fieldValues().value("msgId") == "42");
    CHECK(editor.fieldValues().value("boolValue") == "true");
    editor.collapseAll();
    CHECK(!tree->topLevelItem(0)->isExpanded());
    editor.expandAll();
    CHECK(tree->topLevelItem(0)->isExpanded());
    editor.clear();
    CHECK(editor.fieldValues().isEmpty() && !editor.hasInvalidValues() && lastValid);
    CHECK(tree->topLevelItemCount() == 0);
    editor.setStructure(parsed.structure);
    CHECK(editor.fieldValues().value("msgId") == "0" && editor.fieldValues().value("boolValue") == "false");
    QTextStream(stdout) << "PASS: standalone editor, validation notification, last valid values, apply, alias-safe reload, expansion and reset\n";
    return 0;
}
