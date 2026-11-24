QT += core gui qml quick quickcontrols2 multimedia concurrent

CONFIG += c++17 release
CONFIG += qmltypes
QML_IMPORT_NAME = CutCap
QML_IMPORT_MAJOR_VERSION = 1
TARGET = cutcap
TEMPLATE = app

# The generated QML type registration includes the headers as <name.h>.
INCLUDEPATH += src

HEADERS += \
    src/backend.h \
    src/design.h \
    src/clipframeitem.h \
    src/cli.h \
    src/cliplistmodel.h \
    src/mediaengine.h \
    src/project.h \
    src/theme.h

SOURCES += \
    src/backend.cpp \
    src/design.cpp \
    src/clipframeitem.cpp \
    src/cli.cpp \
    src/cliplistmodel.cpp \
    src/main.cpp \
    src/mediaengine.cpp \
    src/project.cpp \
    src/theme.cpp

RESOURCES += src/resources.qrc
