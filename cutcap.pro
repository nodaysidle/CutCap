QT += core gui qml quick quickcontrols2 multimedia concurrent dbus

CONFIG += c++17 release
TARGET = cutcap
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/design.h \
    src/clipframeitem.h \
    src/cli.h \
    src/mediaengine.h \
    src/project.h \
    src/theme.h

SOURCES += \
    src/backend.cpp \
    src/design.cpp \
    src/clipframeitem.cpp \
    src/cli.cpp \
    src/main.cpp \
    src/mediaengine.cpp \
    src/project.cpp \
    src/theme.cpp

RESOURCES += src/resources.qrc
