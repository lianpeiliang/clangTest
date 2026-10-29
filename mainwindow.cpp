#include "mainwindow.h"
#include "headerparser.h"
#include "fieldvalue.h"
#include "binaryencoder.h"
#include "binarydecoder.h"
#include "definitionjson.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <functional>
#include <memory>

// 界面层的数据流：加载文件 → 得到候选结构体 → 选择一项 → 显示字段树 → 编辑/预览/导出。
// 可以先读构造函数，再读 reloadDefinition()、showSelectedStructure()、appendFields()。
namespace {

QString byteOffset(qint64 bits)
{
    // 位域保留不足一个字节的 bit 偏移。
    return bits % 8 == 0 ? QString::number(bits / 8)
                        : QStringLiteral("%1 + %2 bit").arg(bits / 8).arg(bits % 8);
}

void setInvalid(QWidget *widget, bool invalid)
{
    if (widget->property("invalid").toBool() == invalid)
        return;
    widget->setProperty("invalid", invalid);
    // Qt 动态属性变化后重新应用 QSS，错误颜色集中定义在主题文件中。
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

// 每次处理一层成员；遇到嵌套成员时再次调用自己，形成与结构体对应的树。
void MainWindow::appendFields(QTreeWidgetItem *parent, const QVector<FieldInfo> &fields,
                              const QString &parentPath,
                              const QHash<QString, QString> &previousValues)
{
    for (const FieldInfo &field : fields) {
        // 完整路径用于唯一标识字段，例如 source 与 address.source 是不同的输入项。
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
            // bool 只有两个合法值，用下拉框；其他可编辑基础类型用文本输入框。
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
                // connect 把控件的“信号”连接到处理函数：文字变化时立即校验。
                // 方括号是 lambda 捕获列表，让处理函数稍后还能使用这里的对象和字段信息。
                connect(editor, &QLineEdit::textChanged, this,
                        [this, editor, field, fieldPath, hint](const QString &text) {
                    const FieldValueValidation validation = validateFieldValue(field, text);
                    if (validation.valid) {
                        m_fieldValues.insert(fieldPath, text.trimmed());
                        m_valueErrors.remove(fieldPath);
                    } else {
                        // 无效文字仍显示在输入框中，但不覆盖 m_fieldValues 中的上次合法值。
                        m_valueErrors.insert(fieldPath, validation.error);
                    }
                    setInvalid(editor, !validation.valid);
                    editor->setToolTip(fieldPath + QLatin1Char('\n')
                                      + (validation.valid ? hint : validation.error));
                    updateValueStatus();
                });
            }
        }
        appendFields(item, field.children, fieldPath, previousValues);
    }
}

// 每个输入框只维护自己的错误，这里统一决定按钮状态和顶部的错误提示。
void MainWindow::updateValueStatus()
{
    m_binaryButton->setEnabled(m_hasStructure && m_valueErrors.isEmpty());
    setInvalid(m_valueStatus, !m_valueErrors.isEmpty());
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

// 先调用编码模块得到字节，再把字节排版到弹窗；此函数不直接计算字段布局。
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

    // this 是对话框的父对象；WA_DeleteOnClose 让关闭弹窗时释放它及其子控件。
    auto *dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("binaryPreviewDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("十六进制预览：%1").arg(m_parsedStructure.name));
    dialog->resize(860, 520);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    auto *summary = new QLabel(QStringLiteral(
        "%1    |    %2 字节    |    Windows x64 / MinGW，小端序\n"
        "当前值快照；每行 16 字节，左侧为十六进制偏移，数组、填充字节和未命名位域补 0。")
        .arg(m_parsedStructure.name).arg(result.bytes.size()), dialog);
    summary->setObjectName(QStringLiteral("dialogSummary"));
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
    auto *exportStatus = new QLabel(dialog);
    exportStatus->setObjectName(QStringLiteral("hexExportStatus"));
    exportStatus->setTextFormat(Qt::PlainText);
    exportStatus->setWordWrap(true);
    exportStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(exportStatus);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    auto *exportHex = buttons->addButton(QStringLiteral("导出到文件"), QDialogButtonBox::ActionRole);
    exportHex->setObjectName(QStringLiteral("exportHexFileButton"));
    exportHex->setToolTip(QStringLiteral("保存当前预览的十六进制文本，每行 16 字节，不含偏移地址。"));
    auto *copy = buttons->addButton(QStringLiteral("复制十六进制"), QDialogButtonBox::ActionRole);
    copy->setObjectName(QStringLiteral("copyHexButton"));
    copy->style()->unpolish(copy);
    copy->style()->polish(copy);
    const QByteArray bytes = result.bytes;
    // 按值捕获 bytes，复制按钮使用的是打开弹窗那一刻的快照。
    connect(copy, &QPushButton::clicked, dialog, [bytes, exportStatus] {
        QApplication::clipboard()->setText(QString::fromLatin1(bytes.toHex(' ').toUpper()));
        exportStatus->setText(QStringLiteral("复制成功：已将 %1 字节的十六进制数据复制到剪贴板。")
                              .arg(bytes.size()));
    });
    const QString suggested = QFileInfo(m_loadedPath).dir().filePath(
        QString(m_parsedStructure.name).replace(QStringLiteral("::"), QStringLiteral("_")) + QStringLiteral(".hex"));
    // 保存与复制使用同一份快照；即使主界面已修改，也只导出当前预览中的字节。
    connect(exportHex, &QPushButton::clicked, dialog, [dialog, bytes, suggested, exportStatus] {
        const QString hexFilter = QStringLiteral("十六进制文本 (*.hex)");
        const QString textFilter = QStringLiteral("文本文件 (*.txt)");
        QFileDialog saveDialog(dialog, QStringLiteral("导出十六进制数据"), suggested);
        saveDialog.setAcceptMode(QFileDialog::AcceptSave);
        saveDialog.setNameFilters({hexFilter, textFilter, QStringLiteral("所有文件 (*)")});
        saveDialog.setDefaultSuffix(QStringLiteral("hex"));
        connect(&saveDialog, &QFileDialog::filterSelected, &saveDialog, [&saveDialog, textFilter](const QString &filter) {
            saveDialog.setDefaultSuffix(filter == textFilter ? QStringLiteral("txt") : QStringLiteral("hex"));
        });
        // 由文件选择器补全后缀并确认覆盖，避免手动追加后缀绕过已有文件的确认。
        if (saveDialog.exec() != QDialog::Accepted)
            return;
        const QString path = saveDialog.selectedFiles().first();
        QByteArray text;
        for (int offset = 0; offset < bytes.size(); offset += 16) {
            text.append(bytes.mid(offset, 16).toHex(' ').toUpper());
            text.append('\n');
        }
        // 先写临时文件，完整写入后再替换目标，避免保存失败留下不完整的数据。
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(text) != text.size() || !file.commit()) {
            exportStatus->setText(QStringLiteral("导出失败：%1").arg(file.errorString()));
            return;
        }
        exportStatus->setText(QStringLiteral("已导出 %1 字节的十六进制数据：%2")
                              .arg(bytes.size()).arg(QDir::toNativeSeparators(path)));
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);
    dialog->show();
}

// 导出只传结构定义 m_parsedStructure，字段输入值 m_fieldValues 不参与保存。
void MainWindow::exportDefinitionJson()
{
    if (!m_hasStructure)
        return;
    const QDir sourceDir = QFileInfo(m_valuesSource.section(QLatin1Char('\n'), 0, 0)).dir();
    const QString suggested = sourceDir.filePath(m_parsedStructure.name + QStringLiteral(".json"));
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出结构定义 JSON"), suggested,
                                               QStringLiteral("JSON 文件 (*.json);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".json");
    QString error;
    if (!saveStructureDefinitionJson(m_parsedStructure, path, &error)) {
        m_status->setText(QStringLiteral("导出失败：%1").arg(error));
        return;
    }
    m_status->setText(QStringLiteral("已导出结构定义（不含值）：%1").arg(QDir::toNativeSeparators(path)));
}

// 解码分成两步：先预览解析结果，再由用户点击“填入可编辑字段”更新主窗口。
void MainWindow::showHexDecoder()
{
    if (!m_hasStructure)
        return;
    auto *dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("hexDecodeDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowTitle(QStringLiteral("解析十六进制：%1").arg(m_parsedStructure.name));
    dialog->resize(1100, 720);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    auto *summary = new QLabel(QStringLiteral(
        "%1：需要完整的 %2 字节（含结构体填充）。支持连续十六进制、空格/换行/逗号分隔及 0x 前缀。\n"
        "例如：78 56 34 12 或 0x78, 0x56, 0x34, 0x12。请粘贴字节本身，不含偏移地址。")
        .arg(m_parsedStructure.name).arg(m_parsedStructure.size), dialog);
    summary->setObjectName(QStringLiteral("dialogSummary"));
    summary->setTextFormat(Qt::PlainText);
    summary->setWordWrap(true);
    layout->addWidget(summary);
    auto *input = new QPlainTextEdit(dialog);
    input->setObjectName(QStringLiteral("hexDecodeInput"));
    input->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    input->setPlaceholderText(QStringLiteral("在这里输入或粘贴十六进制字节…"));
    input->setPlainText(m_lastHexInput);
    layout->addWidget(input, 1);
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("数据字节序："), dialog));
    auto *order = new QComboBox(dialog);
    order->setObjectName(QStringLiteral("hexByteOrder"));
    order->addItems({QStringLiteral("小端（与当前目标一致）"), QStringLiteral("大端（逐字段解释）")});
    order->setCurrentIndex(m_lastHexByteOrder);
    order->setToolTip(QStringLiteral("仅改变字段内的字节解释；字段偏移/对齐仍来自已导入布局。大端位域和大端 long double 暂不支持。"));
    row->addWidget(order);
    row->addStretch();
    auto *parse = new QPushButton(QStringLiteral("解析"), dialog);
    parse->setObjectName(QStringLiteral("parseHexButton"));
    row->addWidget(parse);
    layout->addLayout(row);
    auto *status = new QLabel(QStringLiteral("输入数据后点击“解析”，结果按字段层级显示。"), dialog);
    status->setObjectName(QStringLiteral("hexDecodeStatus"));
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status);
    auto *tree = new QTreeWidget(dialog);
    tree->setObjectName(QStringLiteral("hexDecodedFields"));
    tree->setHeaderLabels({QStringLiteral("字段"), QStringLiteral("类型"), QStringLiteral("绝对字节偏移"),
                           QStringLiteral("原始字节"), QStringLiteral("解析值"), QStringLiteral("说明")});
    tree->setAlternatingRowColors(true);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tree->header()->setStretchLastSection(true);
    layout->addWidget(tree, 3);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    auto *apply = buttons->addButton(QStringLiteral("填入可编辑字段"), QDialogButtonBox::ActionRole);
    apply->setObjectName(QStringLiteral("applyDecodedValuesButton"));
    apply->style()->unpolish(apply);
    apply->style()->polish(apply);
    apply->setEnabled(false);
    apply->setToolTip(QStringLiteral("仅填入已解析且通过校验的基础类型字段。原始数据中的填充和只读数组不会保存到字段值；二进制预览仍将这些区域补 0，并使用小端序。"));
    layout->addWidget(buttons);
    // 多个按钮回调需要共享这份结果；shared_ptr 使局部函数结束后数据仍然有效。
    auto values = std::make_shared<QHash<QString, QString>>();
    // 输入或字节序改变后清空旧结果，防止将旧数据误填回主窗口。
    const auto reset = [tree, status, apply, values] {
        tree->clear();
        values->clear();
        apply->setEnabled(false);
        status->setText(QStringLiteral("输入已更新，请点击“解析”。"));
    };
    connect(input, &QPlainTextEdit::textChanged, dialog, [this, input, reset] {
        m_lastHexInput = input->toPlainText();
        reset();
    });
    connect(order, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, [this, reset](int index) {
        m_lastHexByteOrder = index;
        reset();
    });
    // 捕获当前定义；窗口模态阻止用户解析期间切换主窗口的定义。
    const StructInfo structure = m_parsedStructure;
    connect(parse, &QPushButton::clicked, dialog, [structure, input, order, tree, status, apply, values, reset] {
        reset();
        // 第一步把用户文字转换成原始字节；第二步按当前结构体布局解释这些字节。
        const BinaryResult hex = parseHexBytes(input->toPlainText());
        if (!hex.success) {
            status->setText(hex.diagnostics.join(QLatin1Char('\n')));
            return;
        }
        const DecodeResult result = decodeStructure(structure, hex.bytes,
            order->currentIndex() == 0 ? BinaryByteOrder::LittleEndian : BinaryByteOrder::BigEndian);
        if (!result.success) {
            status->setText(result.diagnostics.join(QLatin1Char('\n')));
            return;
        }
        // 递归 lambda 与 appendFields() 的思路相同，用于显示嵌套的解码结果。
        std::function<void(QTreeWidgetItem *, const QVector<DecodedField> &)> append;
        append = [&append](QTreeWidgetItem *parent, const QVector<DecodedField> &fields) {
            for (const DecodedField &field : fields) {
                QString raw = QString::fromLatin1(field.bytes.left(32).toHex(' ').toUpper());
                if (field.bytes.size() > 32)
                    raw += QStringLiteral(" …（共 %1 字节）").arg(field.bytes.size());
                auto *item = new QTreeWidgetItem(parent, {field.name, field.typeName,
                    byteOffset(field.offsetBits), raw, field.value, field.note});
                item->setToolTip(0, field.path);
                item->setToolTip(2, QStringLiteral("绝对偏移 %1 bit，字段宽度 %2 bit；位域原始字节含相邻位。")
                                 .arg(field.offsetBits).arg(field.widthBits));
                append(item, field.children);
            }
        };
        auto *root = new QTreeWidgetItem(tree, {structure.name, QStringLiteral("struct"), QStringLiteral("0")});
        append(root, result.fields);
        tree->expandAll();
        *values = result.editableValues;
        apply->setEnabled(!values->isEmpty());
        status->setText(QStringLiteral("已解析 %1 字节，%2 个字段可填回。数组/不支持的只读类型显示原始字节；NaN/无穷大仅显示。填回只更新字段值，预览按目标小端序编码，数组和填充仍补 0。")
                        .arg(hex.bytes.size()).arg(values->size()));
    });
    connect(apply, &QPushButton::clicked, dialog, [this, values, status] {
        applyDecodedValues(*values);
        status->setText(QStringLiteral("已将 %1 个解析值填入主窗口。仅更新字段值；数组和填充字节不会保留，二进制预览使用目标小端序。").arg(values->size()));
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    dialog->show();
}

void MainWindow::applyDecodedValues(const QHash<QString, QString> &values)
{
    // 只遍历当前树，避免命中重载后等待延迟销毁的旧编辑控件。
    for (QTreeWidgetItemIterator it(m_fields); *it; ++it) {
        const auto value = values.constFind((*it)->toolTip(0));
        if (value == values.cend())
            continue;
        QWidget *widget = m_fields->itemWidget(*it, 1);
        // 设置控件值会触发原来的输入信号，同时刷新内存值和错误提示。
        if (auto *editor = qobject_cast<QLineEdit *>(widget))
            editor->setText(value.value());
        else if (auto *combo = qobject_cast<QComboBox *>(widget))
            combo->setCurrentText(value.value());
    }
    updateValueStatus();
}

bool MainWindow::importHexText(const QString &text, QString &error)
{
    error.clear();
    if (!m_hasStructure) {
        error = QStringLiteral("请先加载结构体定义。");
        return false;
    }
    const BinaryResult hex = parseHexBytes(text);
    if (!hex.success) {
        error = hex.diagnostics.join(QLatin1Char('\n'));
        return false;
    }
    const DecodeResult result = decodeStructure(m_parsedStructure, hex.bytes);
    if (!result.success) {
        error = result.diagnostics.join(QLatin1Char('\n'));
        return false;
    }
    // 例如 NaN 可供解析预览，但不能导入编辑器；先检查全部字段，避免只导入一部分。
    QStringList missing;
    for (auto it = m_fieldValues.cbegin(); it != m_fieldValues.cend(); ++it) {
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
    applyDecodedValues(result.editableValues);
    m_lastHexInput = text;
    m_status->setText(QStringLiteral("已导入 %1 字节，更新 %2 个字段。可直接修改字段值，再点击“查看二进制”生成新数据。只读字段和填充字节未保存。")
                      .arg(hex.bytes.size()).arg(result.editableValues.size()));
    return true;
}

void MainWindow::importHexFile()
{
    if (!m_hasStructure)
        return;
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("从文件导入16进制数据"),
        QFileInfo(m_loadedPath).absolutePath(),
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
    if (!m_hasStructure)
        return;
    auto *dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("hexImportDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowTitle(QStringLiteral("导入16进制数据：%1").arg(m_parsedStructure.name));
    dialog->resize(760, 440);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    auto *summary = new QLabel(QStringLiteral(
        "导入到 %1：请输入完整的 %2 字节（含填充），按当前目标的小端序读取。\n"
        "支持连续十六进制、空格/换行/逗号分隔及 0x 前缀。可直接粘贴“复制十六进制”的内容，不含偏移地址。\n"
        "导入后替换可编辑字段的值，可继续在主界面修改。只读字段和填充字节不保存，再次生成二进制时按现有规则处理。")
        .arg(m_parsedStructure.name).arg(m_parsedStructure.size), dialog);
    summary->setObjectName(QStringLiteral("dialogSummary"));
    summary->setTextFormat(Qt::PlainText);
    summary->setWordWrap(true);
    layout->addWidget(summary);
    auto *input = new QPlainTextEdit(dialog);
    input->setObjectName(QStringLiteral("hexImportInput"));
    input->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    input->setPlaceholderText(QStringLiteral("例如：78 56 34 12 22 11 44 33 66 55"));
    input->setPlainText(m_lastHexInput);
    layout->addWidget(input, 1);
    auto *status = new QLabel(dialog);
    status->setObjectName(QStringLiteral("hexImportStatus"));
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("导入"));
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("importHexButton"));
    buttons->button(QDialogButtonBox::Ok)->style()->unpolish(buttons->button(QDialogButtonBox::Ok));
    buttons->button(QDialogButtonBox::Ok)->style()->polish(buttons->button(QDialogButtonBox::Ok));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);
    connect(input, &QPlainTextEdit::textChanged, dialog, [this, input, status] {
        m_lastHexInput = input->toPlainText();
        status->clear();
    });
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, input, status] {
        QString error;
        if (!importHexText(input->toPlainText(), error)) {
            status->setText(QStringLiteral("导入失败：%1").arg(error));
            return;
        }
        dialog->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    dialog->show();
    input->setFocus();
}

// 构造函数分三步：创建控件、用布局排列控件、连接信号；最后首次加载文件。
MainWindow::MainWindow(const QString &filePath, QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("StructHex · 结构体数据编辑器"));
    setObjectName(QStringLiteral("mainWindow"));
    setAttribute(Qt::WA_StyledBackground);
    resize(1440, 900);

    // QVBoxLayout 从上到下排列各行；每一行再用 QHBoxLayout 从左到右排列。
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(12);

    auto *brand = new QFrame(this);
    brand->setObjectName(QStringLiteral("brandHeader"));
    auto *brandRow = new QHBoxLayout(brand);
    brandRow->setContentsMargins(18, 12, 20, 12);
    brandRow->setSpacing(12);
    auto *logo = new QLabel(brand);
    logo->setPixmap(QIcon(QStringLiteral(":/icons/structhex.png")).pixmap(52, 52));
    brandRow->addWidget(logo);
    auto *brandText = new QVBoxLayout;
    brandText->setSpacing(2);
    auto *brandTitle = new QLabel(QStringLiteral("StructHex"), brand);
    brandTitle->setObjectName(QStringLiteral("brandTitle"));
    auto *brandSubtitle = new QLabel(QStringLiteral("结构体数据编辑器"), brand);
    brandSubtitle->setObjectName(QStringLiteral("brandSubtitle"));
    brandText->addWidget(brandTitle);
    brandText->addWidget(brandSubtitle);
    brandRow->addLayout(brandText);
    brandRow->addStretch();
    auto *badge = new QLabel(QStringLiteral("C / C++  ·  HEX"), brand);
    badge->setObjectName(QStringLiteral("brandBadge"));
    brandRow->addWidget(badge);
    layout->addWidget(brand);

    auto *definitionPanel = new QFrame(this);
    definitionPanel->setObjectName(QStringLiteral("definitionPanel"));
    auto *definitionLayout = new QVBoxLayout(definitionPanel);
    definitionLayout->setContentsMargins(16, 14, 16, 14);
    definitionLayout->setSpacing(10);
    layout->addWidget(definitionPanel);

    auto *sourceRow = new QHBoxLayout;
    sourceRow->addWidget(new QLabel(QStringLiteral("结构定义文件："), this));
    m_filePath = new QLineEdit(QDir::toNativeSeparators(filePath), this);
    // objectName 是程序内部标识，测试和 findChild() 用它查找控件，不是界面显示文字。
    m_filePath->setObjectName(QStringLiteral("headerFilePath"));
    m_filePath->setPlaceholderText(QStringLiteral("输入头文件或结构定义 JSON 路径，按回车加载"));
    sourceRow->addWidget(m_filePath, 1);
    auto *openButton = new QPushButton(QStringLiteral("打开结构定义…"), this);
    openButton->setObjectName(QStringLiteral("openDefinitionButton"));
    auto *reloadButton = new QPushButton(QStringLiteral("重新加载"), this);
    reloadButton->setObjectName(QStringLiteral("reloadDefinitionButton"));
    sourceRow->addWidget(openButton);
    sourceRow->addWidget(reloadButton);
    definitionLayout->addLayout(sourceRow);
    m_sourceInfo = new QLabel(this);
    m_sourceInfo->setObjectName(QStringLiteral("definitionSourceInfo"));
    m_sourceInfo->setTextFormat(Qt::PlainText);
    m_sourceInfo->setWordWrap(true);
    definitionLayout->addWidget(m_sourceInfo);

    auto *nameRow = new QHBoxLayout;
    auto *nameLabel = new QLabel(QStringLiteral("选择结构体："), this);
    nameRow->addWidget(nameLabel);
    m_structName = new QComboBox(this);
    m_structName->setObjectName(QStringLiteral("structSelector"));
    m_structName->setMinimumContentsLength(24);
    m_structName->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    nameLabel->setBuddy(m_structName);
    nameRow->addWidget(m_structName, 1);
    auto *expandButton = new QPushButton(QStringLiteral("全部展开"), this);
    auto *collapseButton = new QPushButton(QStringLiteral("全部折叠"), this);
    nameRow->addWidget(expandButton);
    nameRow->addWidget(collapseButton);
    definitionLayout->addLayout(nameRow);
    auto *actionRow = new QHBoxLayout;
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
    layout->addLayout(actionRow);

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
    m_status->setObjectName(QStringLiteral("operationStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    m_diagnostics = new QPlainTextEdit(this);
    m_diagnostics->setReadOnly(true);
    m_diagnostics->setMaximumHeight(130);
    layout->addWidget(m_diagnostics);

    // 按钮只触发“重新读取文件并刷新界面”，不参与 libclang 的 AST 遍历。
    connect(reloadButton, &QPushButton::clicked, this, [this] { reloadDefinition(); });
    connect(m_filePath, &QLineEdit::returnPressed, this, [this] { reloadDefinition(); });
    connect(m_structName, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { showSelectedStructure(); });
    connect(expandButton, &QPushButton::clicked, m_fields, &QTreeWidget::expandAll);
    connect(collapseButton, &QPushButton::clicked, m_fields, &QTreeWidget::collapseAll);
    connect(m_binaryButton, &QPushButton::clicked, this, [this] { showBinary(); });
    connect(m_decodeButton, &QPushButton::clicked, this, [this] { showHexDecoder(); });
    connect(m_importHexButton, &QPushButton::clicked, this, [this] { showHexImporter(); });
    connect(m_importHexFileButton, &QPushButton::clicked, this, [this] { importHexFile(); });
    connect(m_exportDefinitionButton, &QPushButton::clicked, this, [this] { exportDefinitionJson(); });
    connect(openButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("打开结构定义"), m_filePath->text(),
            QStringLiteral("结构定义 (*.h *.hpp *.hh *.hxx *.json);;C++ 头文件 (*.h *.hpp *.hh *.hxx);;结构定义 JSON (*.json);;所有文件 (*)"));
        if (!path.isEmpty()) {
            m_filePath->setText(QDir::toNativeSeparators(path));
            reloadDefinition();
        }
    });
    reloadDefinition();
}

// 读取文件是两个入口的共同起点：头文件走 libclang，JSON 直接恢复相同的数据结构。
void MainWindow::reloadDefinition()
{
    const QString path = QDir::fromNativeSeparators(m_filePath->text().trimmed());
    m_loadedPath = QFileInfo(path).absoluteFilePath();
    m_jsonMode = QFileInfo(path).suffix().compare(QLatin1String("json"), Qt::CaseInsensitive) == 0;
    // 清空/添加下拉选项也会触发选择变化；填充列表期间暂时屏蔽信号，避免显示半成品。
    // blocker 离开函数时自动恢复原信号状态，这种按作用域管理资源的方式称为 RAII。
    const QSignalBlocker blocker(m_structName);
    m_structName->clear();
    m_structures.clear();
    m_fileDiagnostics.clear();
    if (m_jsonMode) {
        const ParseResult result = loadStructureDefinitionJson(path);
        m_fileDiagnostics = result.diagnostics;
        if (result.success) {
            m_structures.append(result);
            m_structName->addItem(result.structure.name);
        }
    } else {
        const HeaderParseResult result = parseHeaderStructures(path);
        m_fileDiagnostics = result.diagnostics;
        m_structures = result.structures;
        for (const ParseResult &structure : m_structures)
            m_structName->addItem(structure.structure.name);
        // 同一头文件重载或从 JSON 切回时，尽量保持原选择；新文件默认选择第一项。
        const int previous = m_headerPath == m_loadedPath
            ? m_structName->findText(m_headerStructName) : -1;
        if (previous >= 0)
            m_structName->setCurrentIndex(previous);
    }
    m_structName->setEnabled(!m_jsonMode && m_structures.size() > 1);
    m_structName->setToolTip(m_jsonMode ? QStringLiteral("结构体名称来自 JSON 文件。")
        : QStringLiteral("自动发现主头文件中的结构体；选择后立即显示字段，无需输入名称。"));
    showSelectedStructure();
}

// 将当前候选应用到界面：下拉框下标与 m_structures 中的下标一一对应。
void MainWindow::showSelectedStructure()
{
    for (auto *dialog : findChildren<QDialog *>(QStringLiteral("hexDecodeDialog")))
        dialog->close();
    for (auto *dialog : findChildren<QDialog *>(QStringLiteral("hexImportDialog")))
        dialog->close();
    const int index = m_structName->currentIndex();
    ParseResult result;
    if (index >= 0 && index < m_structures.size())
        result = m_structures.at(index);
    result.diagnostics = m_fileDiagnostics + (m_jsonMode ? QStringList() : result.diagnostics);
    const QString structName = m_structName->currentText();
    if (!m_jsonMode && index >= 0) {
        m_headerPath = m_loadedPath;
        m_headerStructName = structName;
    }
    m_sourceInfo->setText((m_jsonMode ? QStringLiteral("来源：结构定义 JSON")
                                    : QStringLiteral("来源：头文件解析    |    发现 %1 个结构体").arg(m_structures.size()))
                         + (result.success ? QStringLiteral("    |    目标：Windows x64 / MinGW，小端序")
                                           : QStringLiteral("    |    加载失败")));

    // 清空旧结果，避免解析失败后还显示上一个文件的字段。
    m_fields->clear();
    const QString source = m_loadedPath + QLatin1Char('\n') + structName;
    // 只有同一文件、同一结构体才复用旧输入；具体字段是否仍合法由 appendFields() 检查。
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
    m_decodeButton->setEnabled(false);
    m_importHexButton->setEnabled(false);
    m_importHexFileButton->setEnabled(false);
    m_exportDefinitionButton->setEnabled(false);
    m_diagnostics->setPlainText(result.diagnostics.join(QLatin1Char('\n')));
    m_diagnostics->setVisible(!result.diagnostics.isEmpty());
    if (!result.success) {
        m_structureName->setText(QStringLiteral("加载失败"));
        m_summary->setText(QStringLiteral("尚无可显示的结构体信息"));
        m_status->setText(QStringLiteral("请检查结构定义文件和下方诊断信息，修正后点击“重新加载”；也可选择其他结构体。"));
        return;
    }

    const StructInfo &structure = result.structure;
    m_parsedStructure = structure;
    m_hasStructure = true;
    m_decodeButton->setEnabled(true);
    m_importHexButton->setEnabled(true);
    m_importHexFileButton->setEnabled(true);
    m_exportDefinitionButton->setEnabled(true);
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
        ? QStringLiteral("加载成功。点击箭头展开或折叠嵌套字段；修改并保存结构定义文件后，点击“重新加载”更新。")
        : QStringLiteral("加载成功，下方有诊断提示。"));
}
