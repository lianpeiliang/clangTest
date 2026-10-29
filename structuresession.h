#ifndef STRUCTURESESSION_H
#define STRUCTURESESSION_H

#include "headerparser.h"

// 结构定义会话：管理文件来源、候选和当前选择，不依赖界面或字段输入值。
class StructureSession
{
public:
    // 重新读取头文件或定义 JSON；优先恢复同一头文件上次选中的结构体。
    // 加载失败会清空本次候选与选择，错误通过 diagnostics() 返回。
    void load(const QString &filePath);

    // 只切换缓存中的候选，不重新解析文件；非法索引清空选择并返回 false。
    // 返回 true 表示索引有效，该候选的布局是否成功仍应检查 hasStructure()。
    bool selectStructure(int index);
    QStringList structureNames() const;
    int selectedIndex() const { return m_selectedIndex; }
    bool isJson() const { return m_jsonMode; }
    const QString &loadedPath() const { return m_loadedPath; }
    QString currentName() const;
    QString sourceKey() const; // 文件绝对路径 + 结构体名，供编辑区判断是否恢复旧值。
    bool hasStructure() const;
    // 无有效选择时返回空定义；使用前先检查 hasStructure()。引用有效期至下一次 load()。
    const StructInfo &structure() const;
    QStringList diagnostics() const; // 当前文件诊断 + 当前候选诊断，JSON 诊断不重复合并。

private:
    QVector<ParseResult> m_structures; // 本次文件中的候选，选择失败项不影响其他项。
    QStringList m_fileDiagnostics;
    QString m_loadedPath;
    int m_selectedIndex = -1;
    bool m_jsonMode = false;
    QString m_headerPath;       // 上一次在头文件模式下选择的来源。
    QString m_headerStructName; // 切回该头文件或重新加载时优先恢复此名称。
};

#endif
