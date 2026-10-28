// 头文件保护：同一次编译中即使多处包含本文件，也只处理一次这些类型和函数声明。
#ifndef HEADERPARSER_H
#define HEADERPARSER_H

#include <QString>
#include <QStringList>
#include <QVector>

// 本文件是各模块共用的数据定义：解析器填写它，界面、JSON 和二进制模块读取它。
// 用普通 Qt 数据保存查询结果，界面无需持有 libclang 的 Cursor 或 Type。
// enum class 表示一组可选分类，决定界面如何编辑字段，以及如何编码、解码。
// 解析器会把 Clang 类型映射到这些分类，例如 unsigned short 归入 UnsignedInteger。
// 使用时写 FieldValueKind::SignedInteger 这样的完整名称。
enum class FieldValueKind
{
    ReadOnly,        // 只读字段，例如指针、引用、数组、const 成员或当前不支持编辑的类型。
    Record,          // 记录类型分组，如嵌套结构体；展开后编辑子字段，分组本身不直接赋值。
    SignedInteger,   // 有符号整数，如 short、int、long long；可输入负数，范围由 valueBits 决定。
    UnsignedInteger, // 无符号整数，如 unsigned short、unsigned int；只能输入非负数。
    Boolean,         // 布尔类型 bool；界面使用下拉框选择 false 或 true。
    Float,           // 单精度浮点类型 float；接受小数或科学计数法，并检查 float 的数值范围。
    Double,          // 双精度浮点类型 double；通常比 float 精度更高、数值范围更大。
    LongDouble       // 扩展浮点类型 long double；具体精度和大小由目标平台及工具链决定。
};

// 一个成员字段的信息。嵌套结构体的成员仍然用 FieldInfo 表示，保存在 children 中。
struct FieldInfo
{
    QString name;                  // 字段名，例如 sequence；未命名位域的名称为空。
    QString typeName;              // 声明中的类型名，例如 uint16_t。
    QString canonicalTypeName;     // 展开类型别名后的名称，例如 unsigned short。
    qint64 size = -1;              // 字段类型的大小，单位字节；负数表示尚无有效布局。
    qint64 alignment = -1;         // 字段类型的自然对齐，单位字节；负数表示尚无有效结果。
    qint64 offsetBits = -1;         // 相对直接所属结构体起点的偏移，单位 bit；负数表示无有效结果。
    qint64 absoluteOffsetBits = -1; // 相对最外层结构体起点的偏移，单位 bit；负数表示无有效结果。
    QVector<FieldInfo> children;   // 按值嵌套的成员列表；基础类型、指针、引用和数组没有子字段。
    FieldValueKind valueKind = FieldValueKind::ReadOnly; // 编辑和编解码采用的类型分类，默认只读。
    int valueBits = 0;             // 整数/bool 的有效位数（bit）；位域使用声明宽度，如 flags:3 对应 3。
    QString readOnlyReason;        // 字段只读或作为分组的原因，用于界面提示；无需说明时为空。
    bool isBitField = false;       // 是否为位域，例如 unsigned flags:3；位域可以只占一个字节中的几位。
    bool isArray = false;          // 是否为数组；数组只读，二进制预览将整个数组区域补 0。
    QString binaryEncodingIssue;   // 此字段的嵌套记录布局限制，如联合体；为空表示此处未记录限制。
};

// 一个完整结构体：总大小包含填充，fields 按声明顺序保存直接成员。
struct StructInfo
{
    QString name;                // 结构体名称，可包含作用域，例如 demo::Packet 或 Outer::Inner。
    qint64 size = -1;            // 整个结构体的大小，单位字节，包含填充；负数表示尚无有效布局。
    qint64 alignment = -1;       // 结构体的对齐要求，单位字节，受 #pragma pack 等设置影响。
    QVector<FieldInfo> fields;   // 按声明顺序保存直接成员；更深层成员保存在各字段的 children 中。
    QString binaryEncodingIssue; // 整体布局的二进制处理限制原因，如继承、虚函数；为空表示未记录限制。
};

// 单个结构体的解析结果。调用方先检查 success，再使用 structure；诊断可能包含警告。
struct ParseResult
{
    bool success = false;    // 当前结构体是否加载成功；是否能赋值、生成二进制仍需单独检查。
    StructInfo structure;    // 成功时可使用的结构定义，包含字段及布局，不保存用户输入值。
    QStringList diagnostics; // 本次处理的错误和警告；存在警告不一定表示失败。
};

// 整个头文件的解析结果；QVector 是可增长的顺序容器，QStringList 是字符串列表。
struct HeaderParseResult
{
    bool success = false;           // 文件级解析成功且发现候选；不保证每个候选的布局都读取成功。
    QVector<ParseResult> structures; // 按声明顺序保存候选；逐项检查 success，名称含命名空间/外层类型限定。
    QStringList diagnostics;        // 文件级错误和警告；单个结构体的布局诊断在对应候选的 diagnostics 中。
};

// 一次解析，自动发现主文件中的 struct 定义，并递归读取按值嵌套的成员。
HeaderParseResult parseHeaderStructures(const QString &filePath);

// 名称为空时自动选择唯一结构体；有多个时返回候选名称，避免猜测调用方的目标。
// 可传完整限定名，或没有歧义的短名称，兼容原有按名称解析的调用。
ParseResult parseHeader(const QString &filePath,
                        const QString &structName = QString());

#endif
