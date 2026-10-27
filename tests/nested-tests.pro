include(../clangTest.pro)

TARGET = nested-tests
CONFIG += console
INCLUDEPATH += $$PWD/..
SOURCES = $$PWD/nested-tests.cpp $$PWD/../headerparser.cpp $$PWD/../mainwindow.cpp $$PWD/../fieldvalue.cpp
HEADERS = $$PWD/../headerparser.h $$PWD/../mainwindow.h $$PWD/../fieldvalue.h
