#include "mainwindow.h"
#include "hexdialogs.h"
#include "fieldeditor.h"
#include "definitionpanel.h"
#include "headerparser.h"
#include "binaryencoder.h"
#include "binarydecoder.h"
#include "definitionjson.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

// 界面层的数据流：加载文件 → 得到候选结构体 → 选择一项 → 显示字段树 → 编辑/预览/导出。
// MainWindow 协调加载与操作；字段控件和校验由 FieldEditor 负责。

// 编辑区统一维护输入错误，主窗口只决定相关操作按钮是否可用。
void MainWindow::updateValueStatus()
{
    m_binaryButton->setEnabled(m_session.hasStructure() && !m_fieldEditor->hasInvalidValues());
}

const QHash<QString, QString> &MainWindow::fieldValues() const
{
    return m_fieldEditor->fieldValues();
}

bool MainWindow::hasInvalidFieldValues() const
{
    return m_fieldEditor->hasInvalidValues();
}

// 先调用编码模块得到字节，再把字节排版到弹窗；此函数不直接计算字段布局。
void MainWindow::showBinary()
{
    if (!m_session.hasStructure() || hasInvalidFieldValues())
        return;
    const BinaryResult result = encodeStructure(m_session.structure(), fieldValues());
    if (!result.success) {
        m_status->setText(QStringLiteral("无法生成二进制，请查看下方原因。"));
        m_diagnostics->setPlainText(result.diagnostics.join(QLatin1Char('\n')));
        m_diagnostics->show();
        return;
    }
    m_diagnostics->setPlainText(m_session.diagnostics().join(QLatin1Char('\n')));
    m_diagnostics->setVisible(!m_session.diagnostics().isEmpty());
    m_status->setText(QStringLiteral("已生成 %1 字节的二进制预览。修改值后请重新点击“查看二进制”。")
                          .arg(result.bytes.size()));

    m_hexDialogs->showPreview(m_session.structure(), result.bytes, m_session.loadedPath());
}

// 导出只传结构定义 m_session.structure()，字段输入值 fieldValues() 不参与保存。
void MainWindow::exportDefinitionJson()
{
    if (!m_session.hasStructure())
        return;
    const QDir sourceDir = QFileInfo(m_valuesSource.section(QLatin1Char('\n'), 0, 0)).dir();
    const QString suggested = sourceDir.filePath(m_session.structure().name + QStringLiteral(".json"));
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出结构定义 JSON"), suggested,
                                               QStringLiteral("JSON 文件 (*.json);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".json");
    QString error;
    if (!saveStructureDefinitionJson(m_session.structure(), path, &error)) {
        m_status->setText(QStringLiteral("导出失败：%1").arg(error));
        return;
    }
    m_status->setText(QStringLiteral("已导出结构定义（不含值）：%1").arg(QDir::toNativeSeparators(path)));
}

// 解码分成两步：先预览解析结果，再由用户点击“填入可编辑字段”更新主窗口。
void MainWindow::showHexDecoder()
{
    if (m_session.hasStructure())
        m_hexDialogs->showDecoder(m_session.structure(), [this](const QHash<QString, QString> &values) {
            m_fieldEditor->applyValues(values);
        });
}

bool MainWindow::importHexText(const QString &text, QString &error)
{
    error.clear();
    if (!m_session.hasStructure()) {
        error = QStringLiteral("请先加载结构体定义。");
        return false;
    }
    const BinaryResult hex = parseHexBytes(text);
    if (!hex.success) {
        error = hex.diagnostics.join(QLatin1Char('\n'));
        return false;
    }
    const DecodeResult result = decodeStructure(m_session.structure(), hex.bytes);
    if (!result.success) {
        error = result.diagnostics.join(QLatin1Char('\n'));
        return false;
    }
    // 例如 NaN 可供解析预览，但不能导入编辑器；先检查全部字段，避免只导入一部分。
    QStringList missing;
    for (auto it = fieldValues().cbegin(); it != fieldValues().cend(); ++it) {
        if (!result.editableValues.contains(it.key()))
            missing.append(it.key());
    }
    if (!missing.isEmpty()) {
        missing.sort();
        error = QStringLiteral("%1 的值无法用于编辑（如 NaN 或无穷大）。当前字段值未修改，可使用“解析十六进制”查看详情。")
                .arg(missing.join(QStringLiteral("、")));
        return false;
    }
    if (result.editableValues.isEmpty()) {
        error = QStringLiteral("当前结构体没有可导入的可编辑字段，可使用“解析十六进制”查看原始字节。");
        return false;
    }
    m_fieldEditor->applyValues(result.editableValues);
    m_hexDialogs->rememberInput(text);
    m_status->setText(QStringLiteral("已导入 %1 字节，更新 %2 个字段。可直接修改字段值，再点击“查看二进制”生成新数据。只读字段和填充字节未保存。")
                      .arg(hex.bytes.size()).arg(result.editableValues.size()));
    return true;
}

void MainWindow::importHexFile()
{
    if (!m_session.hasStructure())
        return;
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("从文件导入16进制数据"),
        QFileInfo(m_session.loadedPath()).absolutePath(),
        QStringLiteral("十六进制文本 (*.hex *.txt);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_status->setText(QStringLiteral("导入失败：无法读取 %1：%2")
                          .arg(QDir::toNativeSeparators(path), file.errorString()));
        return;
    }
    // 与文本解析器的限制一致，最多读取上限加一个字节，避免一次加载任意大的文件。
    const int maximumTextBytes = 8 * 1024 * 1024;
    QByteArray contents = file.read(maximumTextBytes + 1);
    if (file.error() != QFileDevice::NoError) {
        m_status->setText(QStringLiteral("导入失败：读取文件出错：%1").arg(file.errorString()));
        return;
    }
    if (contents.size() > maximumTextBytes) {
        m_status->setText(QStringLiteral("导入失败：十六进制文本文件超过 8 MiB 上限。当前字段值未修改。"));
        return;
    }
    // 接受本程序导出的纯文本及常见文本编辑器保存的 UTF-8 BOM。
    if (contents.startsWith(QByteArray::fromHex("efbbbf")))
        contents.remove(0, 3);
    QString error;
    if (!importHexText(QString::fromUtf8(contents), error)) {
        m_status->setText(QStringLiteral("导入失败：%1").arg(error));
        return;
    }
    m_status->setText(QStringLiteral("来源文件：%1\n%2")
                      .arg(QDir::toNativeSeparators(path), m_status->text()));
}

void MainWindow::showHexImporter()
{
    if (m_session.hasStructure())
        m_hexDialogs->showImporter(m_session.structure(), [this](const QString &text, QString &error) {
            return importHexText(text, error);
        });
}

// 构造函数分三步：创建控件、用布局排列控件、连接信号；最后首次加载文件。
MainWindow::MainWindow(const QString &filePath, QWidget *parent)
    : FramelessWindow(parent)
{
    m_hexDialogs = new HexDialogs(this);
    setWindowTitle(QStringLiteral("StructHex · 结构体数据编辑器"));
    setObjectName(QStringLiteral("mainWindow"));
    setAttribute(Qt::WA_StyledBackground);
    resize(1440, 900);
    setMinimumSize(1040, 640);

    // 外层不留边距，标题栏贴齐窗口；内容区独立保留控件间距。
    auto *windowLayout = new QVBoxLayout(this);
    windowLayout->setContentsMargins(0, 0, 0, 0);
    windowLayout->setSpacing(0);
    windowLayout->addWidget(createTitleBar());

    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);
    windowLayout->addLayout(layout, 1);

    m_definitionPanel = new DefinitionPanel(filePath, this);
    layout->addWidget(m_definitionPanel);
    connect(m_definitionPanel, &DefinitionPanel::loadRequested, this, &MainWindow::reloadDefinition);
    connect(m_definitionPanel, &DefinitionPanel::structureSelected, this, [this](int index) {
        m_session.selectStructure(index);
        showSelectedStructure();
    });
    layout->addWidget(createActionPanel());

    m_structureName = new QLabel(this);
    m_structureName->setObjectName(QStringLiteral("structureName"));
    QFont titleFont = font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_structureName->setFont(titleFont);
    m_structureName->setTextFormat(Qt::PlainText);
    m_structureName->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *structureRow = new QHBoxLayout;
    structureRow->addWidget(m_structureName);
    structureRow->addStretch();
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("structureSummary"));
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    structureRow->addWidget(m_summary);
    layout->addLayout(structureRow);
    m_fieldEditor = new FieldEditor(this);
    layout->addWidget(m_fieldEditor, 1);
    connect(m_fieldEditor, &FieldEditor::validityChanged, this, [this](bool) { updateValueStatus(); });

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("operationStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    m_diagnostics = new QPlainTextEdit(this);
    m_diagnostics->setReadOnly(true);
    m_diagnostics->setMaximumHeight(130);
    layout->addWidget(m_diagnostics);

    connect(m_definitionPanel, &DefinitionPanel::expandRequested, m_fieldEditor, &FieldEditor::expandAll);
    connect(m_definitionPanel, &DefinitionPanel::collapseRequested, m_fieldEditor, &FieldEditor::collapseAll);
    reloadDefinition(filePath);
}

// 读取文件是两个入口的共同起点：头文件走 libclang，JSON 直接恢复相同的数据结构。
void MainWindow::reloadDefinition(const QString &filePath)
{
    m_session.load(filePath);
    m_definitionPanel->setDefinitions(m_session.structureNames(), m_session.selectedIndex(), m_session.isJson());
    showSelectedStructure();
}

// 显示会话的当前结果；候选切换由 DefinitionPanel 信号的连接负责。
void MainWindow::showSelectedStructure()
{
    m_hexDialogs->closeEditors();
    const bool loaded = m_session.hasStructure();
    const QStringList diagnostics = m_session.diagnostics();
    m_definitionPanel->setSourceState(m_session.isJson(), loaded);

    // 清空旧结果，避免解析失败后还显示上一个文件的字段。
    const QString source = m_session.sourceKey();
    // 只有同一文件、同一结构体才复用旧输入；具体字段是否仍合法由 FieldEditor 检查。
    const QHash<QString, QString> previousValues = source == m_valuesSource
        ? fieldValues() : QHash<QString, QString>();
    m_fieldEditor->clear();
    m_valuesSource.clear();
    m_binaryButton->setEnabled(false);
    m_decodeButton->setEnabled(false);
    m_importHexButton->setEnabled(false);
    m_importHexFileButton->setEnabled(false);
    m_exportDefinitionButton->setEnabled(false);
    m_diagnostics->setPlainText(diagnostics.join(QLatin1Char('\n')));
    m_diagnostics->setVisible(!diagnostics.isEmpty());
    if (!loaded) {
        m_structureName->setText(QStringLiteral("加载失败"));
        m_summary->setText(QStringLiteral("尚无可显示的结构体信息"));
        m_status->setText(QStringLiteral("请检查结构定义文件和下方诊断信息，修正后点击“重新加载”；也可选择其他结构体。"));
        return;
    }

    const StructInfo &structure = m_session.structure();
    m_decodeButton->setEnabled(true);
    m_importHexButton->setEnabled(true);
    m_importHexFileButton->setEnabled(true);
    m_exportDefinitionButton->setEnabled(true);
    m_valuesSource = source;
    // 结构体名来自 AST 查询结果，不是在界面中写死的显示文本。
    m_structureName->setText(QStringLiteral("结构体：%1").arg(structure.name));
    m_summary->setText(QStringLiteral("%1 个直接字段    |    大小：%2 字节    |    结构体对齐：%3 字节")
                           .arg(structure.fields.size()).arg(structure.size).arg(structure.alignment));
    m_fieldEditor->setStructure(structure, previousValues);
    updateValueStatus();
    m_status->setText(diagnostics.isEmpty()
        ? QStringLiteral("加载成功。点击箭头展开或折叠嵌套字段；修改并保存结构定义文件后，点击“重新加载”更新。")
        : QStringLiteral("加载成功，下方有诊断提示。"));
}

QWidget *MainWindow::createActionPanel()
{
    auto *panel = new QWidget(this);
    auto *actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(0, 0, 0, 0);
    m_binaryButton = new QPushButton(QStringLiteral("查看二进制"), this);
    m_binaryButton->setObjectName(QStringLiteral("viewBinaryButton"));
    m_binaryButton->setEnabled(false);
    m_binaryButton->setToolTip(QStringLiteral("按当前结构体布局生成十六进制快照。请先修正所有无效输入。"));
    actionRow->addWidget(m_binaryButton);
    m_importHexButton = new QPushButton(QStringLiteral("导入16进制数据"), this);
    m_importHexButton->setObjectName(QStringLiteral("importHexDataButton"));
    m_importHexButton->setEnabled(false);
    m_importHexButton->setToolTip(QStringLiteral("粘贴已保存的小端十六进制数据，导入当前结构体后继续编辑字段值。"));
    actionRow->addWidget(m_importHexButton);
    m_importHexFileButton = new QPushButton(QStringLiteral("从文件导入16进制数据"), this);
    m_importHexFileButton->setObjectName(QStringLiteral("importHexFileButton"));
    m_importHexFileButton->setEnabled(false);
    m_importHexFileButton->setToolTip(QStringLiteral("选择 .hex 或 .txt 十六进制文本文件，导入当前结构体后继续编辑。"));
    actionRow->addWidget(m_importHexFileButton);
    m_decodeButton = new QPushButton(QStringLiteral("解析十六进制…"), this);
    m_decodeButton->setObjectName(QStringLiteral("decodeHexButton"));
    m_decodeButton->setEnabled(false);
    actionRow->addWidget(m_decodeButton);
    m_exportDefinitionButton = new QPushButton(QStringLiteral("导出结构定义 JSON"), this);
    m_exportDefinitionButton->setObjectName(QStringLiteral("exportDefinitionButton"));
    m_exportDefinitionButton->setToolTip(QStringLiteral("导出当前解析的类型和布局，不包含输入值；无需填写字段值。"));
    m_exportDefinitionButton->setEnabled(false);
    actionRow->addWidget(m_exportDefinitionButton);
    actionRow->addStretch();
    panel->setLayout(actionRow);

    connect(m_binaryButton, &QPushButton::clicked, this, [this] { showBinary(); });
    connect(m_decodeButton, &QPushButton::clicked, this, [this] { showHexDecoder(); });
    connect(m_importHexButton, &QPushButton::clicked, this, [this] { showHexImporter(); });
    connect(m_importHexFileButton, &QPushButton::clicked, this, [this] { importHexFile(); });
    connect(m_exportDefinitionButton, &QPushButton::clicked, this, [this] { exportDefinitionJson(); });
    return panel;
}
