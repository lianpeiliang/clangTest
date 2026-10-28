#include "headerparser.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QVersionNumber>
#include <clang-c/Index.h>

// 本模块负责把 C++ 头文件转换为 headerparser.h 中的普通数据，不负责绘制界面。
// AST（抽象语法树）是编译器理解代码后得到的树，结构体、字段等分别对应树上的节点。
// 解析流程：创建 CXIndex → 解析得到 CXTranslationUnit → 遍历 AST → 查询布局。
// CXCursor 是 AST 节点，CXType 是节点对应的类型，两者都依赖所属的翻译单元。
// 字符串和布局数值复制到 ParseResult 后，就可以安全地释放翻译单元。
// 匿名命名空间中的辅助函数只在当前 .cpp 内使用，对外接口放在命名空间外。
namespace {

// 标准库和 Clang 内置头文件按版本号放在子目录中，选择其中最新的版本。
QString versionDirectory(const QString &parentPath)
{
    QString selected;
    QVersionNumber bestVersion;
    const QDir parent(parentPath);
    for (const QString &name : parent.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QVersionNumber version = QVersionNumber::fromString(name);
        if (!version.isNull() && (selected.isEmpty() || version > bestVersion)) {
            selected = parent.filePath(name);
            bestVersion = version;
        }
    }
    return selected;
}

QString toQString(CXString value)
{
    // getCString() 只借用指针；先复制，再释放需要调用者管理的 CXString。
    const QString text = QString::fromUtf8(clang_getCString(value));
    clang_disposeString(value);
    return text;
}

// 根据编译器返回的类型分类，决定字段用哪种输入方式，以及无法编辑时显示什么原因。
void describeValueType(FieldInfo &field, CXType type, CXCursor cursor, bool inheritedConst)
{
    const CXType canonical = clang_getCanonicalType(type);
    field.isArray = canonical.kind == CXType_ConstantArray || canonical.kind == CXType_IncompleteArray
        || canonical.kind == CXType_VariableArray || canonical.kind == CXType_DependentSizedArray;
    if (field.isArray) {
        field.readOnlyReason = QStringLiteral("数组只读，二进制预览按数组实际大小全部填 0。");
        return;
    }
    field.valueBits = clang_Cursor_isBitField(cursor)
        ? clang_getFieldDeclBitWidth(cursor) : int(field.size * 8);
    if (inheritedConst || clang_isConstQualifiedType(canonical)) {
        field.readOnlyReason = QStringLiteral("const 字段及其子成员暂不支持赋值。");
        return;
    }
    if (field.name.isEmpty()) {
        field.readOnlyReason = QStringLiteral("匿名字段暂不支持赋值。");
        return;
    }
    switch (canonical.kind) {
    case CXType_Bool:
        field.valueKind = FieldValueKind::Boolean;
        break;
    case CXType_Char_S: case CXType_SChar: case CXType_Short:
    case CXType_Int: case CXType_Long: case CXType_LongLong:
        field.valueKind = FieldValueKind::SignedInteger;
        break;
    case CXType_Char_U: case CXType_UChar: case CXType_Char16: case CXType_Char32:
    case CXType_UShort: case CXType_UInt: case CXType_ULong: case CXType_ULongLong:
    case CXType_WChar: // 固定的 Windows x64 / MinGW 目标使用无符号 wchar_t。
        field.valueKind = FieldValueKind::UnsignedInteger;
        break;
    case CXType_Float: field.valueKind = FieldValueKind::Float; break;
    case CXType_Double: field.valueKind = FieldValueKind::Double; break;
    case CXType_LongDouble: field.valueKind = FieldValueKind::LongDouble; break;
    case CXType_Record:
        field.valueKind = FieldValueKind::Record;
        field.readOnlyReason = QStringLiteral("结构体作为分组，请编辑展开后的基础类型成员。");
        break;
    case CXType_Pointer:
        field.readOnlyReason = QStringLiteral("指针暂不支持赋值。");
        break;
    case CXType_LValueReference: case CXType_RValueReference:
        field.readOnlyReason = QStringLiteral("引用暂不支持赋值。");
        break;
    default:
        field.readOnlyReason = QStringLiteral("此类型暂不支持赋值。");
        break;
    }
    if ((field.valueKind == FieldValueKind::SignedInteger
         || field.valueKind == FieldValueKind::UnsignedInteger)
        && (field.valueBits <= 0 || field.valueBits > 64)) {
        field.valueKind = FieldValueKind::ReadOnly;
        field.readOnlyReason = QStringLiteral("当前仅支持 1～64 位整数赋值。");
    }
}

// “可以显示布局”不一定等于“可以生成二进制”；这里记录当前编码器不支持的布局。
CXChildVisitResult checkRecordEncoding(CXCursor cursor, CXCursor, CXClientData data)
{
    QString &issue = *static_cast<QString *>(data);
    const CXCursorKind kind = clang_getCursorKind(cursor);
    if (kind == CXCursor_CXXBaseSpecifier)
        issue = QStringLiteral("含继承布局，暂不支持二进制编码。");
    else if ((kind == CXCursor_CXXMethod || kind == CXCursor_Destructor)
             && clang_CXXMethod_isVirtual(cursor))
        issue = QStringLiteral("含虚函数布局，暂不支持二进制编码。");
    else if ((kind == CXCursor_StructDecl || kind == CXCursor_UnionDecl)
             && clang_Cursor_isAnonymousRecordDecl(cursor))
        issue = QStringLiteral("含匿名结构体或联合体，暂不支持二进制编码。");
    return issue.isEmpty() ? CXChildVisit_Continue : CXChildVisit_Break;
}

QString recordEncodingIssue(CXCursor cursor)
{
    if (clang_getCursorKind(cursor) == CXCursor_UnionDecl)
        return QStringLiteral("联合体尚未指定有效成员，暂不支持二进制编码。");
    QString issue;
    clang_visitChildren(cursor, checkRecordEncoding, &issue);
    return issue;
}

// libclang 的遍历回调通过 void* 携带用户数据；context 用来累计结果和错误。
struct VisitorContext
{
    ParseResult result;
    bool valid = true;
};

struct FieldVisitorContext
{
    VisitorContext &parser;
    QVector<FieldInfo> &fields;
    qint64 baseOffsetBits; // 当前所属结构体相对最外层结构体的起点，单位 bit。
    QString path;         // 当前字段路径，用于生成可定位的诊断信息。
    bool inheritedConst;  // 外层成员为 const 时，内部成员也按只读处理。
};

// 回调参数依次为当前节点、父节点、用户数据；这里不使用父节点。
CXChildVisitResult visitField(CXCursor cursor, CXCursor, CXClientData data)
{
    // FieldDecl 是非静态成员字段，跳过方法、类型别名等其他子节点。
    if (clang_getCursorKind(cursor) != CXCursor_FieldDecl)
        return CXChildVisit_Continue;

    FieldVisitorContext &context = *static_cast<FieldVisitorContext *>(data);
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
    const QString fieldPath = context.path + QLatin1Char('.') + field.name;
    // 负数表示 CXTypeLayoutError，例如不完整类型，不能当作有效布局。
    if (field.size < 0 || field.alignment < 0 || field.offsetBits < 0) {
        context.parser.valid = false;
        context.parser.result.diagnostics.append(QStringLiteral("无法读取字段 %1 的内存布局。")
                                                     .arg(fieldPath));
        return CXChildVisit_Continue;
    }
    field.absoluteOffsetBits = context.baseOffsetBits + field.offsetBits;
    field.isBitField = clang_Cursor_isBitField(cursor);
    describeValueType(field, type, cursor, context.inheritedConst);

    // 通过规范类型找到定义，可展开 typedef/using 后的记录类型以及其他头文件中的定义。
    // 只递归按值存放的记录类型，指针/引用不会被追踪，自引用指针也不会造成递归。
    const CXType canonicalType = clang_getCanonicalType(type);
    if (canonicalType.kind == CXType_Record) {
        const CXCursor declaration = clang_getTypeDeclaration(canonicalType);
        const CXCursor definition = clang_getCursorDefinition(declaration);
        if (!clang_Cursor_isNull(definition)) {
            field.binaryEncodingIssue = recordEncodingIssue(definition);
            FieldVisitorContext childContext{
                context.parser, field.children, field.absoluteOffsetBits, fieldPath,
                context.inheritedConst || bool(clang_isConstQualifiedType(canonicalType))
            };
            clang_visitChildren(definition, visitField, &childContext);
        }
    }
    context.fields.append(field);
    return CXChildVisit_Continue;
}

// 临时保存 AST 节点供本次解析使用；释放翻译单元前必须把它转换成 StructInfo。
struct RecordCandidate
{
    CXCursor cursor;
    QString name;
};

// 沿语义上的父作用域向上查找，把短名 Packet 拼成 demo::Packet 或 Outer::Inner。
QString qualifiedName(CXCursor cursor)
{
    QStringList parts;
    parts.prepend(toQString(clang_getCursorSpelling(cursor)));
    for (CXCursor parent = clang_getCursorSemanticParent(cursor);
         !clang_Cursor_isNull(parent) && clang_getCursorKind(parent) != CXCursor_TranslationUnit;
         parent = clang_getCursorSemanticParent(parent)) {
        QString name = toQString(clang_getCursorSpelling(parent));
        if (name.isEmpty() && clang_getCursorKind(parent) == CXCursor_Namespace)
            name = QStringLiteral("(anonymous namespace)");
        if (!name.isEmpty())
            parts.prepend(name);
    }
    return parts.join(QStringLiteral("::"));
}

// 第一轮遍历只收集“有哪些结构体”，不在这里读取成员；第二轮逐个读取布局。
CXChildVisitResult discoverRecords(CXCursor cursor, CXCursor, CXClientData data)
{
    // TranslationUnit 也包含被 #include 引入的声明，只查找主文件中的定义。
    if (!clang_Location_isFromMainFile(clang_getCursorLocation(cursor)))
        return CXChildVisit_Continue;

    // 将传入的 void* 还原为候选列表指针，再取引用，后续追加会直接更新调用方的列表。
    auto &candidates = *static_cast<QVector<RecordCandidate> *>(data);
    const CXCursorKind kind = clang_getCursorKind(cursor);
    CXCursor definition = cursor;
    QString name;
    if (kind == CXCursor_StructDecl && clang_isCursorDefinition(cursor)
        && !clang_Cursor_isAnonymous(cursor)
        && !toQString(clang_getCursorSpelling(cursor)).isEmpty()) {
        name = qualifiedName(cursor);
    } else if (kind == CXCursor_TypedefDecl || kind == CXCursor_TypeAliasDecl) {
        const CXType type = clang_getCanonicalType(clang_getCursorType(cursor));
        definition = clang_getCursorDefinition(clang_getTypeDeclaration(type));
        // typedef struct { ... } Packet; 也能作为独立结构体；普通别名不重复列出。
        if (type.kind == CXType_Record && clang_getCursorKind(definition) == CXCursor_StructDecl
            && clang_Location_isFromMainFile(clang_getCursorLocation(definition))
            && (clang_Cursor_isAnonymous(definition)
                || toQString(clang_getCursorSpelling(definition)).isEmpty()))
            name = qualifiedName(cursor);
    }
    if (!name.isEmpty()) {
        // 同一个定义可能经由别名再次访问，用节点身份去重，不只比较显示名称。
        bool duplicate = false;
        for (const RecordCandidate &candidate : candidates)
            duplicate |= bool(clang_equalCursors(candidate.cursor, definition));
        if (!duplicate)
            candidates.append(RecordCandidate{definition, name});
    }
    // 进入命名空间和记录作用域，跳过函数局部类型及尚未实例化的模板。
    return kind == CXCursor_Namespace || kind == CXCursor_LinkageSpec
        || kind == CXCursor_UnexposedDecl || kind == CXCursor_StructDecl
        || kind == CXCursor_ClassDecl || kind == CXCursor_UnionDecl
        || kind == CXCursor_TypedefDecl || kind == CXCursor_TypeAliasDecl
        ? CXChildVisit_Recurse : CXChildVisit_Continue;
}

// 每个结构体独立保存成功状态；一个候选的布局读取失败，不覆盖其他候选的结果。
ParseResult parseRecord(const RecordCandidate &candidate)
{
    VisitorContext context;
    const CXCursor cursor = candidate.cursor;
    StructInfo &structure = context.result.structure;
    structure.name = candidate.name;
    structure.binaryEncodingIssue = recordEncodingIssue(cursor);
    const CXType type = clang_getCursorType(cursor);
    // libclang 按目标平台和 #pragma pack 计算对齐与填充，无需手动累加字段大小。
    structure.size = clang_Type_getSizeOf(type);
    structure.alignment = clang_Type_getAlignOf(type);
    if (structure.size < 0 || structure.alignment < 0) {
        context.valid = false;
        context.result.diagnostics.append(QStringLiteral("无法读取结构体 %1 的内存布局。")
                                              .arg(structure.name));
        return context.result;
    }
    // 每一层传入自己的字段容器和累计偏移，保存完整的成员树。
    FieldVisitorContext fieldContext{context, structure.fields, 0, structure.name, false};
    clang_visitChildren(cursor, visitField, &fieldContext);
    context.result.success = context.valid;
    return context.result;
}

} // namespace

// 自动发现的对外入口：每次重新读取磁盘文件，一次解析取得所有候选。
HeaderParseResult parseHeaderStructures(const QString &filePath)
{
    HeaderParseResult result;
    const QFileInfo sourceInfo(filePath);
    if (filePath.trimmed().isEmpty() || !sourceInfo.isFile() || !sourceInfo.isReadable()) {
        result.diagnostics.append(QStringLiteral("头文件不存在或无法读取：%1").arg(filePath));
        return result;
    }

    const QByteArray sourceFile = sourceInfo.absoluteFilePath().toUtf8();
    // 这些参数用于解析头文件，与编译当前 Qt 程序的参数是两回事。
    // FullArgv 的首项为编译器路径，不会启动编译器子进程。
    // -x c++ 指定语言；-std 指定标准；--target 决定 ABI 和内存布局的目标平台。
    QList<QByteArray> arguments = {
        LIBCLANG_ROOT "/bin/clang++.exe", "-x", "c++", "-std=c++11",
        "--target=x86_64-w64-windows-gnu"
    };
    const QString runtimePath = QDir(QCoreApplication::applicationDirPath())
                                    .filePath(QStringLiteral("runtime"));
    if (QDir(runtimePath).exists()) {
        // 便携包显式使用随包头文件，禁止回退到本机 Qt/Clang/MSYS2 的搜索路径。
        const QString cxxPath = versionDirectory(runtimePath + QStringLiteral("/include/c++"));
        const QString resourcePath = versionDirectory(runtimePath + QStringLiteral("/lib/clang"));
        if (cxxPath.isEmpty() || resourcePath.isEmpty()) {
            result.diagnostics.append(QStringLiteral("随包标准库头文件不完整，请完整解压程序压缩包。"));
            return result;
        }
        // argv[0] 仅供 libclang 识别工具链位置，便携包不需要真正附带 clang++.exe。
        arguments[0] = (runtimePath + QStringLiteral("/bin/clang++.exe")).toUtf8();
        arguments.append("-nostdinc");
        arguments.append((QStringLiteral("-resource-dir=") + resourcePath).toUtf8());
        // C++ 头文件必须在 C 头文件之前，保证标准库的 #include_next 顺序正确。
        const QStringList includePaths = {
            cxxPath, cxxPath + QStringLiteral("/x86_64-w64-mingw32"),
            cxxPath + QStringLiteral("/backward"), resourcePath + QStringLiteral("/include"),
            runtimePath + QStringLiteral("/include")
        };
        for (const QString &includePath : includePaths) {
            arguments.append("-isystem");
            arguments.append(includePath.toUtf8());
        }
    }
    // QByteArray 保存参数文本，指针数组只在解析调用期间借用这些文本。
    QVector<const char *> argumentPointers;
    for (const QByteArray &argument : arguments)
        argumentPointers.append(argument.constData());

    // 第一个 0：不排除预编译头声明；第二个 0：不自动打印诊断，稍后自行收集。
    CXIndex index = clang_createIndex(0, 0);
    if (!index) {
        result.diagnostics.append(QStringLiteral("无法创建 libclang 索引。"));
        return result;
    }
    CXTranslationUnit unit = nullptr;
    // 参数顺序：索引、文件、编译参数、参数数量、内存文件、内存文件数量、选项、输出指针。
    // nullptr, 0 表示没有 CXUnsavedFile，读取磁盘；None 表示不启用额外选项。
    const CXErrorCode error = clang_parseTranslationUnit2FullArgv(
        index, sourceFile.constData(), argumentPointers.constData(), argumentPointers.size(),
        nullptr, 0, CXTranslationUnit_None, &unit);
    if (error != CXError_Success || !unit) {
        result.diagnostics.append(QStringLiteral("libclang 解析失败，错误码：%1").arg(int(error)));
        if (unit)
            clang_disposeTranslationUnit(unit);
        clang_disposeIndex(index);
        return result;
    }

    // 接口调用成功仍可能有语法错误，必须检查诊断；Error/Fatal 阻止后续布局查询。
    bool hasErrors = false;
    const unsigned diagnosticCount = clang_getNumDiagnostics(unit);
    for (unsigned i = 0; i < diagnosticCount; ++i) {
        CXDiagnostic diagnostic = clang_getDiagnostic(unit, i);
        hasErrors |= clang_getDiagnosticSeverity(diagnostic) >= CXDiagnostic_Error;
        result.diagnostics.append(toQString(clang_formatDiagnostic(
            diagnostic, clang_defaultDiagnosticDisplayOptions())));
        clang_disposeDiagnostic(diagnostic);
    }
    if (!hasErrors) {
        // 从 AST 根节点开始遍历；Continue 跳过子节点，Recurse 进入子节点，Break 结束遍历。
        QVector<RecordCandidate> candidates;
        clang_visitChildren(clang_getTranslationUnitCursor(unit), discoverRecords, &candidates);
        for (const RecordCandidate &candidate : candidates)
            result.structures.append(parseRecord(candidate));
        if (candidates.isEmpty())
            result.diagnostics.append(QStringLiteral("未在主文件中找到可选择的 struct 定义（前置声明、无名称的匿名成员和未实例化模板不列出）。"));
    }
    result.success = !hasErrors && !result.structures.isEmpty();
    // 先释放翻译单元再释放索引。Cursor/Type 不需单独释放，此后界面只用复制好的结果。
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    return result;
}

// 便于只需要一个结构体的调用方使用：先取得全部候选，再处理唯一选择或名称匹配。
ParseResult parseHeader(const QString &filePath, const QString &structName)
{
    const HeaderParseResult header = parseHeaderStructures(filePath);
    ParseResult result;
    result.diagnostics = header.diagnostics;
    if (!header.success)
        return result;
    const QString requested = structName.trimmed();
    QStringList names;
    QVector<int> matches;
    for (int i = 0; i < header.structures.size(); ++i) {
        const QString name = header.structures.at(i).structure.name;
        names.append(name);
        if (name == requested) {
            matches = {i};
            break;
        }
        if (name.section(QStringLiteral("::"), -1) == requested)
            matches.append(i);
    }
    if (requested.isEmpty() && header.structures.size() == 1)
        matches = {0};
    if (matches.size() == 1) {
        result = header.structures.at(matches.first());
        result.diagnostics = header.diagnostics + result.diagnostics;
    } else if (requested.isEmpty() || matches.size() > 1) {
        result.diagnostics.append(QStringLiteral("发现多个结构体，请选择完整名称：%1。")
                                  .arg(names.join(QStringLiteral("、"))));
    } else {
        result.diagnostics.append(QStringLiteral("未在主文件中找到结构体 %1 的定义。可选结构体：%2。")
                                  .arg(requested, names.join(QStringLiteral("、"))));
    }
    return result;
}
