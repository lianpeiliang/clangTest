include(nested-tests.pro)
TARGET = decoder-tests
SOURCES -= $$PWD/nested-tests.cpp
SOURCES += $$PWD/decoder-tests.cpp
