#include "structuresession.h"
#include "definitionjson.h"
#include <QDir>
#include <QFileInfo>

void StructureSession::load(const QString &filePath)
{
    const QString path = QDir::fromNativeSeparators(filePath.trimmed());
    m_loadedPath = QFileInfo(path).absoluteFilePath();
    m_jsonMode = QFileInfo(path).suffix().compare(QLatin1String("json"), Qt::CaseInsensitive) == 0;
    m_structures.clear();
    m_fileDiagnostics.clear();
    m_selectedIndex = -1;
    if (m_jsonMode) {
        const ParseResult result = loadStructureDefinitionJson(path);
        m_fileDiagnostics = result.diagnostics;
        if (result.success)
            m_structures.append(result);
    } else {
        const HeaderParseResult result = parseHeaderStructures(path);
        m_structures = result.structures;
        m_fileDiagnostics = result.diagnostics;
    }
    int index = m_structures.isEmpty() ? -1 : 0;
    if (!m_jsonMode && m_loadedPath == m_headerPath) {
        const int previous = structureNames().indexOf(m_headerStructName);
        if (previous >= 0)
            index = previous;
    }
    selectStructure(index);
}

bool StructureSession::selectStructure(int index)
{
    m_selectedIndex = index >= 0 && index < m_structures.size() ? index : -1;
    if (m_selectedIndex >= 0 && !m_jsonMode) {
        m_headerPath = m_loadedPath;
        m_headerStructName = currentName();
    }
    return m_selectedIndex >= 0;
}

QStringList StructureSession::structureNames() const
{
    QStringList names;
    for (const ParseResult &result : m_structures)
        names.append(result.structure.name);
    return names;
}

QString StructureSession::currentName() const
{
    return m_selectedIndex >= 0 ? m_structures.at(m_selectedIndex).structure.name : QString();
}

QString StructureSession::sourceKey() const
{
    return m_loadedPath + QLatin1Char('\n') + currentName();
}

bool StructureSession::hasStructure() const
{
    return m_selectedIndex >= 0 && m_structures.at(m_selectedIndex).success;
}

const StructInfo &StructureSession::structure() const
{
    static const StructInfo empty;
    return m_selectedIndex >= 0 ? m_structures.at(m_selectedIndex).structure : empty;
}

QStringList StructureSession::diagnostics() const
{
    return m_fileDiagnostics + (m_selectedIndex >= 0 && !m_jsonMode
        ? m_structures.at(m_selectedIndex).diagnostics : QStringList());
}
