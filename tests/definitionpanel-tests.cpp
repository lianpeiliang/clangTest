#include "UI/definitionpanel.h"
#include "UI/theme.h"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextStream>

#define CHECK(c) do { if (!(c)) { QTextStream(stderr) << "FAIL " << __LINE__ << ": " << #c << '\n'; return 1; } } while (false)

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    applyApplicationTheme(app);
    DefinitionPanel panel(QStringLiteral("initial.h"));
    int loads = 0, selections = 0, expands = 0, collapses = 0;
    int selected = -1;
    QString requestedPath;
    QObject::connect(&panel, &DefinitionPanel::loadRequested, [&](const QString &path) { ++loads; requestedPath = path; });
    QObject::connect(&panel, &DefinitionPanel::structureSelected, [&](int index) { ++selections; selected = index; });
    QObject::connect(&panel, &DefinitionPanel::expandRequested, [&] { ++expands; });
    QObject::connect(&panel, &DefinitionPanel::collapseRequested, [&] { ++collapses; });
    auto *path = panel.findChild<QLineEdit *>("headerFilePath");
    auto *selector = panel.findChild<QComboBox *>("structSelector");
    auto *source = panel.findChild<QLabel *>("definitionSourceInfo");
    CHECK(path && selector && source && path->text() == "initial.h");
    path->setText("not-loaded.h");
    CHECK(loads == 0); // 编辑路径只改变界面，确认前不发起加载。
    panel.findChild<QPushButton *>("reloadDefinitionButton")->click();
    CHECK(loads == 1 && requestedPath == "not-loaded.h");
    CHECK(QMetaObject::invokeMethod(path, "returnPressed"));
    CHECK(loads == 2);
    panel.setDefinitions({"First", "Second"}, 0, false);
    CHECK(selections == 0 && selector->isEnabled());
    selector->setCurrentIndex(1);
    CHECK(selections == 1 && selected == 1);
    panel.setSourceState(false, true);
    CHECK(source->text().contains(QStringLiteral("2 个结构体")));
    panel.setDefinitions({"Saved"}, 0, true);
    CHECK(selections == 1 && !selector->isEnabled() && selector->currentText() == "Saved");
    panel.setSourceState(true, true);
    CHECK(source->text().contains("JSON"));
    panel.setDefinitions({}, -1, false);
    panel.setSourceState(false, false);
    CHECK(selections == 1 && selector->count() == 0 && !selector->isEnabled());
    CHECK(source->text().contains(QStringLiteral("加载失败")));
    panel.findChild<QPushButton *>("expandFieldsButton")->click();
    panel.findChild<QPushButton *>("collapseFieldsButton")->click();
    CHECK(expands == 1 && collapses == 1);
    CHECK(loads == 2 && selections == 1);
    QTextStream(stdout) << "PASS: standalone definition panel, explicit load intents, silent refresh, selection, JSON/failure presentation and expand/collapse signals\n";
    return 0;
}
