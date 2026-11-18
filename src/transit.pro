QT += widgets

CONFIG += c++17 release windows
CONFIG -= debug_and_release

TEMPLATE = app
TARGET = YuntuTrafficQt

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    transit_core.cpp

HEADERS += \
    mainwindow.h \
    transit_core.h

DESTDIR = $$PWD/bin
