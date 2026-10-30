include(../clangTest.pro)

TARGET = nested-tests
CONFIG += console
INCLUDEPATH += $$PWD/..
SOURCES = $$PWD/nested-tests.cpp $$PWD/../headerparser.cpp $$PWD/../UI/mainwindow.cpp $$PWD/../fieldvalue.cpp $$PWD/../binaryencoder.cpp $$PWD/../binarydecoder.cpp $$PWD/../definitionjson.cpp $$PWD/../UI/theme.cpp
HEADERS = $$PWD/../headerparser.h $$PWD/../UI/mainwindow.h $$PWD/../fieldvalue.h $$PWD/../binaryencoder.h $$PWD/../binarydecoder.h $$PWD/../definitionjson.h $$PWD/../UI/theme.h

SOURCES += $$PWD/../UI/framelesswindow.cpp $$PWD/../UI/hexdialogs.cpp
HEADERS += $$PWD/../UI/framelesswindow.h $$PWD/../UI/hexdialogs.h
SOURCES += $$PWD/../UI/helpdialog.cpp
HEADERS += $$PWD/../UI/helpdialog.h

SOURCES += $$PWD/../UI/fieldeditor.cpp
HEADERS += $$PWD/../UI/fieldeditor.h

SOURCES += $$PWD/../structuresession.cpp
HEADERS += $$PWD/../structuresession.h

SOURCES += $$PWD/../UI/definitionpanel.cpp
HEADERS += $$PWD/../UI/definitionpanel.h
SOURCES += $$PWD/../UI/definitionfilebar.cpp $$PWD/../UI/structureselectorbar.cpp
HEADERS += $$PWD/../UI/definitionfilebar.h $$PWD/../UI/structureselectorbar.h
