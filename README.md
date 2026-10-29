# StructHex · 结构体数据编辑器

StructHex 是一个使用 qmake（`.pro`）构建的 Qt Widgets 结构体数据编辑工具，支持通过 libclang 自动发现 C++ 头文件中的结构体，或直接加载本程序的结构定义 JSON；统一显示成员树和内存布局，支持基础类型字段赋值，以及十六进制数据的预览、复制和文件导入/导出。启动时默认打开 `TestHeader/MsgHeader.h`，无需输入结构体名称。

应用图标采用 Microsoft Fluent UI System Icons 的 [Code Block 彩色图标](https://github.com/microsoft/fluentui-system-icons/blob/main/assets/Code%20Block/SVG/ic_fluent_code_block_48_color.svg)，遵循 MIT 许可。图标源文件、多尺寸 Windows `.ico` 及许可保存在 `assets/`；程序内嵌图标，便携包同时附带许可文件。

界面采用与图标一致的紫色主题：浅紫背景、白色内容区、紫色渐变主按钮及统一的输入框、字段树和弹窗样式。样式集中保存在 `assets/structhex.qss`，通过 `resources.qrc` 内嵌，在 `theme.cpp` 中加载；修改 QSS 后重新构建即可。无效输入使用红色边框和提示，禁用、悬停、焦点和选中状态分别定义。

主窗口使用紫色自定义标题栏，右侧提供最小化、最大化/还原和关闭按钮。Windows 下可拖动标题区域、双击最大化/还原、拖动边缘和四角缩放，并沿用系统的贴边吸附；最大化使用当前显示器的工作区，保留任务栏。窗口最小尺寸为 1040 × 640。窗口操作检查使用 `tests/window-tests.pro`，需在 Windows 桌面环境运行。

## 项目文件

| 文件 | 用途 |
| --- | --- |
| `clangTest.pro` | Qt 项目配置及 libclang 链接配置 |
| `main.cpp` | 创建 QApplication，启动窗口 |
| `mainwindow.h / .cpp` | 协调结构定义加载、字段编辑区、操作按钮及解析诊断 |
| `fieldeditor.h / .cpp` | 独立字段编辑区，管理树形控件、输入值、校验和填回 |
| `framelesswindow.h / .cpp` | 自定义标题栏、窗口按钮及 Windows 原生拖动、缩放、工作区处理 |
| `hexdialogs.h / .cpp` | 十六进制预览、复制/导出、解析与粘贴导入弹窗及输入记忆 |
| `headerparser.h / .cpp` | 调用 libclang，返回结构体和字段的查询结果 |
| `fieldvalue.h / .cpp` | 根据字段类型、位宽校验输入，保留完整的 64 位整数文本 |
| `binaryencoder.h / .cpp` | 按目标布局编码当前字段值，并格式化十六进制预览 |
| `binarydecoder.h / .cpp` | 校验十六进制输入，按已导入布局和字节序解析字段 |
| `definitionjson.h / .cpp` | 导出及校验加载结构定义 JSON，使用 QSaveFile 保存 |
| `TestHeader/MsgHeader.h` | 待解析的结构体定义，使用 `#pragma pack(push, 1)` |
| `TestHeader/SimpleMsgHeader.h` | 只有 3 个基础类型字段的简单对比样例，同样使用 1 字节打包 |
| `TestHeader/MinimalMsgHeader.h` | 只有 2 个字段的最小样例，使用默认对齐 |
| `TestHeader/PacketHeader.h` | 不同名称的样例，定义 `PacketHeader`，包含 3 个字段，使用 1 字节打包 |
| `TestHeader/NestedPacketHeader.h` | 嵌套样例，`NestedPacketHeader` 包含 `PacketAddress` 类型的成员，使用 1 字节打包 |
| `TestHeader/MultiStructHeader.h` | 包含 20 个结构体的自动发现与下拉选择样例 |
| `tests/discovery-tests.pro` | 单个/20 个结构体、名称限定、自动选择及重载的回归检查 |
| `tests/nested-tests.pro` | 嵌套解析、偏移累计和树形界面的回归检查 |
| `tests/value-tests.pro` | 字段赋值、数值范围、只读类型和内存保存的回归检查 |
| `tests/binary-tests.pro` | 二进制与原生 C++ 内存布局比对，以及十六进制预览检查 |
| `tests/decoder-tests.pro` | 十六进制格式、大小端解析、数值边界、嵌套映射和填回界面检查 |
| `tests/definition-tests.pro` | 结构定义 JSON、嵌套元数据、不含值及导出保存检查 |
| `tests/json-import-tests.pro` | JSON 导入、格式和布局校验、双来源界面和二进制一致性检查 |

测试和示例头文件统一放在 `TestHeader/` 目录，便携包及随包源码也保留同名目录。

`build/` 和 `*.pro.user` 已加入 Git 忽略规则。

## 建议阅读顺序

界面按职责分为四块：`MainWindow` 组织主界面，协调结构定义加载与操作按钮；`FieldEditor` 独立持有字段树、输入值和校验状态，通过信号通知主窗口更新按钮；基类 `FramelessWindow` 只处理标题栏与系统窗口行为；`HexDialogs` 持有弹窗状态，通过回调请求填回字段或导入文本。编码、解码及定义 JSON 继续由各自模块完成。弹窗以主窗口为 Qt 父对象，切换结构定义时由主窗口通知关闭编辑弹窗。`tests/fieldeditor-tests.pro` 检查编辑区脱离主窗口后的独立使用、校验通知、填回、原值恢复和清空。

初次阅读可以先打开 `TestHeader/MinimalMsgHeader.h`，对照这个只有两个字段的样例，再按下面的顺序看源码：

1. `main.cpp`：程序从这里开始，创建应用和窗口，进入 Qt 事件循环。
2. `headerparser.h`：先认识 `FieldInfo`、`StructInfo` 和解析结果，各功能都围绕这些数据工作。
3. `mainwindow.cpp`：先看构造函数如何创建控件、连接信号，再看 `reloadDefinition()` → `showSelectedStructure()` → `appendFields()`，理解文件到界面的流程。
4. `headerparser.cpp`：从 `parseHeaderStructures()` 看解析入口，再看 `discoverRecords()`、`parseRecord()`、`visitField()`，理解结构体发现、布局读取和嵌套递归。
5. `fieldvalue.cpp`：看 `validateFieldValue()`，理解字符串格式检查、数值转换及范围校验。
6. `binaryencoder.cpp` / `binarydecoder.cpp`：看字段值如何变成字节，以及如何按偏移、位宽和字节序读回来。
7. `definitionjson.cpp`：看结构定义如何递归转为 JSON，以及读取时怎样检查数据。

常见写法：`const T &` 表示通过只读引用传参；`auto` 让编译器推导变量类型；`[this, ...](...) { ... }` 是 lambda，常用来处理 Qt 信号。`QVector` 保存顺序列表，`QHash` 按键查找数据。布局中的 `size`、`alignment` 以字节为单位，名字带 `Bits` 的偏移以 bit 为单位，1 字节 = 8 bit。

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

- 启动时默认解析 `TestHeader/MsgHeader.h`，上方显示结构体名称、字段数量、总大小和结构体对齐。
- 树形控件显示字段名称、声明类型、展开别名后的类型、大小、自然对齐及偏移。根节点为所选结构体，成员按层级显示，可点击箭头展开、折叠，或使用 **全部展开 / 全部折叠**。
- 同时显示相对直接所属结构体和相对最外层结构体的偏移，均提供字节和 bit 两种表示；位域的字节偏移保留不足一个字节的 bit 部分。
- 在 **值** 列直接编辑基础类型字段；布尔值使用下拉框。嵌套结构体作为分组，展开后可分别填写内部字段。
- 点击 **查看二进制**，在弹窗中查看当前值对应的十六进制快照，也可点击 **复制十六进制**。
- 点击 **解析十六进制…**，输入字节并选择大小端，按当前结构体布局查看字段原始字节和解析值；可填入主窗口的可编辑字段。
- 点击 **从文件导入16进制数据**，选择之前导出的 `.hex` / `.txt` 文本，立即恢复当前结构体的可编辑字段，然后继续修改。
- 修改并保存头文件后，点击 **重新加载** 更新结果。
- 点击 **打开结构定义…** 选择头文件或本程序导出的 `.json`，也可直接修改路径并按回车。界面显示来源和当前目标平台。
- **选择结构体** 自动列出主头文件中的 struct 定义，上方显示发现数量。只有一个时直接加载；有多个时默认显示声明顺序中的第一个，使用下拉框即可切换，无需输入名称。同一头文件重新加载时保留仍存在的选择；从 JSON 切回上次头文件时也尽量恢复选择。在 JSON 模式下显示文件中的结构体名称，下拉框不可切换。
- 解析失败时清空旧树，并在下方显示错误原因；修正文件后可再次解析。

libclang 的查询结果先复制到 `StructInfo` 和 `FieldInfo`，再交给窗口显示。解析资源释放后，界面仍可安全使用这些普通 Qt 数据。

对比测试时，用 **打开结构定义…** 分别选择 `TestHeader/MsgHeader.h`（21 个字段、83 字节）、`TestHeader/SimpleMsgHeader.h`（3 个字段、7 字节）或 `TestHeader/MinimalMsgHeader.h`（2 个字段、4 字节）。最小样例使用默认对齐，两个字段偏移为 0、2 字节，可观察到中间的填充。各文件都定义名为 `MsgHeader` 的结构体，应分别解析，不要在同一个源文件中同时包含这些头文件。

验证单个结构体时，打开 `TestHeader/PacketHeader.h`，程序直接识别并加载 `PacketHeader`。预期显示 `sequence`、`payloadLength`、`flags` 三个字段，偏移分别为 0、4、6 字节，总大小 7 字节，对齐 1 字节。

验证多个结构体时，打开 `TestHeader/MultiStructHeader.h`，应发现 20 个结构体，依次列出 `Packet01`～`Packet20`，选择后立即显示对应字段。结构体数量没有固定为 20。

验证嵌套结构体时，打开 `TestHeader/NestedPacketHeader.h`，下拉框会列出 `PacketAddress` 和 `NestedPacketHeader`。选择 `NestedPacketHeader` 后，预期显示 `sequence`、`address`、`payloadLength` 三个直接字段，偏移分别为 0、4、8 字节，总大小 10 字节，对齐 1 字节。展开 `address` 可看到 `source`、`destination`：相对偏移分别为 0、2 字节，绝对偏移分别为 4、6 字节。也可选择 `PacketAddress` 单独查看。

自动发现仅列出当前主文件中有定义的 struct，包括命名空间、外层类型中的命名结构体，以及 `typedef struct { ... } Name`；普通 `typedef` / `using` 别名不重复列出。同名类型使用 `a::Packet`、`b::Packet` 这样的完整名称区分。前置声明、被包含文件中的独立定义、注释、未启用的条件编译分支、函数局部类型、未实例化模板和没有名称的匿名成员不列为候选。按值嵌套字段仍可展开被包含文件中的类型。

每次打开或重新加载头文件只创建一个 libclang 翻译单元，复制各结构体的布局后释放解析资源；下拉切换使用本次加载的结果。某个结构体无法读取布局时，选择它会显示诊断，其他布局正常的结构体仍可使用；文件存在语法/语义错误时整体加载失败。

代码接口 `parseHeaderStructures(path)` 返回全部候选及各自的布局状态；`parseHeader(path)` 自动解析唯一结构体，存在多个时返回候选名称及选择提示。原有 `parseHeader(path, name)` 仍支持完整名称或无歧义的短名称。

按值嵌套的记录类型会递归展开，包括经过 `typedef` / `using` 别名的类型；嵌套类型可定义在被包含的头文件中。指针、引用和数组保持为叶子节点，不追踪其指向对象或展开数组元素。顶部字段数量仅统计所选结构体的直接字段。

## 字段赋值

- 整数支持十进制及带 `0x` / `0X` 前缀的十六进制，例如 `42`、`0x2A`，有符号负数可写成 `-0x10`。根据类型位宽和位域宽度校验范围：`unsigned short` 接受 0～65535（`0xFFFF`），3 位无符号位域接受 0～7（`0x7`）。64 位整数以文本保存，避免浮点转换丢失精度。
- 十六进制按数值解释，不自动按补码解释负数。例如 32 位 `int` 接受 `0x7FFFFFFF`、`-0x80000000`，拒绝超出正数范围的 `0xFFFFFFFF`。无符号类型不接受负数。无前缀的 `010` 仍表示十进制 10，`FF` 等省略前缀的写法不接受。
- `char`、`wchar_t`、`char16_t`、`char32_t` 等字符类型按十进制或 `0x` 前缀十六进制数值编码填写。
- `bool` 通过下拉框选择 `false` 或 `true`。
- `float`、`double`、`long double` 接受小数及科学计数法，例如 `-1.25`、`1e-3`；使用小数点，拒绝非法格式、NaN、无穷大、溢出和非零数值下溢至 0。
- 指针、引用、数组、枚举、const 字段及其子成员等暂时只读，悬停可查看原因。结构体分组本身不能赋值。

输入无效时编辑框标红，上方显示字段路径和错误原因，内存中保留上一次有效值；修正后自动更新。值按 `address.source` 这样的完整路径单独保存，同名嵌套字段互不覆盖。

合法输入的进制写法会在重新加载时保留。二进制预览按数值编码，例如 `0x1234` 与十进制 `4660` 生成相同的字节。

数值初始为 `0`，布尔值初始为 `false`，暂不读取头文件中的成员初始化表达式。重新加载同一文件、同一结构体时保留仍符合类型要求的值；字段范围变化导致旧值无效时恢复初始值。切换文件、切换结构体或解析失败时清空之前的值。本版值仅保存在当前窗口内存中，关闭后不保留，不修改头文件。

## 十六进制预览

编辑完成后点击 **查看二进制**。程序按 libclang 返回的字段绝对偏移和结构体总大小编码，采用当前固定目标 Windows x64 / MinGW 的小端序；结构体填充字节、未命名位域和 `long double` 的填充部分统一补 0。支持嵌套结构体、整数、字符编码、bool、位域以及 float / double / long double。

例如 `NestedPacketHeader` 的 `sequence = 305419896`、`address.source = 4386`、`address.destination = 13124`、`payloadLength = 21862`，会显示：

```text
00000000  78 56 34 12 22 11 44 33 66 55
```

左侧是十六进制字节偏移，每行最多 16 字节；复制按钮只复制字节，不包含偏移。预览是点击时的快照，修改值后再次点击按钮可生成新快照。

在预览窗口点击 **导出到文件**，选择位置即可保存当前预览的十六进制文本。默认文件名为 `结构体名称.hex`，也可选择 `.txt`；没有填写扩展名时按所选类型自动补全。文件只包含大写十六进制字节，每字节两位、空格分隔、每行最多 16 字节，不含偏移地址或其他说明，可直接将文件内容粘贴到 **导入16进制数据** 中恢复字段值。导出使用打开预览时的快照，与复制按钮一致；取消保存不写入文件，保存失败会在预览窗口显示原因。

数组保持只读，不展开、不接受赋值，但允许二进制预览：按数组实际大小和绝对偏移将整个数组区域填为 `00`，后续字段的位置保持不变。该规则也适用于嵌套、多维、类型别名、结构体元素和 const 数组；数组元素内的填充也全为 0。

没有解析结果或存在无效输入时，按钮不可用。包含指针、引用、枚举、非数组 const 等尚无可编码值的字段，或存在联合体、继承、虚函数、匿名结构体/联合体等暂不支持的布局时，会提示具体原因并停止生成。单个结构体预览大小上限为 1 MiB。

## 导入16进制数据并继续编辑

需要恢复之前保存的字段值时，先加载对应头文件或结构定义 JSON，选择结构体，然后点击 **导入16进制数据**。粘贴此前通过 **复制十六进制** 保存的内容，点击 **导入**，成功后弹窗关闭，主界面的字段值立即更新。随后可修改任意可编辑字段，再点击 **查看二进制** 获取修改后的数据。

例如选择 `NestedPacketHeader`，导入 `78 56 34 12 22 11 44 33 66 55`，主界面会显示 `sequence=305419896`、`address.source=4386`、`address.destination=13124`、`payloadLength=21862`。将 `address.destination` 改为 `0x7788` 后，生成的字节为 `78 56 34 12 22 11 88 77 66 55`。

- 直接导入使用与二进制预览一致的小端序。支持连续十六进制、空格/换行/Tab/逗号分隔和 `0x` 前缀；须提供完整结构体大小的字节，包含填充，不含偏移地址。
- 输入格式、长度、布局或可编辑字段数值有误时显示原因，所有现有字段值保持不变；NaN、无穷大等不能用于编辑的值会阻止整次导入。取消也不会修改字段值。
- 导入同时更新文本框、布尔下拉框及嵌套字段，并清除被替换字段的输入错误。当前有无效输入也可以使用导入按钮恢复值；尚未成功加载结构体时按钮不可用。
- 导入恢复可编辑基础字段，数组等只读字段和原始填充字节不会保存。再次编码仍沿用现有规则；需要查看原始字节或按大端序解释时，使用 **解析十六进制…**。

也可点击 **从文件导入16进制数据**，直接选择此前通过预览窗口 **导出到文件** 保存的 `.hex` 或 `.txt`。文件使用纯文本（ASCII / UTF-8，支持 UTF-8 BOM），内容格式与粘贴导入相同。成功后主界面立即显示可编辑的字段值，并提示来源文件；之后可以修改字段、重新查看二进制并导出新文件。请先加载匹配的结构体定义，数据文件本身不包含结构定义或结构体名称。

文件导入与粘贴导入共用小端解析和完整校验：文件无法读取、内容格式或长度不符、包含无法编辑的数值时，均保留原值和现有输入；取消选择也不修改字段。文本文件大小上限为 8 MiB，解码后数据上限仍为 1 MiB。只读字段、数组和填充字节的处理沿用上面的规则。

## 十六进制映射到结构体

先加载头文件或结构定义 JSON，再点击 **解析十六进制…**。输入完整结构体的字节，默认使用小端序，点击 **解析**。结果树显示字段名称、类型、绝对字节偏移、原始字节、解析值及说明；嵌套字段按层级展开。

输入可以连续书写（`78563412`），也可以用空格、换行、Tab 或逗号分隔（`78 56 34 12`、`0x78, 0x56, 0x34, 0x12`）。每段须为偶数位十六进制字符，前缀 `0x` 大小写均可。输入表示按顺序排列的字节，`0x1234` 表示字节 `12 34`。不接受偏移地址、ASCII 栏、冒号或非法字符；复制已有预览时请使用 **复制十六进制** 按钮。

例如加载 `TestHeader/NestedPacketHeader.h`，在下拉框中选择 `NestedPacketHeader`，输入：

```text
78 56 34 12 22 11 44 33 66 55
```

小端解析结果为 `sequence=305419896`、`address.source=4386`、`address.destination=13124`、`payloadLength=21862`。选择大端后，输入 `12 34 56 78 11 22 33 44 55 66` 可得到相同的字段值。

- 支持有符号/无符号整数（含完整 64 位数值）、字符数值、bool、float、double、小端 long double、嵌套结构体及小端位域。负整数按补码解释。bool 按非零为 true 解释，非规范值附带提示。NaN、无穷大及不符合编辑器范围的数值仅显示，不填回。
- 使用导入的绝对偏移提取数据，跳过填充字节和未命名位域。输入长度必须与结构体总大小完全一致，包含填充；过长和过短均提示具体字节数，出错时不显示部分结果，不修改字段值。数据上限为 1 MiB。
- 大端选项仅改变字段内部的字节解释，字段位置和对齐仍使用已导入的 Windows x64 / MinGW 布局，不重算另一套 ABI。大端位域和大端 long double 暂不支持，会明确报错。
- 数组当前没有逐元素元数据，显示整个区域的原始字节；指针、引用、枚举、const 等只读类型暂只显示原始字节。长字段的原始字节栏最多显示前 32 字节并标出总长度。联合体、继承、虚函数等无法确定映射的布局会阻止解析。
- 点击 **填入可编辑字段** 才会更新主窗口，关闭弹窗不会自动修改值。填回只更新通过校验的基础字段，原始数组及填充内容不会保存；再次生成二进制预览时，仍按现有规则使用小端序，并将数组、填充及未命名位域补 0。
- 修改输入或字节序后，旧结果和填回按钮立即失效，须重新解析。当前窗口记住上次输入和字节序，关闭程序后不保留。

## 导出结构定义 JSON

成功解析头文件中的结构体后，点击 **导出结构定义 JSON**，选择保存位置，默认文件名为 `结构体名称.json`。导出的是当前已经解析的结构体；修改并保存头文件后，应先点击 **重新加载** 更新定义。

文件为缩进格式的 UTF-8 JSON，不包含输入值或成员初始化表达式。输入值有错误、数组只读或二进制编码暂不支持，都不影响结构定义导出；解析失败时按钮不可用。

顶层保存 `schemaVersion`、`target`、`byteOrder`、`name`、`size`、`alignment` 和 `fields`。当前格式版本为 1，目标为 `x86_64-w64-windows-gnu`，字节序为 `little`。这是一份本程序的结构描述格式。

字段保存 `name`、`type`、`canonicalType`、`kind`、`size`、`alignment`、`offsetBits`、`absoluteOffsetBits`、`isArray`、`isBitField`、`editable` 和递归的 `fields`。整数附带 `integerBits`，位域附带 `bitWidth`；必要时附带只读原因和二进制编码限制。`size`、`alignment` 单位为字节，偏移单位为 bit：`offsetBits` 相对直接所属结构体，`absoluteOffsetBits` 相对最外层结构体。

数组保留原始类型、展开别名后的类型、尺寸和数组标记，当前不展开元素，`fields` 为空数组。普通叶子字段的 `fields` 同样为空数组。布局整数通常保存为 JSON 数字；超过 ±(2^53−1) 的整数以十进制字符串保存，避免 Qt 5 JSON 数字转换丢失精度。

## 加载结构定义 JSON

使用 **打开结构定义…** 选择 `.json` 文件，或在启动参数中传入 JSON 路径。扩展名匹配不区分大小写。JSON 加载路径直接重建 `StructInfo` 和 `FieldInfo`，不调用 libclang；随后使用相同的树形展示、十进制/十六进制赋值、数组填零、二进制预览和定义导出功能。

只接受本程序的结构定义格式，当前支持 `schemaVersion=1`、`target=x86_64-w64-windows-gnu`、`byteOrder=little`。普通业务 JSON 和包含实例值的文件不能作为结构定义加载。导入时检查必需字段、类型、整数和位域宽度、字段名称重复、大小、对齐、边界及相对/绝对偏移的一致性；错误会显示具体 JSON 字段路径。文件上限为 16 MiB、字段总数上限为 10000、嵌套层级限制为 64。

点击 **重新加载** 会按当前来源重新读取。加载同一文件、同一结构体时保留仍合法的输入值；切换来源或加载失败时清空旧结果和值。加载失败也会禁用导出和二进制按钮。此改动没有移除主程序现有的 libclang 动态链接依赖，JSON 路径自身不使用编译器解析。

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
.\StructHex.exe
```

## 解析结果

程序默认读取项目中的 `TestHeader/MsgHeader.h`，解析目标为 `x86_64-w64-windows-gnu`。启用 `pack(1)` 时：

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

头文件在运行时解析，修改其内容后点击 **重新加载** 即可。也可以传入其他头文件，结构体名称会自动识别：

```powershell
.\StructHex.exe C:/path/to/other.h
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

检查覆盖普通结构体、多层嵌套、类型别名、累计偏移、位域、被包含文件中的嵌套类型、指针/引用/数组不展开、树形节点展开折叠，以及重新加载和错误后的清空。

自动发现检查采用相同构建方式，将项目文件换为 `tests/discovery-tests.pro`，在独立目录构建并运行 `discovery-tests.exe`。覆盖单个及 20 个结构体、命名空间同名、嵌套定义、匿名 typedef、别名去重、主文件过滤、前置声明、条件编译、布局失败隔离、下拉切换、值重置、重载及 JSON 模式。

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

检查包含整数及位域边界、64 位整数精度、浮点格式与范围、布尔下拉框、只读字段、嵌套路径、无效输入提示，以及重新加载后的值保留和重置。

二进制检查采用相同构建方式，将项目文件换为 `tests/binary-tests.pro`，在独立目录构建并运行 `binary-tests.exe`。检查包括已知十六进制结果、原生 C++ 结构体内存比对、填充、带符号位域、浮点临界舍入和次正规数、不支持的布局，以及预览和复制操作。

结构定义导出检查同样使用独立目录和 `tests/definition-tests.pro`，运行 `definition-tests.exe`。检查覆盖嵌套布局、类型别名、数组、位域、不含输入值、UTF-8 文件保存、导出按钮、保存对话框取消和错误处理。

JSON 导入检查使用 `tests/json-import-tests.pro`，运行 `json-import-tests.exe`。检查覆盖已有五个样例的导出/导入往返、格式及布局错误、二进制输出、打开对话框、来源提示、JSON 名称只读、重新加载和头文件模式恢复。

十六进制解析检查使用 `tests/decoder-tests.pro`，在独立目录构建并运行 `decoder-tests.exe`。检查覆盖严格格式校验、长度错误、大小端整数/浮点、JSON 与嵌套布局、非零填充、整数边界、带符号位域、浮点特殊值、数组/指针原始字节、布局错误，以及解析、失效和填回界面。直接导入检查还覆盖错误和取消时保留值、嵌套与布尔控件同步、无效输入恢复、导入后修改并复制新字节、非有限值阻止整次导入，以及重载和加载失败时的状态更新。

同一检查还覆盖十六进制文件导出/导入往返、导入后继续编辑、头文件与 JSON 两种定义来源、中文文件名、UTF-8 BOM、CRLF、读取失败、取消选择、空文件/非法内容/长度错误/超限文件时保留原值，以及布尔字段同步和非有限值拒绝。

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

输出为 `dist/StructHex-windows-x64.zip`，校验文件为同目录下的 `.zip.sha256`。将 ZIP 作为 GitHub Release 附件上传即可。

使用者只需完整解压，再双击 `StructHex.exe`，无需配置 Qt、Clang 或 PATH。程序会优先读取 `TestHeader/MsgHeader.h`，并使用 `runtime/` 内的标准库及 Clang 内置头文件进行解析。
