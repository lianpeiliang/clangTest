#include "hexdialogs.h"
#include "binarydecoder.h"
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <memory>
namespace {
QString byteOffset(qint64 bits)
{
    return bits % 8 == 0 ? QString::number(bits / 8)
        : QStringLiteral("%1 + %2 bit").arg(bits / 8).arg(bits % 8);
}
}
HexDialogs::HexDialogs(QWidget *owner) : QObject(owner), m_owner(owner) {}
void HexDialogs::closeEditors()
{
    for (const QString &name : {QStringLiteral("hexDecodeDialog"), QStringLiteral("hexImportDialog")})
        for (auto *dialog : m_owner->findChildren<QDialog *>(name))
            dialog->close();
}

void HexDialogs::showPreview(const StructInfo &structure, const QByteArray &bytes, const QString &sourcePath)
{
    // 主窗口作为父对象；WA_DeleteOnClose 在关闭时释放弹窗及其子控件。
    auto *dialog = new QDialog(m_owner);
    dialog->setObjectName(QStringLiteral("binaryPreviewDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("十六进制预览：%1").arg(structure.name));
    dialog->resize(860, 520);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    auto *summary = new QLabel(QStringLiteral(
        "%1    |    %2 字节    |    Windows x64 / MinGW，小端序\n"
        "当前值快照；每行 16 字节，左侧为十六进制偏移，数组、填充字节和未命名位域补 0。")
        .arg(structure.name).arg(bytes.size()), dialog);
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
    hex->setPlainText(formatHexDump(bytes));
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
    // 按值捕获 bytes，复制按钮使用的是打开弹窗那一刻的快照。
    connect(copy, &QPushButton::clicked, dialog, [bytes, exportStatus] {
        QApplication::clipboard()->setText(QString::fromLatin1(bytes.toHex(' ').toUpper()));
        exportStatus->setText(QStringLiteral("复制成功：已将 %1 字节的十六进制数据复制到剪贴板。")
                              .arg(bytes.size()));
    });
    const QString suggested = QFileInfo(sourcePath).dir().filePath(
        QString(structure.name).replace(QStringLiteral("::"), QStringLiteral("_")) + QStringLiteral(".hex"));
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

void HexDialogs::showDecoder(const StructInfo &structure, const ApplyValues &applyValues)
{
    auto *dialog = new QDialog(m_owner);
    dialog->setObjectName(QStringLiteral("hexDecodeDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowTitle(QStringLiteral("解析十六进制：%1").arg(structure.name));
    dialog->resize(1100, 720);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    auto *summary = new QLabel(QStringLiteral(
        "%1：需要完整的 %2 字节（含结构体填充）。支持连续十六进制、空格/换行/逗号分隔及 0x 前缀。\n"
        "例如：78 56 34 12 或 0x78, 0x56, 0x34, 0x12。请粘贴字节本身，不含偏移地址。")
        .arg(structure.name).arg(structure.size), dialog);
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
    connect(apply, &QPushButton::clicked, dialog, [applyValues, values, status] {
        applyValues(*values);
        status->setText(QStringLiteral("已将 %1 个解析值填入主窗口。仅更新字段值；数组和填充字节不会保留，二进制预览使用目标小端序。").arg(values->size()));
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    dialog->show();
}

void HexDialogs::showImporter(const StructInfo &structure, const ImportText &importText)
{
    auto *dialog = new QDialog(m_owner);
    dialog->setObjectName(QStringLiteral("hexImportDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowTitle(QStringLiteral("导入16进制数据：%1").arg(structure.name));
    dialog->resize(760, 440);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    auto *summary = new QLabel(QStringLiteral(
        "导入到 %1：请输入完整的 %2 字节（含填充），按当前目标的小端序读取。\n"
        "支持连续十六进制、空格/换行/逗号分隔及 0x 前缀。可直接粘贴“复制十六进制”的内容，不含偏移地址。\n"
        "导入后替换可编辑字段的值，可继续在主界面修改。只读字段和填充字节不保存，再次生成二进制时按现有规则处理。")
        .arg(structure.name).arg(structure.size), dialog);
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
    connect(buttons, &QDialogButtonBox::accepted, dialog, [importText, dialog, input, status] {
        QString error;
        if (!importText(input->toPlainText(), error)) {
            status->setText(QStringLiteral("导入失败：%1").arg(error));
            return;
        }
        dialog->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    dialog->show();
    input->setFocus();
}
