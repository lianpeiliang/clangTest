# qmake 项目文件：声明 Qt 模块、编译选项、源码文件以及外部库依赖。
QT += core gui widgets

TEMPLATE = app
TARGET = StructHex
CONFIG += c++11
CONFIG -= console app_bundle

# Windows 可执行文件图标，以及窗口/任务栏使用的内嵌图标资源。
win32:RC_ICONS = $$PWD/assets/structhex.ico
RESOURCES += $$PWD/resources.qrc

# SOURCES 中的 .cpp 会参与编译；HEADERS 列出头文件，便于 Qt Creator 展示和项目管理。
SOURCES += main.cpp headerparser.cpp mainwindow.cpp fieldvalue.cpp binaryencoder.cpp binarydecoder.cpp definitionjson.cpp
HEADERS += headerparser.h mainwindow.h fieldvalue.h binaryencoder.h binarydecoder.h definitionjson.h
HEADERS += TestHeader/MsgHeader.h TestHeader/SimpleMsgHeader.h TestHeader/MinimalMsgHeader.h \
           TestHeader/PacketHeader.h TestHeader/NestedPacketHeader.h TestHeader/MultiStructHeader.h

# MSYS2 MINGW64 安装目录，可用 qmake LIBCLANG_ROOT=C:/path/to/mingw64 覆盖。
isEmpty(LIBCLANG_ROOT): LIBCLANG_ROOT = D:/msys64/mingw64
!exists($$LIBCLANG_ROOT/include/clang-c/Index.h): error("Cannot find clang-c/Index.h; set LIBCLANG_ROOT")

# 将此目录放在常规头文件搜索目录之后，避免打乱 C++ 标准库的 include_next 顺序。
QMAKE_CXXFLAGS += -idirafter $$shell_quote($$LIBCLANG_ROOT/include)
# 链接 libclang C API 的 DLL 导入库；运行时仍需通过 PATH 找到 DLL 及其依赖。
LIBS += "$$LIBCLANG_ROOT/lib/libclang.dll.a"
# 将工具链目录及默认待解析头文件路径作为字符串宏传入程序。
DEFINES += LIBCLANG_ROOT=\\\"$$LIBCLANG_ROOT\\\"
DEFINES += MSG_HEADER_PATH=\\\"$$PWD/TestHeader/MsgHeader.h\\\"
