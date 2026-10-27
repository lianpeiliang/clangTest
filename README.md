# clangTest

一个使用 qmake（`.pro`）构建的 Qt Widgets 示例，通过 libclang 解析 C++ 头文件，在窗口中显示指定结构体的成员树和内存布局，并支持基础类型字段赋值，默认解析 `MsgHeader`。

## 项目文件

| 文件 | 用途 |
| --- | --- |
| `clangTest.pro` | Qt 项目配置及 libclang 链接配置 |
| `main.cpp` | 创建 QApplication，启动窗口 |
| `mainwindow.h / .cpp` | 使用 QTreeWidget 显示结构体成员树及解析诊断 |
| `headerparser.h / .cpp` | 调用 libclang，返回结构体和字段的查询结果 |
| `fieldvalue.h / .cpp` | 根据字段类型、位宽校验输入，保留完整的 64 位整数文本 |
| `binaryencoder.h / .cpp` | 按目标布局编码当前字段值，并格式化十六进制预览 |
| `MsgHeader.h` | 待解析的结构体定义，使用 `#pragma pack(push, 1)` |
| `SimpleMsgHeader.h` | 只有 3 个基础类型字段的简单对比样例，同样使用 1 字节打包 |
| `MinimalMsgHeader.h` | 只有 2 个字段的最小样例，使用默认对齐 |
| `PacketHeader.h` | 不同名称的样例，定义 `PacketHeader`，包含 3 个字段，使用 1 字节打包 |
| `NestedPacketHeader.h` | 嵌套样例，`NestedPacketHeader` 包含 `PacketAddress` 类型的成员，使用 1 字节打包 |
| `tests/nested-tests.pro` | 嵌套解析、偏移累计和树形界面的回归检查 |
| `tests/value-tests.pro` | 字段赋值、数值范围、只读类型和内存保存的回归检查 |
| `tests/binary-tests.pro` | 二进制与原生 C++ 内存布局比对，以及十六进制预览检查 |

`build/` 和 `*.pro.user` 已加入 Git 忽略规则。

## 环境

已验证的 Windows 64 位环境：

- Qt 5.12.8 MinGW 64-bit。
- MSYS2 MINGW64，已安装 Clang 23.1.4 和 libclang。

下文使用以下安装路径；如果你的路径不同，请相应调整：

| 组件 | 路径 |
| --- | --- |
| Qt | `D:/Qt/Qt5.12.8` |
| MSYS2 MINGW64 | `D:/msys64/mingw64` |

`.pro` 中的 `LIBCLANG_ROOT` 默认指向上述 MINGW64 目录。Qt、编译器和 libclang 均需使用 64 位版本。

## 在 Qt Creator 中运行

1. 打开 `clangTest.pro`，选择 **Desktop Qt 5.12.8 MinGW 64-bit** Kit。
2. 进入 **项目 → Run → Run Environment → 详情 → 批量编辑**，添加：

   ```text
   PATH=D:\msys64\mingw64\bin;${PATH}
   ```

3. 取消勾选 **Run in terminal**，直接显示程序窗口。
4. 按 `Ctrl+R` 构建并运行。

MSYS2 的 `bin` 目录需要位于原 PATH 前面，使 libclang、LLVM 和新版 C++ 运行库使用同一套依赖。该配置仅作用于本项目的运行环境。

## 窗口操作

- 启动时默认解析 `MsgHeader.h`，上方显示结构体名称、字段数量、总大小和结构体对齐。
- 树形控件显示字段名称、声明类型、展开别名后的类型、大小、自然对齐及偏移。根节点为所选结构体，成员按层级显示，可点击箭头展开、折叠，或使用 **全部展开 / 全部折叠**。
- 同时显示相对直接所属结构体和相对最外层结构体的偏移，均提供字节和 bit 两种表示；位域的字节偏移保留不足一个字节的 bit 部分。
- 在 **值** 列直接编辑基础类型字段；布尔值使用下拉框。嵌套结构体作为分组，展开后可分别填写内部字段。
- 点击 **查看二进制**，在弹窗中查看当前值对应的十六进制快照，也可点击 **复制十六进制**。
- 修改并保存头文件后，点击 **重新解析** 更新结果。
- 点击 **打开头文件…** 选择其他文件，也可直接修改路径并按回车。
- **结构体名称** 输入框默认填入 `MsgHeader`，可修改为其他名称，按回车或点击 **重新解析** 生效。程序按名称精确查找主文件中的 struct 定义，不查找 `#include` 引入的结构体；名称不能为空，输入两端的空白会被忽略。
- 解析失败时清空旧树，并在下方显示错误原因；修正文件后可再次解析。

libclang 的查询结果先复制到 `StructInfo` 和 `FieldInfo`，再交给窗口显示。解析资源释放后，界面仍可安全使用这些普通 Qt 数据。

对比测试时，用 **打开头文件…** 分别选择 `MsgHeader.h`（21 个字段、83 字节）、`SimpleMsgHeader.h`（3 个字段、7 字节）或 `MinimalMsgHeader.h`（2 个字段、4 字节）。最小样例使用默认对齐，两个字段偏移为 0、2 字节，可观察到中间的填充。各文件都定义名为 `MsgHeader` 的结构体，应分别解析，不要在同一个源文件中同时包含这些头文件。

验证其他结构体名称时，打开 `PacketHeader.h`，将 **结构体名称** 改为 `PacketHeader`，按回车或点击 **重新解析**。预期显示 `sequence`、`payloadLength`、`flags` 三个字段，偏移分别为 0、4、6 字节，总大小 7 字节，对齐 1 字节。

验证嵌套结构体时，打开 `NestedPacketHeader.h`，将 **结构体名称** 设为 `NestedPacketHeader`。预期显示 `sequence`、`address`、`payloadLength` 三个直接字段，偏移分别为 0、4、8 字节，总大小 10 字节，对齐 1 字节。展开 `address` 可看到 `source`、`destination`：相对偏移分别为 0、2 字节，绝对偏移分别为 4、6 字节。也可将名称改为 `PacketAddress` 单独解析。

按值嵌套的记录类型会递归展开，包括经过 `typedef` / `using` 别名的类型；嵌套类型可定义在被包含的头文件中。指针、引用和数组保持为叶子节点，不追踪其指向对象或展开数组元素。顶部字段数量仅统计所选结构体的直接字段。

## 字段赋值

- 整数支持十进制及带 `0x` / `0X` 前缀的十六进制，例如 `42`、`0x2A`，有符号负数可写成 `-0x10`。根据类型位宽和位域宽度校验范围：`unsigned short` 接受 0～65535（`0xFFFF`），3 位无符号位域接受 0～7（`0x7`）。64 位整数以文本保存，避免浮点转换丢失精度。
- 十六进制按数值解释，不自动按补码解释负数。例如 32 位 `int` 接受 `0x7FFFFFFF`、`-0x80000000`，拒绝超出正数范围的 `0xFFFFFFFF`。无符号类型不接受负数。无前缀的 `010` 仍表示十进制 10，`FF` 等省略前缀的写法不接受。
- `char`、`wchar_t`、`char16_t`、`char32_t` 等字符类型按十进制或 `0x` 前缀十六进制数值编码填写。
- `bool` 通过下拉框选择 `false` 或 `true`。
- `float`、`double`、`long double` 接受小数及科学计数法，例如 `-1.25`、`1e-3`；使用小数点，拒绝非法格式、NaN、无穷大、溢出和非零数值下溢至 0。
- 指针、引用、数组、枚举、const 字段及其子成员等暂时只读，悬停可查看原因。结构体分组本身不能赋值。

输入无效时编辑框标红，上方显示字段路径和错误原因，内存中保留上一次有效值；修正后自动更新。值按 `address.source` 这样的完整路径单独保存，同名嵌套字段互不覆盖。

合法输入的进制写法会在重新解析时保留。二进制预览按数值编码，例如 `0x1234` 与十进制 `4660` 生成相同的字节。

数值初始为 `0`，布尔值初始为 `false`，暂不读取头文件中的成员初始化表达式。重新解析同一文件、同一结构体时保留仍符合类型要求的值；字段范围变化导致旧值无效时恢复初始值。切换文件、切换结构体或解析失败时清空之前的值。本版值仅保存在当前窗口内存中，关闭后不保留，不修改头文件。

## 十六进制预览

编辑完成后点击 **查看二进制**。程序按 libclang 返回的字段绝对偏移和结构体总大小编码，采用当前固定目标 Windows x64 / MinGW 的小端序；结构体填充字节、未命名位域和 `long double` 的填充部分统一补 0。支持嵌套结构体、整数、字符编码、bool、位域以及 float / double / long double。

例如 `NestedPacketHeader` 的 `sequence = 305419896`、`address.source = 4386`、`address.destination = 13124`、`payloadLength = 21862`，会显示：

```text
00000000  78 56 34 12 22 11 44 33 66 55
```

左侧是十六进制字节偏移，每行最多 16 字节；复制按钮只复制字节，不包含偏移。预览是点击时的快照，修改值后再次点击按钮可生成新快照。

没有解析结果或存在无效输入时，按钮不可用。包含指针、引用、数组、枚举、const 等尚无可编码值的字段，或存在联合体、继承、虚函数、匿名结构体/联合体等暂不支持的布局时，会提示具体原因并停止生成；不会把这些字段当作 0 编码。单个结构体预览大小上限为 1 MiB。

## 在 PowerShell 中构建

在项目根目录执行。此方式使用 MSYS2 Clang 编译项目：

```powershell
$qtRoot = "D:/Qt/Qt5.12.8"
$clangRoot = "D:/msys64/mingw64"
$qtBin = "$qtRoot/5.12.8/mingw73_64/bin"
$env:PATH = "$clangRoot/bin;$qtBin;" + $env:PATH

New-Item -ItemType Directory -Path build -Force | Out-Null
Set-Location build
& "$qtBin/qmake.exe" ../clangTest.pro -spec win32-clang-g++ "LIBCLANG_ROOT=$clangRoot" "CONFIG+=release" "CONFIG-=debug_and_release"
& "$qtRoot/Tools/mingw730_64/bin/mingw32-make.exe" -j2
.\clangTest.exe
```

## 解析结果

程序默认读取项目中的 `MsgHeader.h`，解析目标为 `x86_64-w64-windows-gnu`。启用 `pack(1)` 时：

| 字段 | 声明类型 | 大小（字节） | 偏移（字节） |
| --- | --- | ---: | ---: |
| `msgId` | `uint16_t` | 2 | 0 |
| `msgType` | `uint8_t` | 1 | 2 |
| `dataLength` | `uint32_t` | 4 | 3 |
| `charValue` | `char` | 1 | 7 |
| `signedCharValue` | `signed char` | 1 | 8 |
| `unsignedCharValue` | `unsigned char` | 1 | 9 |
| `boolValue` | `bool` | 1 | 10 |
| `shortValue` | `short` | 2 | 11 |
| `unsignedShortValue` | `unsigned short` | 2 | 13 |
| `intValue` | `int` | 4 | 15 |
| `unsignedIntValue` | `unsigned int` | 4 | 19 |
| `longValue` | `long` | 4 | 23 |
| `unsignedLongValue` | `unsigned long` | 4 | 27 |
| `longLongValue` | `long long` | 8 | 31 |
| `unsignedLongLongValue` | `unsigned long long` | 8 | 39 |
| `floatValue` | `float` | 4 | 47 |
| `doubleValue` | `double` | 8 | 51 |
| `longDoubleValue` | `long double` | 16 | 59 |
| `wcharValue` | `wchar_t` | 2 | 75 |
| `char16Value` | `char16_t` | 2 | 77 |
| `char32Value` | `char32_t` | 4 | 79 |

结构体包含 21 个字段，总大小为 **83 字节**，对齐为 **1 字节**。上表是当前 MSYS2 Clang 目标环境的实测结果，`long`、`long double`、`wchar_t` 等类型的大小可能随平台和工具链变化。

这些值由 libclang 的 AST 和布局查询 API 读取。成员树同时显示字节偏移和原始 bit 偏移；字段的类型对齐值表示自然对齐，可能大于结构体的打包对齐值。

头文件在运行时解析，修改其内容后直接重新运行即可。也可以传入其他包含 `MsgHeader` 定义的头文件：

```powershell
.\clangTest.exe C:/path/to/other.h
```

## 回归检查

沿用上文构建环境的 PATH 设置，在项目根目录执行：

```powershell
New-Item -ItemType Directory -Path build/tree-tests -Force | Out-Null
Set-Location build/tree-tests
& "$qtBin/qmake.exe" ../../tests/nested-tests.pro -spec win32-clang-g++ "LIBCLANG_ROOT=$clangRoot" "CONFIG+=release" "CONFIG-=debug_and_release"
& "$qtRoot/Tools/mingw730_64/bin/mingw32-make.exe" -j2
$env:QT_QPA_PLATFORM = 'offscreen'
.\nested-tests.exe
Remove-Item Env:QT_QPA_PLATFORM
```

检查覆盖普通结构体、多层嵌套、类型别名、累计偏移、位域、被包含文件中的嵌套类型、指针/引用/数组不展开、树形节点展开折叠，以及重新解析和错误后的清空。

赋值检查使用独立构建目录。在项目根目录执行：

```powershell
New-Item -ItemType Directory -Path build/value-tests -Force | Out-Null
Set-Location build/value-tests
& "$qtBin/qmake.exe" ../../tests/value-tests.pro -spec win32-clang-g++ "LIBCLANG_ROOT=$clangRoot" "CONFIG+=release" "CONFIG-=debug_and_release"
& "$qtRoot/Tools/mingw730_64/bin/mingw32-make.exe" -j2
$env:QT_QPA_PLATFORM = 'offscreen'
.\value-tests.exe
Remove-Item Env:QT_QPA_PLATFORM
```

检查包含整数及位域边界、64 位整数精度、浮点格式与范围、布尔下拉框、只读字段、嵌套路径、无效输入提示，以及重新解析后的值保留和重置。

二进制检查采用相同构建方式，将项目文件换为 `tests/binary-tests.pro`，在独立目录构建并运行 `binary-tests.exe`。检查包括已知十六进制结果、原生 C++ 结构体内存比对、填充、带符号位域、浮点临界舍入和次正规数、不支持的布局，以及预览和复制操作。

## 启动错误：0xC0000135

退出码 `-1073741515`（`0xC0000135`）表示找不到运行所需的 DLL。请检查运行环境 PATH：

- 包含 `D:/msys64/mingw64/bin`，并放在旧版 MinGW 运行库目录前面。
- 包含所选 Qt 版本的 `bin` 目录；Qt Creator 的 Kit 会自动配置 Qt 路径。

Qt Creator 中的配置方法见上文“在 Qt Creator 中运行”。仅链接 `libclang.dll.a` 不会自动配置运行时的 DLL 搜索路径。

## 生成 Windows 便携压缩包

在已安装上述开发环境的电脑上，进入项目目录运行：

```powershell
.\package.ps1
```

安装路径不同时，可通过 `-QtRoot` 和 `-ClangRoot` 参数指定。脚本会构建 Release，部署 Qt、递归收集 DLL 依赖，并附带 C/C++ 标准库头文件、五个测试样例、程序源码和第三方许可文件。

输出为 `dist/clangTest-windows-x64.zip`，校验文件为同目录下的 `.zip.sha256`。将 ZIP 作为 GitHub Release 附件上传即可。

使用者只需完整解压，再双击 `clangTest.exe`，无需配置 Qt、Clang 或 PATH。程序会优先读取 `samples/MsgHeader.h`，并使用 `runtime/` 内的标准库及 Clang 内置头文件进行解析。
