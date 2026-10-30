#include "structureselectorbar.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

StructureSelectorBar::StructureSelectorBar(QWidget *parent) : QWidget(parent)
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    auto *label = new QLabel(QStringLiteral("选择结构体："), this);
    row->addWidget(label);
    m_structName = new QComboBox(this);
    m_structName->setObjectName(QStringLiteral("structSelector"));
    m_structName->setMinimumContentsLength(24);
    m_structName->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    label->setBuddy(m_structName);
    row->addWidget(m_structName, 1);
    auto *expandButton = new QPushButton(QStringLiteral("全部展开"), this);
    auto *collapseButton = new QPushButton(QStringLiteral("全部折叠"), this);
    expandButton->setObjectName(QStringLiteral("expandFieldsButton"));
    collapseButton->setObjectName(QStringLiteral("collapseFieldsButton"));
    row->addWidget(expandButton);
    row->addWidget(collapseButton);
    connect(m_structName, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StructureSelectorBar::structureSelected);
    connect(expandButton, &QPushButton::clicked, this, &StructureSelectorBar::expandRequested);
    connect(collapseButton, &QPushButton::clicked, this, &StructureSelectorBar::collapseRequested);
}

void StructureSelectorBar::setDefinitions(const QStringList &names, int selectedIndex, bool jsonMode)
{
    const QSignalBlocker blocker(m_structName);
    m_structName->clear();
    m_structName->addItems(names);
    m_structName->setCurrentIndex(selectedIndex);
    m_structName->setEnabled(!jsonMode && m_structName->count() > 1);
    m_structName->setToolTip(jsonMode ? QStringLiteral("结构体名称来自 JSON 文件。")
        : QStringLiteral("自动发现主头文件中的结构体；选择后立即显示字段，无需输入名称。"));
}

int StructureSelectorBar::count() const
{
    return m_structName->count();
}
