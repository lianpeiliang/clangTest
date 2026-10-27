#include "headerparser.h"

#include <QFileInfo>
#include <clang-c/Index.h>

// 解析流程：创建 CXIndex → 解析得到 CXTranslationUnit → 遍历 AST → 查询布局。
// CXCursor 是 AST 节点，CXType 是节点对应的类型，两者都依赖所属的翻译单元。
// 字符串和布局数值复制到 ParseResult 后，就可以安全地释放翻译单元。
namespace {

QString toQString(CXString value)
{
    // getCString() 只借用指针；先复制，再释放需要调用者管理的 CXString。
    const QString text = QString::fromUtf8(clang_getCString(value));
    clang_disposeString(value);
    return text;
}

struct VisitorContext
{
    QString targetName;
    ParseResult result;
    bool found = false;
    bool valid = true;
};

// 回调参数依次为当前节点、父节点、用户数据；这里不使用父节点。
CXChildVisitResult visitField(CXCursor cursor, CXCursor, CXClientData data)
{
    // FieldDecl 是非静态成员字段，跳过方法、类型别名等其他子节点。
    if (clang_getCursorKind(cursor) != CXCursor_FieldDecl)
        return CXChildVisit_Continue;

    VisitorContext &context = *static_cast<VisitorContext *>(data);
    const CXType type = clang_getCursorType(cursor);
    FieldInfo field;
    field.name = toQString(clang_getCursorSpelling(cursor));
    field.typeName = toQString(clang_getTypeSpelling(type));
    // CanonicalType 展开 typedef 等类型别名，例如 uint16_t 对应 unsigned short。
    field.canonicalTypeName = toQString(clang_getTypeSpelling(clang_getCanonicalType(type)));
    field.size = clang_Type_getSizeOf(type);
    field.alignment = clang_Type_getAlignOf(type);
    // 大小和自然对齐以字节为单位；字段相对于结构体起点的偏移以 bit 为单位。
    // pack(1) 影响字段位置，但 uint32_t 类型本身的自然对齐仍然是 4。
    field.offsetBits = clang_Cursor_getOffsetOfField(cursor);
    // 负数表示 CXTypeLayoutError，例如不完整类型，不能当作有效布局。
    if (field.size < 0 || field.alignment < 0 || field.offsetBits < 0) {
        context.valid = false;
        context.result.diagnostics.append(QStringLiteral("无法读取字段 %1 的内存布局。")
                                              .arg(field.name));
        return CXChildVisit_Continue;
    }
    context.result.structure.fields.append(field);
    return CXChildVisit_Continue;
}

CXChildVisitResult visitRecord(CXCursor cursor, CXCursor, CXClientData data)
{
    // TranslationUnit 也包含被 #include 引入的声明，只查找主文件中的定义。
    if (!clang_Location_isFromMainFile(clang_getCursorLocation(cursor)))
        return CXChildVisit_Continue;

    VisitorContext &context = *static_cast<VisitorContext *>(data);
    // 排除 struct MsgHeader; 这样的前置声明；其他节点继续递归，以支持命名空间。
    if (clang_getCursorKind(cursor) != CXCursor_StructDecl
        || !clang_isCursorDefinition(cursor)
        || toQString(clang_getCursorSpelling(cursor)) != context.targetName)
        return CXChildVisit_Recurse;

    context.found = true;
    StructInfo &structure = context.result.structure;
    structure.name = toQString(clang_getCursorSpelling(cursor));
    const CXType type = clang_getCursorType(cursor);
    // libclang 按目标平台和 #pragma pack 计算对齐与填充，无需手动累加字段大小。
    structure.size = clang_Type_getSizeOf(type);
    structure.alignment = clang_Type_getAlignOf(type);
    if (structure.size < 0 || structure.alignment < 0) {
        context.valid = false;
        context.result.diagnostics.append(QStringLiteral("无法读取结构体 %1 的内存布局。")
                                              .arg(structure.name));
        return CXChildVisit_Break;
    }
    // 从结构体的直接子节点读取各个字段，同一个 context 会传入每次字段回调。
    clang_visitChildren(cursor, visitField, data);
    return CXChildVisit_Break;
}

} // namespace

ParseResult parseHeader(const QString &filePath, const QString &structName)
{
    VisitorContext context;
    context.targetName = structName;
    const QFileInfo sourceInfo(filePath);
    if (filePath.trimmed().isEmpty() || !sourceInfo.isFile() || !sourceInfo.isReadable()) {
        context.result.diagnostics.append(QStringLiteral("头文件不存在或无法读取：%1").arg(filePath));
        return context.result;
    }

    const QByteArray sourceFile = sourceInfo.absoluteFilePath().toUtf8();
    // 这些参数用于解析头文件，与编译当前 Qt 程序的参数是两回事。
    // FullArgv 的首项为编译器路径，帮助寻找 MSYS2 标准库头文件，不会启动编译器子进程。
    // -x c++ 指定语言；-std 指定标准；--target 决定 ABI 和内存布局的目标平台。
    const char *arguments[] = {
        LIBCLANG_ROOT "/bin/clang++.exe", "-x", "c++", "-std=c++11",
        "--target=x86_64-w64-windows-gnu"
    };

    // 第一个 0：不排除预编译头声明；第二个 0：不自动打印诊断，稍后自行收集。
    CXIndex index = clang_createIndex(0, 0);
    if (!index) {
        context.result.diagnostics.append(QStringLiteral("无法创建 libclang 索引。"));
        return context.result;
    }
    CXTranslationUnit unit = nullptr;
    // 参数顺序：索引、文件、编译参数、参数数量、内存文件、内存文件数量、选项、输出指针。
    // nullptr, 0 表示没有 CXUnsavedFile，读取磁盘；None 表示不启用额外选项。
    const CXErrorCode error = clang_parseTranslationUnit2FullArgv(
        index, sourceFile.constData(), arguments,
        static_cast<int>(sizeof(arguments) / sizeof(arguments[0])),
        nullptr, 0, CXTranslationUnit_None, &unit);
    if (error != CXError_Success || !unit) {
        context.result.diagnostics.append(QStringLiteral("libclang 解析失败，错误码：%1").arg(int(error)));
        if (unit)
            clang_disposeTranslationUnit(unit);
        clang_disposeIndex(index);
        return context.result;
    }

    // 接口调用成功仍可能有语法错误，必须检查诊断；Error/Fatal 阻止后续布局查询。
    bool hasErrors = false;
    const unsigned diagnosticCount = clang_getNumDiagnostics(unit);
    for (unsigned i = 0; i < diagnosticCount; ++i) {
        CXDiagnostic diagnostic = clang_getDiagnostic(unit, i);
        hasErrors |= clang_getDiagnosticSeverity(diagnostic) >= CXDiagnostic_Error;
        context.result.diagnostics.append(toQString(clang_formatDiagnostic(
            diagnostic, clang_defaultDiagnosticDisplayOptions())));
        clang_disposeDiagnostic(diagnostic);
    }
    if (!hasErrors) {
        // 从 AST 根节点开始遍历；Continue 跳过子节点，Recurse 进入子节点，Break 结束遍历。
        clang_visitChildren(clang_getTranslationUnitCursor(unit), visitRecord, &context);
        if (!context.found)
            context.result.diagnostics.append(QStringLiteral("未在主文件中找到结构体 %1 的定义。")
                                                  .arg(structName));
    }
    context.result.success = !hasErrors && context.found && context.valid;
    // 先释放翻译单元再释放索引。Cursor/Type 不需单独释放，此后界面只用复制好的结果。
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    return context.result;
}
