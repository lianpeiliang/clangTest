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
#include <QTableWidget>
#include <QVBoxLayout>

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

    // 每一行对应一个 FieldInfo，单元格显示的是解析结果，禁止直接编辑。
    m_fields = new QTableWidget(0, 7, this);
    m_fields->setObjectName(QStringLiteral("fieldsTable"));
    m_fields->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("字段名称"), QStringLiteral("字段类型"), QStringLiteral("展开后的类型"),
        QStringLiteral("大小（字节）"), QStringLiteral("自然对齐（字节）"),
        QStringLiteral("偏移（字节）"), QStringLiteral("偏移（bit）")
    });
    m_fields->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fields->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fields->setAlternatingRowColors(true);
    m_fields->setWordWrap(false);
    m_fields->verticalHeader()->setDefaultSectionSize(25);
    m_fields->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_fields->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fields->horizontalHeaderItem(2)->setToolTip(
        QStringLiteral("展开 typedef 等类型别名，例如 uint16_t 对应 unsigned short。"));
    m_fields->horizontalHeaderItem(4)->setToolTip(
        QStringLiteral("类型本身的自然对齐，可能大于结构体 pack 指定的对齐值。"));
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
    m_fields->setRowCount(0);
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
    m_summary->setText(QStringLiteral("%1 个字段    |    大小：%2 字节    |    结构体对齐：%3 字节")
                           .arg(structure.fields.size()).arg(structure.size).arg(structure.alignment));
    m_fields->setRowCount(structure.fields.size());
    for (int row = 0; row < structure.fields.size(); ++row) {
        const FieldInfo &field = structure.fields.at(row);
        // 普通字段除以 8 即为字节偏移；遇到位域时保留余下的 bit，避免丢失信息。
        const QString byteOffset = field.offsetBits % 8 == 0
            ? QString::number(field.offsetBits / 8)
            : QStringLiteral("%1 + %2 bit").arg(field.offsetBits / 8).arg(field.offsetBits % 8);
        const QStringList values{
            field.name, field.typeName, field.canonicalTypeName,
            QString::number(field.size), QString::number(field.alignment),
            byteOffset, QString::number(field.offsetBits)
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column >= 3)
                item->setTextAlignment(Qt::AlignCenter);
            m_fields->setItem(row, column, item);
        }
    }
    m_status->setText(result.diagnostics.isEmpty()
        ? QStringLiteral("解析成功。修改并保存头文件后，点击“重新解析”更新表格。")
        : QStringLiteral("解析成功，下方有诊断提示。"));
}
