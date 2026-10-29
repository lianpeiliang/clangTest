#include "structuresession.h"
#include "definitionjson.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

#define CHECK(c) do { if (!(c)) { QTextStream(stderr) << "FAIL " << __LINE__ << ": " << #c << '\n'; return 1; } } while (false)

static bool write(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    StructureSession session;
    CHECK(!session.hasStructure() && session.selectedIndex() == -1 && session.currentName().isEmpty());
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    const QString header = temporary.path() + "/Types.h";
    CHECK(write(header, "struct Good { int x; }; struct Second { bool active; }; struct Flexible { int count; char data[]; };"));
    session.load(header);
    CHECK(session.hasStructure() && !session.isJson() && session.currentName() == "Good");
    CHECK(session.structureNames() == QStringList({"Good", "Second", "Flexible"}));
    const QString originalKey = session.sourceKey();
    CHECK(session.loadedPath() == QFileInfo(header).absoluteFilePath());
    CHECK(session.selectStructure(2) && !session.hasStructure() && !session.diagnostics().isEmpty());
    CHECK(session.selectStructure(1) && session.hasStructure() && session.diagnostics().isEmpty());
    CHECK(session.sourceKey() != originalKey && session.structure().name == "Second");
    session.load(header);
    CHECK(session.currentName() == "Second" && session.selectedIndex() == 1);
    const QString json = temporary.path() + "/Saved.JSON";
    CHECK(saveStructureDefinitionJson(session.structure(), json));
    session.load(json);
    CHECK(session.isJson() && session.hasStructure() && session.selectedIndex() == 0);
    CHECK(session.structureNames() == QStringList({"Second"}));
    CHECK(session.diagnostics() == loadStructureDefinitionJson(json).diagnostics);
    CHECK(session.sourceKey().startsWith(QFileInfo(json).absoluteFilePath()));
    session.load(header);
    CHECK(session.currentName() == "Second" && !session.isJson());
    // 文件变化不影响缓存中的切换，只有 load 才重新解析。
    CHECK(write(header, "struct SyntaxError { UnknownType x; };"));
    CHECK(session.selectStructure(0) && session.hasStructure() && session.currentName() == "Good");
    session.load(header);
    CHECK(!session.hasStructure() && session.structureNames().isEmpty() && session.selectedIndex() == -1);
    CHECK(session.currentName().isEmpty() && session.structure().fields.isEmpty() && !session.diagnostics().isEmpty());
    CHECK(write(header, "struct Replacement { unsigned value; };"));
    session.load(header);
    CHECK(session.hasStructure() && session.currentName() == "Replacement");
    CHECK(!session.selectStructure(99) && !session.hasStructure() && session.currentName().isEmpty());
    CHECK(session.selectStructure(0) && session.hasStructure());
    session.load(temporary.path() + "/missing.json");
    CHECK(session.isJson() && !session.hasStructure() && session.structureNames().isEmpty());
    CHECK(!session.diagnostics().isEmpty());
    session.load(header);
    CHECK(session.hasStructure() && session.currentName() == "Replacement");
    QTextStream(stdout) << "PASS: headless session, candidates, cached selection, header/JSON restore, source identity, isolated layout errors and failure reset\n";
    return 0;
}
