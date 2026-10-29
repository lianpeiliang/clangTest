include(../clangTest.pro)
TARGET = session-tests
QT = core
CONFIG += console
RESOURCES =
RC_ICONS =
SOURCES = $$PWD/session-tests.cpp $$PWD/../structuresession.cpp $$PWD/../headerparser.cpp $$PWD/../definitionjson.cpp $$PWD/../fieldvalue.cpp
HEADERS = $$PWD/../structuresession.h $$PWD/../headerparser.h $$PWD/../definitionjson.h $$PWD/../fieldvalue.h
