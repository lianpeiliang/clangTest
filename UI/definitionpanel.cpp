#include "definitionpanel.h"
#include "definitionfilebar.h"
#include "structureselectorbar.h"
#include <QLabel>
#include <QVBoxLayout>

DefinitionPanel::DefinitionPanel(const QString &filePath, QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("definitionPanel"));
    auto *definitionLayout = new QVBoxLayout(this);
    definitionLayout->setContentsMargins(16, 14, 16, 14);
    definitionLayout->setSpacing(10);

    m_fileBar = new DefinitionFileBar(filePath, this);
    definitionLayout->addWidget(m_fileBar);
    m_sourceInfo = new QLabel(this);
    m_sourceInfo->setObjectName(QStringLiteral("definitionSourceInfo"));
    m_sourceInfo->setTextFormat(Qt::PlainText);
    m_sourceInfo->setWordWrap(true);
    definitionLayout->addWidget(m_sourceInfo);
    m_selector = new StructureSelectorBar(this);
    definitionLayout->addWidget(m_selector);

    connect(m_fileBar, &DefinitionFileBar::loadRequested, this, &DefinitionPanel::loadRequested);
    connect(m_fileBar, &DefinitionFileBar::exportRequested, this, &DefinitionPanel::exportRequested);
    connect(m_selector, &StructureSelectorBar::structureSelected, this, &DefinitionPanel::structureSelected);
    connect(m_selector, &StructureSelectorBar::expandRequested, this, &DefinitionPanel::expandRequested);
    connect(m_selector, &StructureSelectorBar::collapseRequested, this, &DefinitionPanel::collapseRequested);
}

void DefinitionPanel::setDefinitions(const QStringList &names, int selectedIndex, bool jsonMode)
{
    m_selector->setDefinitions(names, selectedIndex, jsonMode);
}

void DefinitionPanel::setSourceState(bool jsonMode, bool loaded)
{
    m_fileBar->setExportEnabled(loaded);
    m_sourceInfo->setText((jsonMode ? QStringLiteral("来源：结构定义 JSON")
                                    : QStringLiteral("来源：头文件解析    |    发现 %1 个结构体").arg(m_selector->count()))
                         + (loaded ? QStringLiteral("    |    目标：Windows x64 / MinGW，小端序")
                                           : QStringLiteral("    |    加载失败")));
}
