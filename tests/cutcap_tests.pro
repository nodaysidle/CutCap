QT += core gui qml quick quickcontrols2 multimedia concurrent dbus testlib

CONFIG += c++17 console
CONFIG -= app_bundle
CONFIG += qmltypes
QML_IMPORT_NAME = CutCap
QML_IMPORT_MAJOR_VERSION = 1
TARGET = cutcap_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/backend.h \
    ../src/design.h \
    ../src/cli.h \
    ../src/clipframeitem.h \
    ../src/cliplistmodel.h \
    ../src/mediaengine.h \
    ../src/project.h \
    ../src/theme.h

SOURCES += \
    ../src/backend.cpp \
    ../src/design.cpp \
    ../src/cli.cpp \
    ../src/clipframeitem.cpp \
    ../src/cliplistmodel.cpp \
    ../src/mediaengine.cpp \
    ../src/project.cpp \
    ../src/theme.cpp \
    cutcap_tests.cpp

RESOURCES += ../src/resources.qrc
