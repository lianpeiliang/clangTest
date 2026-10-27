#include "mainwindow.h"
#include "headerparser.h"

#include <QDir>
#include <QFileDialog>
#include <QFont>
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

void appendFields(QTreeWidgetItem *parent, const QVector<FieldInfo> &fields)
{
    for (const FieldInfo &field : fields) {
        auto *item = new QTreeWidgetItem(parent, QStringList{
            field.name, field.typeName, field.canonicalTypeName,
            QString::number(field.size), QString::number(field.alignment),
            byteOffset(field.offsetBits), byteOffset(field.absoluteOffsetBits),
            QString::number(field.offsetBits), QString::number(field.absoluteOffsetBits)
        });
        for (int column = 3; column < item->columnCount(); ++column)
            item->setTextAlignment(column, Qt::AlignCenter);
        appendFields(item, field.children);
    }
}

} // namespace

MainWindow::MainWindow(const QString &filePath, QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("C++ 结构体查看器"));
    resize(1120, 820);

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

    // 根节点表示结构体，子节点递归显示字段；禁止直接编辑查询结果。
    m_fields = new QTreeWidget(this);
    m_fields->setObjectName(QStringLiteral("fieldsTree"));
    m_fields->setColumnCount(9);
    m_fields->setHeaderLabels(QStringList{
        QStringLiteral("字段名称"), QStringLiteral("字段类型"), QStringLiteral("展开后的类型"),
        QStringLiteral("大小（字节）"), QStringLiteral("自然对齐（字节）"),
        QStringLiteral("相对偏移（字节）"), QStringLiteral("绝对偏移（字节）"),
        QStringLiteral("相对偏移（bit）"), QStringLiteral("绝对偏移（bit）")
    });
    m_fields->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fields->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fields->setAlternatingRowColors(true);
    m_fields->setWordWrap(false);
    m_fields->setUniformRowHeights(true);
    m_fields->setAnimated(true);
    m_fields->header()->setStretchLastSection(false);
    m_fields->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_fields->header()->setMinimumSectionSize(80);
    m_fields->headerItem()->setToolTip(2,
        QStringLiteral("展开 typedef 等类型别名，例如 uint16_t 对应 unsigned short。"));
    m_fields->headerItem()->setToolTip(4,
        QStringLiteral("类型本身的自然对齐，可能大于结构体 pack 指定的对齐值。"));
    for (int column : {5, 7})
        m_fields->headerItem()->setToolTip(column,
            QStringLiteral("字段相对于直接所属结构体起点的偏移。"));
    for (int column : {6, 8})
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
    m_diagnostics->setPlainText(result.diagnostics.join(QLatin1Char('\n')));
    m_diagnostics->setVisible(!result.diagnostics.isEmpty());
    if (!result.success) {
        m_structureName->setText(QStringLiteral("解析失败"));
        m_summary->setText(QStringLiteral("尚无可显示的结构体信息"));
        m_status->setText(QStringLiteral("请检查文件路径、结构体名称和下方诊断信息，修正后点击“重新解析”。"));
        return;
    }

    const StructInfo &structure = result.structure;
    // 结构体名来自 AST 查询结果，不是在界面中写死的显示文本。
    m_structureName->setText(QStringLiteral("结构体：%1").arg(structure.name));
    m_summary->setText(QStringLiteral("%1 个直接字段    |    大小：%2 字节    |    结构体对齐：%3 字节")
                           .arg(structure.fields.size()).arg(structure.size).arg(structure.alignment));
    auto *root = new QTreeWidgetItem(m_fields, QStringList{
        structure.name, QStringLiteral("struct"), structure.name,
        QString::number(structure.size), QString::number(structure.alignment),
        QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0")
    });
    for (int column = 3; column < root->columnCount(); ++column)
        root->setTextAlignment(column, Qt::AlignCenter);
    appendFields(root, structure.fields);
    // 默认展开根节点和第一层嵌套；更深层可手动展开或点击“全部展开”。
    m_fields->expandToDepth(1);
    m_status->setText(result.diagnostics.isEmpty()
        ? QStringLiteral("解析成功。点击箭头展开或折叠嵌套字段；修改并保存头文件后，点击“重新解析”更新。")
        : QStringLiteral("解析成功，下方有诊断提示。"));
}
