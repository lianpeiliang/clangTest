include(nested-tests.pro)
TARGET = binary-tests
SOURCES -= $$PWD/nested-tests.cpp
SOURCES += $$PWD/binary-tests.cpp
