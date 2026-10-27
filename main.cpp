#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <clang-c/Index.h>

// 解析流程：创建 CXIndex → 解析得到 CXTranslationUnit → 遍历 AST → 查询布局。
// CXCursor 代表 AST 节点（如结构体、字段），CXType 代表节点对应的类型。
// Cursor 和 Type 都依赖所属的 TranslationUnit，必须在 unit 释放前使用。

// CXString 由 libclang 管理，需要调用者显式释放。
static QString toQString(CXString value)
{
    // getCString() 只借用指针；先复制到 QString，再 disposeString()，避免悬空指针。
    const QString result = QString::fromUtf8(clang_getCString(value));
    clang_disposeString(value);
    return result;
}

// 通过回调的 CXClientData（void *）传递结果，避免使用全局变量。
struct LayoutResult
{
    bool found = false;
    bool valid = true;
};

// 回调参数依次为：当前节点、父节点、用户数据；此处不使用父节点。
// Continue：下一个同级节点；Recurse：先进入子节点；Break：结束本次遍历。
static CXChildVisitResult visitField(CXCursor cursor, CXCursor, CXClientData data)
{
    // 结构体的子节点可能包含方法等声明；FieldDecl 才是非静态成员字段。
    if (clang_getCursorKind(cursor) != CXCursor_FieldDecl)
        return CXChildVisit_Continue;

    LayoutResult &result = *static_cast<LayoutResult *>(data);
    // 从字段节点取得类型；大小和类型的自然对齐值均以字节为单位。
    const CXType type = clang_getCursorType(cursor);
    const long long size = clang_Type_getSizeOf(type);
    const long long alignment = clang_Type_getAlignOf(type);
    // 字段偏移以 bit 为单位，相对于结构体起点；本例除以 8 得到字节偏移。
    // 如果以后解析位域，要保留 bit 偏移，不能只看整数除法后的字节值。
    const long long offsetBits = clang_Cursor_getOffsetOfField(cursor);
    // 布局查询返回负数表示 CXTypeLayoutError，例如类型不完整，不能当作有效布局。
    if (size < 0 || alignment < 0 || offsetBits < 0) {
        qCritical() << "Cannot read layout for" << toQString(clang_getCursorSpelling(cursor));
        result.valid = false;
        return CXChildVisit_Continue;
    }

    // getCursorSpelling() 取得字段名；getTypeSpelling() 取得类型的文本表示。
    // getCanonicalType() 展开 typedef 等别名，例如 uint16_t 对应 unsigned short。
    // pack(1) 影响字段位置，但 uint32_t 类型本身的自然对齐值仍然为 4。
    qInfo().noquote()
        << QString("  %1: %2 (%3), size=%4 bytes, type alignment=%5 bytes, offset=%6 bits (%7 bytes)")
               .arg(toQString(clang_getCursorSpelling(cursor)))
               .arg(toQString(clang_getTypeSpelling(type)))
               .arg(toQString(clang_getTypeSpelling(clang_getCanonicalType(type))))
               .arg(size).arg(alignment).arg(offsetBits).arg(offsetBits / 8);
    return CXChildVisit_Continue;
}

// 第一层遍历：在主文件的 AST 中查找 MsgHeader 的完整定义。
static CXChildVisitResult visitRecord(CXCursor cursor, CXCursor, CXClientData data)
{
    // AST 也包含 #include 引入的声明；这里跳过 cstdint 等包含文件中的节点。
    if (!clang_Location_isFromMainFile(clang_getCursorLocation(cursor)))
        return CXChildVisit_Continue;
    // StructDecl 表示 struct 声明；isCursorDefinition 排除 struct MsgHeader; 这样的前置声明。
    // 其他主文件节点返回 Recurse，以便继续查找命名空间等节点内部的结构体。
    if (clang_getCursorKind(cursor) != CXCursor_StructDecl
        || !clang_isCursorDefinition(cursor)
        || toQString(clang_getCursorSpelling(cursor)) != QStringLiteral("MsgHeader"))
        return CXChildVisit_Recurse;

    LayoutResult &result = *static_cast<LayoutResult *>(data);
    result.found = true;
    // 对结构体类型查询布局，libclang 会按目标平台和 #pragma pack 计算对齐与填充。
    // 当前大小为 7、对齐为 1；这里没有手动累加字段大小。
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
    // 第二层遍历：以结构体为父节点，查询其各个字段；data 继续传递同一个结果对象。
    clang_visitChildren(cursor, visitField, data);
    // 已处理目标结构体，结束外层遍历。
    return CXChildVisit_Break;
}

int main(int argc, char *argv[])
{
    // 强制 Qt 日志输出到标准错误，避免 Windows 下只发送给调试器。
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    QCoreApplication app(argc, argv);
    // 命令行可指定待解析文件；默认路径 MSG_HEADER_PATH 由 .pro 定义。
    const QByteArray sourceFile = QFile::encodeName(
        app.arguments().size() > 1 ? app.arguments().at(1) : QStringLiteral(MSG_HEADER_PATH));
    // 这些是解析头文件的参数，与编译当前 Qt 程序的选项是两回事。
    // FullArgv 要求首项为编译器路径，以帮助定位 MSYS2 的标准库头文件。
    // libclang 在进程内解析，不会启动 clang++.exe 子进程。
    // -x c++：按 C++ 解析；-std=c++11：语言标准；--target：决定 ABI 和布局的目标平台。
    const char *arguments[] = {
        LIBCLANG_ROOT "/bin/clang++.exe", "-x", "c++", "-std=c++11",
        "--target=x86_64-w64-windows-gnu"
    };

    // CXIndex 是创建翻译单元的上下文，可用于解析多个文件。
    // 第一个 0：不排除预编译头声明；第二个 0：不自动打印诊断，稍后自行处理。
    CXIndex index = clang_createIndex(0, 0);
    if (!index) {
        qCritical() << "Cannot create libclang index";
        return 1;
    }
    CXTranslationUnit unit = nullptr;
    // 翻译单元包含主文件及包含文件的解析结果和 AST。
    // 参数顺序：索引、文件路径、编译参数数组、参数数量、内存文件数组、内存文件数量、选项、输出指针。
    // nullptr, 0 表示没有 CXUnsavedFile（内存中的未保存文件），直接读取磁盘文件。
    // CXTranslationUnit_None 表示不启用额外选项；&unit 接收创建出来的翻译单元。
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

    // 接口返回 CXError_Success 仍可能有语法错误或缺失头文件，必须继续检查诊断。
    // Diagnostic 包含警告、错误及源代码位置；出现 Error/Fatal 就停止后续布局查询。
    bool hasErrors = false;
    const unsigned diagnosticCount = clang_getNumDiagnostics(unit);
    for (unsigned i = 0; i < diagnosticCount; ++i) {
        // 每次取得的 Diagnostic 需 dispose；格式化诊断得到的 CXString 由 toQString() 释放。
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
        // 从 AST 根节点的直接子节点开始回调；是否递归取决于 visitRecord 的返回值。
        // &result 会原样传入回调的 data 参数，在回调中转回 LayoutResult * 即可使用。
        clang_visitChildren(clang_getTranslationUnitCursor(unit), visitRecord, &result);
        if (!result.found)
            qCritical() << "MsgHeader definition not found";
    }
    // 先释放翻译单元，再释放索引。Cursor/Type 无需单独释放，但 unit 释放后不能再用。
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    return !hasErrors && result.found && result.valid ? 0 : 1;
}
