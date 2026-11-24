# Codebase context for model routing

Written by `/jev-route init`; edit freely. `## General` facts go with every plan;
a `## <path>` section is sent only when a planned file is under that path.

## General

- Adding or renaming a QML file requires updating src/resources.qrc and qmldir.
- cutcap.qmltypes is likely generated or mirrors C++ types exposed to QML; update it when backend API changes.

## src/qml/

- QML relies on properties/invokables exposed by src/backend.h; changing them requires updating both sides.

## src/theme.cpp

- Visual values should follow DESIGN.md; keep theme.h/design.h in sync.

## tests/

- cutcap_tests.cpp is a single large QtTest file; tests/cutcap_tests.pro builds it against src/ sources.
