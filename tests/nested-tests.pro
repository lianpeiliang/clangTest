include(../clangTest.pro)

TARGET = nested-tests
CONFIG += console
INCLUDEPATH += $$PWD/..
SOURCES = $$PWD/nested-tests.cpp $$PWD/../headerparser.cpp $$PWD/../mainwindow.cpp $$PWD/../fieldvalue.cpp $$PWD/../binaryencoder.cpp $$PWD/../binarydecoder.cpp $$PWD/../definitionjson.cpp $$PWD/../theme.cpp
HEADERS = $$PWD/../headerparser.h $$PWD/../mainwindow.h $$PWD/../fieldvalue.h $$PWD/../binaryencoder.h $$PWD/../binarydecoder.h $$PWD/../definitionjson.h $$PWD/../theme.h
