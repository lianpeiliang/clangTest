#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <clang-c/Index.h>

static QString toQString(CXString value)
{
    const QString result = QString::fromUtf8(clang_getCString(value));
    clang_disposeString(value);
    return result;
}

struct LayoutResult
{
    bool found = false;
    bool valid = true;
};

static CXChildVisitResult visitField(CXCursor cursor, CXCursor, CXClientData data)
{
    if (clang_getCursorKind(cursor) != CXCursor_FieldDecl)
        return CXChildVisit_Continue;

    LayoutResult &result = *static_cast<LayoutResult *>(data);
    const CXType type = clang_getCursorType(cursor);
    const long long size = clang_Type_getSizeOf(type);
    const long long alignment = clang_Type_getAlignOf(type);
    // libclang returns field offsets in bits; size and alignment are in bytes.
    const long long offsetBits = clang_Cursor_getOffsetOfField(cursor);
    if (size < 0 || alignment < 0 || offsetBits < 0) {
        qCritical() << "Cannot read layout for" << toQString(clang_getCursorSpelling(cursor));
        result.valid = false;
        return CXChildVisit_Continue;
    }

    qInfo().noquote()
        << QString("  %1: %2 (%3), size=%4 bytes, type alignment=%5 bytes, offset=%6 bits (%7 bytes)")
               .arg(toQString(clang_getCursorSpelling(cursor)))
               .arg(toQString(clang_getTypeSpelling(type)))
               .arg(toQString(clang_getTypeSpelling(clang_getCanonicalType(type))))
               .arg(size).arg(alignment).arg(offsetBits).arg(offsetBits / 8);
    return CXChildVisit_Continue;
}

static CXChildVisitResult visitRecord(CXCursor cursor, CXCursor, CXClientData data)
{
    if (!clang_Location_isFromMainFile(clang_getCursorLocation(cursor)))
        return CXChildVisit_Continue;
    if (clang_getCursorKind(cursor) != CXCursor_StructDecl
        || !clang_isCursorDefinition(cursor)
        || toQString(clang_getCursorSpelling(cursor)) != QStringLiteral("MsgHeader"))
        return CXChildVisit_Recurse;

    LayoutResult &result = *static_cast<LayoutResult *>(data);
    result.found = true;
    const CXType type = clang_getCursorType(cursor);
    const long long size = clang_Type_getSizeOf(type);
    const long long alignment = clang_Type_getAlignOf(type);
    if (size < 0 || alignment < 0) {
        qCritical() << "Cannot read MsgHeader layout:" << size << alignment;
        result.valid = false;
        return CXChildVisit_Break;
    }
    qInfo().noquote() << QString("MsgHeader: size=%1 bytes, alignment=%2 bytes")
                            .arg(size).arg(alignment);
    clang_visitChildren(cursor, visitField, data);
    return CXChildVisit_Break;
}

int main(int argc, char *argv[])
{
    // Qt on Windows may otherwise send console messages only to the debugger.
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    QCoreApplication app(argc, argv);
    const QByteArray sourceFile = QFile::encodeName(
        app.arguments().size() > 1 ? app.arguments().at(1) : QStringLiteral(MSG_HEADER_PATH));
    // The driver path lets libclang discover this MSYS2 toolchain's C++ headers.
    const char *arguments[] = {
        LIBCLANG_ROOT "/bin/clang++.exe", "-x", "c++", "-std=c++11",
        "--target=x86_64-w64-windows-gnu"
    };

    CXIndex index = clang_createIndex(0, 0);
    if (!index) {
        qCritical() << "Cannot create libclang index";
        return 1;
    }
    CXTranslationUnit unit = nullptr;
    const CXErrorCode error = clang_parseTranslationUnit2FullArgv(
        index, sourceFile.constData(), arguments,
        static_cast<int>(sizeof(arguments) / sizeof(arguments[0])),
        nullptr, 0, CXTranslationUnit_None, &unit);
    if (error != CXError_Success || !unit) {
        qCritical() << "libclang parse failed:" << error << sourceFile;
        if (unit)
            clang_disposeTranslationUnit(unit);
        clang_disposeIndex(index);
        return 1;
    }

    bool hasErrors = false;
    const unsigned diagnosticCount = clang_getNumDiagnostics(unit);
    for (unsigned i = 0; i < diagnosticCount; ++i) {
        CXDiagnostic diagnostic = clang_getDiagnostic(unit, i);
        hasErrors |= clang_getDiagnosticSeverity(diagnostic) >= CXDiagnostic_Error;
        qWarning().noquote() << toQString(clang_formatDiagnostic(
            diagnostic, clang_defaultDiagnosticDisplayOptions()));
        clang_disposeDiagnostic(diagnostic);
    }

    LayoutResult result;
    if (!hasErrors) {
        qInfo().noquote() << "libclang:" << toQString(clang_getClangVersion());
        qInfo().noquote() << "Source:" << sourceFile;
        clang_visitChildren(clang_getTranslationUnitCursor(unit), visitRecord, &result);
        if (!result.found)
            qCritical() << "MsgHeader definition not found";
    }
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    return !hasErrors && result.found && result.valid ? 0 : 1;
}
