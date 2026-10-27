QT += core gui widgets

TEMPLATE = app
TARGET = clangTest
CONFIG += c++11
CONFIG -= console app_bundle

SOURCES += main.cpp headerparser.cpp mainwindow.cpp
HEADERS += MsgHeader.h headerparser.h mainwindow.h

# MSYS2 MINGW64 安装目录，可用 qmake LIBCLANG_ROOT=C:/path/to/mingw64 覆盖。
isEmpty(LIBCLANG_ROOT): LIBCLANG_ROOT = D:/msys64/mingw64
!exists($$LIBCLANG_ROOT/include/clang-c/Index.h): error("Cannot find clang-c/Index.h; set LIBCLANG_ROOT")

# 将此目录放在常规头文件搜索目录之后，避免打乱 C++ 标准库的 include_next 顺序。
QMAKE_CXXFLAGS += -idirafter $$shell_quote($$LIBCLANG_ROOT/include)
# 链接 libclang C API 的 DLL 导入库；运行时仍需通过 PATH 找到 DLL 及其依赖。
LIBS += "$$LIBCLANG_ROOT/lib/libclang.dll.a"
# 将工具链目录及默认待解析头文件路径作为字符串宏传入程序。
DEFINES += LIBCLANG_ROOT=\\\"$$LIBCLANG_ROOT\\\"
DEFINES += MSG_HEADER_PATH=\\\"$$PWD/MsgHeader.h\\\"
