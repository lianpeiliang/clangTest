include(nested-tests.pro)
TARGET = value-tests
SOURCES -= $$PWD/nested-tests.cpp
SOURCES += $$PWD/value-tests.cpp
