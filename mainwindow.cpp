#include "mainwindow.h"
#include "headerparser.h"
#include "fieldvalue.h"
#include "binaryencoder.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QString byteOffset(qint64 bits)
{
    // 位域保留不足一个字节的 bit 偏移。
    return bits % 8 == 0 ? QString::number(bits / 8)
                        : QStringLiteral("%1 + %2 bit").arg(bits / 8).arg(bits % 8);
}

} // namespace

void MainWindow::appendFields(QTreeWidgetItem *parent, const QVector<FieldInfo> &fields,
                              const QString &parentPath,
                              const QHash<QString, QString> &previousValues)
{
    for (const FieldInfo &field : fields) {
        const QString fieldPath = parentPath.isEmpty() ? field.name
                                                       : parentPath + QLatin1Char('.') + field.name;
        auto *item = new QTreeWidgetItem(parent, QStringList{
            field.name, QString(), field.typeName, field.canonicalTypeName,
            QString::number(field.size), QString::number(field.alignment),
            byteOffset(field.offsetBits), byteOffset(field.absoluteOffsetBits),
            QString::number(field.offsetBits), QString::number(field.absoluteOffsetBits)
        });
        item->setToolTip(0, fieldPath);
        for (int column = 4; column < item->columnCount(); ++column)
            item->setTextAlignment(column, Qt::AlignCenter);
        if (!isEditableField(field)) {
            item->setText(1, field.valueKind == FieldValueKind::Record
                                ? QStringLiteral("分组") : QStringLiteral("只读"));
            item->setToolTip(1, field.readOnlyReason);
        } else {
            const FieldValueValidation previous = validateFieldValue(field, previousValues.value(fieldPath));
            // 保留合法的输入形式（例如 0xFF），编码时再由校验器转换成数值。
            const QString value = previous.valid ? previousValues.value(fieldPath).trimmed() : defaultFieldValue(field);
            m_fieldValues.insert(fieldPath, value);
            const QString hint = fieldValueHint(field);
            if (field.valueKind == FieldValueKind::Boolean) {
                auto *editor = new QComboBox(m_fields);
                editor->setObjectName(QStringLiteral("fieldValue:") + fieldPath);
                editor->addItems({QStringLiteral("false"), QStringLiteral("true")});
                editor->setCurrentText(value);
                editor->setToolTip(fieldPath + QLatin1Char('\n') + hint);
                item->setSizeHint(1, QSize(190, editor->sizeHint().height() + 4));
                m_fields->setItemWidget(item, 1, editor);
                connect(editor, &QComboBox::currentTextChanged, this,
                        [this, fieldPath](const QString &text) {
                    m_fieldValues.insert(fieldPath, text);
                });
            } else {
                auto *editor = new QLineEdit(value, m_fields);
                editor->setObjectName(QStringLiteral("fieldValue:") + fieldPath);
                editor->setToolTip(fieldPath + QLatin1Char('\n') + hint);
                item->setSizeHint(1, QSize(190, editor->sizeHint().height() + 4));
                m_fields->setItemWidget(item, 1, editor);
                connect(editor, &QLineEdit::textChanged, this,
                        [this, editor, field, fieldPath, hint](const QString &text) {
                    const FieldValueValidation validation = validateFieldValue(field, text);
                    if (validation.valid) {
                        m_fieldValues.insert(fieldPath, text.trimmed());
                        m_valueErrors.remove(fieldPath);
                    } else {
                        m_valueErrors.insert(fieldPath, validation.error);
                    }
                    editor->setStyleSheet(validation.valid ? QString() : QStringLiteral(
                        "QLineEdit { border: 1px solid #c62828; background-color: #fff0f0; color: #9c1c1c; }"));
                    editor->setToolTip(fieldPath + QLatin1Char('\n')
                                      + (validation.valid ? hint : validation.error));
                    updateValueStatus();
                });
            }
        }
        appendFields(item, field.children, fieldPath, previousValues);
    }
}

void MainWindow::updateValueStatus()
{
    m_binaryButton->setEnabled(m_hasStructure && m_valueErrors.isEmpty());
    if (m_valueErrors.isEmpty()) {
        m_valueStatus->setText(QStringLiteral("值保存在内存中。整数支持十进制或 0x 前缀十六进制，布尔值使用下拉选择；字符类型按数值编码填写。"));
    } else {
        QStringList paths = m_valueErrors.keys();
        paths.sort();
        const QString path = paths.first();
        m_valueStatus->setText(QStringLiteral("%1 个字段输入无效：%2：%3 上一次有效值已保留。")
            .arg(paths.size()).arg(path, m_valueErrors.value(path)));
    }
}

void MainWindow::showBinary()
{
    if (!m_hasStructure || hasInvalidFieldValues())
        return;
    const BinaryResult result = encodeStructure(m_parsedStructure, m_fieldValues);
    if (!result.success) {
        m_status->setText(QStringLiteral("无法生成二进制，请查看下方原因。"));
        m_diagnostics->setPlainText(result.diagnostics.join(QLatin1Char('\n')));
        m_diagnostics->show();
        return;
    }
    m_diagnostics->setPlainText(m_parseDiagnostics.join(QLatin1Char('\n')));
    m_diagnostics->setVisible(!m_parseDiagnostics.isEmpty());
    m_status->setText(QStringLiteral("已生成 %1 字节的二进制预览。修改值后请重新点击“查看二进制”。")
                          .arg(result.bytes.size()));

    auto *dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("binaryPreviewDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("十六进制预览：%1").arg(m_parsedStructure.name));
    dialog->resize(860, 520);
    auto *layout = new QVBoxLayout(dialog);
    auto *summary = new QLabel(QStringLiteral(
        "%1    |    %2 字节    |    Windows x64 / MinGW，小端序\n"
        "当前值快照；每行 16 字节，左侧为十六进制偏移，填充字节和未命名位域补 0。")
        .arg(m_parsedStructure.name).arg(result.bytes.size()), dialog);
    summary->setTextFormat(Qt::PlainText);
    summary->setWordWrap(true);
    layout->addWidget(summary);
    auto *hex = new QPlainTextEdit(dialog);
    hex->setObjectName(QStringLiteral("binaryHexView"));
    hex->setReadOnly(true);
    hex->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont hexFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    hexFont.setPointSize(11);
    hex->setFont(hexFont);
    hex->setPlainText(formatHexDump(result.bytes));
    layout->addWidget(hex, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    auto *copy = buttons->addButton(QStringLiteral("复制十六进制"), QDialogButtonBox::ActionRole);
    copy->setObjectName(QStringLiteral("copyHexButton"));
    const QByteArray bytes = result.bytes;
    connect(copy, &QPushButton::clicked, dialog, [bytes] {
        QApplication::clipboard()->setText(QString::fromLatin1(bytes.toHex(' ').toUpper()));
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);
    dialog->show();
}

MainWindow::MainWindow(const QString &filePath, QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("C++ 结构体查看器"));
    resize(1360, 820);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *sourceRow = new QHBoxLayout;
    sourceRow->addWidget(new QLabel(QStringLiteral("头文件："), this));
    m_filePath = new QLineEdit(QDir::toNativeSeparators(filePath), this);
    m_filePath->setObjectName(QStringLiteral("headerFilePath"));
    m_filePath->setPlaceholderText(QStringLiteral("输入头文件路径，按回车解析"));
    sourceRow->addWidget(m_filePath, 1);
    auto *openButton = new QPushButton(QStringLiteral("打开头文件…"), this);
    auto *reloadButton = new QPushButton(QStringLiteral("重新解析"), this);
    sourceRow->addWidget(openButton);
    sourceRow->addWidget(reloadButton);
    layout->addLayout(sourceRow);

    auto *nameRow = new QHBoxLayout;
    auto *nameLabel = new QLabel(QStringLiteral("结构体名称："), this);
    nameRow->addWidget(nameLabel);
    m_structName = new QLineEdit(QStringLiteral("MsgHeader"), this);
    m_structName->setObjectName(QStringLiteral("structNameInput"));
    m_structName->setPlaceholderText(QStringLiteral("输入结构体名称，按回车解析"));
    nameLabel->setBuddy(m_structName);
    nameRow->addWidget(m_structName, 1);
    auto *expandButton = new QPushButton(QStringLiteral("全部展开"), this);
    auto *collapseButton = new QPushButton(QStringLiteral("全部折叠"), this);
    nameRow->addWidget(expandButton);
    nameRow->addWidget(collapseButton);
    m_binaryButton = new QPushButton(QStringLiteral("查看二进制"), this);
    m_binaryButton->setObjectName(QStringLiteral("viewBinaryButton"));
    m_binaryButton->setEnabled(false);
    m_binaryButton->setToolTip(QStringLiteral("按当前结构体布局生成十六进制快照。请先修正所有无效输入。"));
    nameRow->addWidget(m_binaryButton);
    layout->addLayout(nameRow);

    m_structureName = new QLabel(this);
    m_structureName->setObjectName(QStringLiteral("structureName"));
    QFont titleFont = font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_structureName->setFont(titleFont);
    m_structureName->setTextFormat(Qt::PlainText);
    m_structureName->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_structureName);
    m_summary = new QLabel(this);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_summary);
    m_valueStatus = new QLabel(this);
    m_valueStatus->setObjectName(QStringLiteral("fieldValueStatus"));
    m_valueStatus->setTextFormat(Qt::PlainText);
    m_valueStatus->setWordWrap(true);
    layout->addWidget(m_valueStatus);

    // 布局列只读；“值”列用独立编辑控件，校验后的值与布局元数据分开保存。
    m_fields = new QTreeWidget(this);
    m_fields->setObjectName(QStringLiteral("fieldsTree"));
    m_fields->setColumnCount(10);
    m_fields->setHeaderLabels(QStringList{
        QStringLiteral("字段名称"), QStringLiteral("值"),
        QStringLiteral("字段类型"), QStringLiteral("展开后的类型"),
        QStringLiteral("大小（字节）"), QStringLiteral("自然对齐（字节）"),
        QStringLiteral("相对偏移（字节）"), QStringLiteral("绝对偏移（字节）"),
        QStringLiteral("相对偏移（bit）"), QStringLiteral("绝对偏移（bit）")
    });
    m_fields->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fields->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fields->setAlternatingRowColors(true);
    m_fields->setWordWrap(false);
    m_fields->setAnimated(true);
    m_fields->header()->setStretchLastSection(false);
    m_fields->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_fields->header()->setMinimumSectionSize(80);
    m_fields->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_fields->setColumnWidth(1, 190);
    m_fields->headerItem()->setToolTip(1, QStringLiteral("基础类型可赋值，嵌套结构体作为分组；指针、引用和数组暂时只读。"));
    m_fields->headerItem()->setToolTip(3,
        QStringLiteral("展开 typedef 等类型别名，例如 uint16_t 对应 unsigned short。"));
    m_fields->headerItem()->setToolTip(5,
        QStringLiteral("类型本身的自然对齐，可能大于结构体 pack 指定的对齐值。"));
    for (int column : {6, 8})
        m_fields->headerItem()->setToolTip(column,
            QStringLiteral("字段相对于直接所属结构体起点的偏移。"));
    for (int column : {7, 9})
        m_fields->headerItem()->setToolTip(column,
            QStringLiteral("字段相对于最外层结构体起点的累计偏移。"));
    layout->addWidget(m_fields, 1);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    m_diagnostics = new QPlainTextEdit(this);
    m_diagnostics->setReadOnly(true);
    m_diagnostics->setMaximumHeight(130);
    layout->addWidget(m_diagnostics);

    // 按钮只触发“重新读取文件并刷新界面”，不参与 libclang 的 AST 遍历。
    connect(reloadButton, &QPushButton::clicked, this, [this] { reloadHeader(); });
    connect(m_filePath, &QLineEdit::returnPressed, this, [this] { reloadHeader(); });
    connect(m_structName, &QLineEdit::returnPressed, this, [this] { reloadHeader(); });
    connect(expandButton, &QPushButton::clicked, m_fields, &QTreeWidget::expandAll);
    connect(collapseButton, &QPushButton::clicked, m_fields, &QTreeWidget::collapseAll);
    connect(m_binaryButton, &QPushButton::clicked, this, [this] { showBinary(); });
    connect(openButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择包含结构体定义的头文件"), m_filePath->text(),
            QStringLiteral("C++ 头文件 (*.h *.hpp *.hh *.hxx);;所有文件 (*)"));
        if (!path.isEmpty()) {
            m_filePath->setText(QDir::toNativeSeparators(path));
            reloadHeader();
        }
    });
    reloadHeader();
}

void MainWindow::reloadHeader()
{
    const QString path = QDir::fromNativeSeparators(m_filePath->text().trimmed());
    const QString structName = m_structName->text().trimmed();
    ParseResult result;
    if (structName.isEmpty())
        result.diagnostics.append(QStringLiteral("请输入要解析的结构体名称，例如 MsgHeader。"));
    else
        result = parseHeader(path, structName);

    // 清空旧结果，避免解析失败后还显示上一个文件的字段。
    m_fields->clear();
    const QString source = QFileInfo(path).absoluteFilePath() + QLatin1Char('\n') + structName;
    const QHash<QString, QString> previousValues = source == m_valuesSource
        ? m_fieldValues : QHash<QString, QString>();
    m_fieldValues.clear();
    m_valueErrors.clear();
    m_valuesSource.clear();
    m_valueStatus->clear();
    m_hasStructure = false;
    m_parsedStructure = StructInfo();
    m_parseDiagnostics = result.diagnostics;
    m_binaryButton->setEnabled(false);
    m_diagnostics->setPlainText(result.diagnostics.join(QLatin1Char('\n')));
    m_diagnostics->setVisible(!result.diagnostics.isEmpty());
    if (!result.success) {
        m_structureName->setText(QStringLiteral("解析失败"));
        m_summary->setText(QStringLiteral("尚无可显示的结构体信息"));
        m_status->setText(QStringLiteral("请检查文件路径、结构体名称和下方诊断信息，修正后点击“重新解析”。"));
        return;
    }

    const StructInfo &structure = result.structure;
    m_parsedStructure = structure;
    m_hasStructure = true;
    m_valuesSource = source;
    // 结构体名来自 AST 查询结果，不是在界面中写死的显示文本。
    m_structureName->setText(QStringLiteral("结构体：%1").arg(structure.name));
    m_summary->setText(QStringLiteral("%1 个直接字段    |    大小：%2 字节    |    结构体对齐：%3 字节")
                           .arg(structure.fields.size()).arg(structure.size).arg(structure.alignment));
    auto *root = new QTreeWidgetItem(m_fields, QStringList{
        structure.name, QStringLiteral("分组"), QStringLiteral("struct"), structure.name,
        QString::number(structure.size), QString::number(structure.alignment),
        QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0")
    });
    for (int column = 4; column < root->columnCount(); ++column)
        root->setTextAlignment(column, Qt::AlignCenter);
    appendFields(root, structure.fields, QString(), previousValues);
    updateValueStatus();
    // 默认展开根节点和第一层嵌套；更深层可手动展开或点击“全部展开”。
    m_fields->expandToDepth(1);
    m_status->setText(result.diagnostics.isEmpty()
        ? QStringLiteral("解析成功。点击箭头展开或折叠嵌套字段；修改并保存头文件后，点击“重新解析”更新。")
        : QStringLiteral("解析成功，下方有诊断提示。"));
}
