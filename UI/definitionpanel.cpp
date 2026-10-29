#include "definitionpanel.h"
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

DefinitionPanel::DefinitionPanel(const QString &filePath, QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("definitionPanel"));
    auto *definitionLayout = new QVBoxLayout(this);
    definitionLayout->setContentsMargins(16, 14, 16, 14);
    definitionLayout->setSpacing(10);

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
    expandButton->setObjectName(QStringLiteral("expandFieldsButton"));
    collapseButton->setObjectName(QStringLiteral("collapseFieldsButton"));
    nameRow->addWidget(expandButton);
    nameRow->addWidget(collapseButton);
    definitionLayout->addLayout(nameRow);
    connect(reloadButton, &QPushButton::clicked, this, [this] { emit loadRequested(m_filePath->text()); });
    connect(m_filePath, &QLineEdit::returnPressed, this, [this] { emit loadRequested(m_filePath->text()); });
    connect(m_structName, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DefinitionPanel::structureSelected);
    connect(expandButton, &QPushButton::clicked, this, &DefinitionPanel::expandRequested);
    connect(collapseButton, &QPushButton::clicked, this, &DefinitionPanel::collapseRequested);
    connect(openButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("打开结构定义"), m_filePath->text(),
            QStringLiteral("结构定义 (*.h *.hpp *.hh *.hxx *.json);;C++ 头文件 (*.h *.hpp *.hh *.hxx);;结构定义 JSON (*.json);;所有文件 (*)"));
        if (!path.isEmpty()) {
            m_filePath->setText(QDir::toNativeSeparators(path));
            emit loadRequested(path);
        }
    });
}

void DefinitionPanel::setDefinitions(const QStringList &names, int selectedIndex, bool jsonMode)
{
    const QSignalBlocker blocker(m_structName);
    m_structName->clear();
    m_structName->addItems(names);
    m_structName->setCurrentIndex(selectedIndex);
    m_structName->setEnabled(!jsonMode && m_structName->count() > 1);
    m_structName->setToolTip(jsonMode ? QStringLiteral("结构体名称来自 JSON 文件。")
        : QStringLiteral("自动发现主头文件中的结构体；选择后立即显示字段，无需输入名称。"));
}

void DefinitionPanel::setSourceState(bool jsonMode, bool loaded)
{
    m_sourceInfo->setText((jsonMode ? QStringLiteral("来源：结构定义 JSON")
                                    : QStringLiteral("来源：头文件解析    |    发现 %1 个结构体").arg(m_structName->count()))
                         + (loaded ? QStringLiteral("    |    目标：Windows x64 / MinGW，小端序")
                                           : QStringLiteral("    |    加载失败")));
}
