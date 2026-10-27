QT += core
QT -= gui

TEMPLATE = app
TARGET = clangTest
CONFIG += console c++11
CONFIG -= app_bundle

SOURCES += main.cpp
HEADERS += MsgHeader.h

# Override with qmake LIBCLANG_ROOT=C:/path/to/mingw64 if needed.
isEmpty(LIBCLANG_ROOT): LIBCLANG_ROOT = D:/msys64/mingw64
!exists($$LIBCLANG_ROOT/include/clang-c/Index.h): error("Cannot find clang-c/Index.h; set LIBCLANG_ROOT")

# Keep the MinGW system headers after the C++ standard library headers.
QMAKE_CXXFLAGS += -idirafter $$shell_quote($$LIBCLANG_ROOT/include)
LIBS += "$$LIBCLANG_ROOT/lib/libclang.dll.a"
DEFINES += LIBCLANG_ROOT=\\\"$$LIBCLANG_ROOT\\\"
DEFINES += MSG_HEADER_PATH=\\\"$$PWD/MsgHeader.h\\\"
