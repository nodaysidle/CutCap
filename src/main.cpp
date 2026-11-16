#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml/qqml.h>
#include <QQuickStyle>
#include <QStringList>
#include <QUrl>

#include "backend.h"
#include "clipframeitem.h"
#include "design.h"
#include "cli.h"
#include "theme.h"

int main(int argc, char *argv[]) {
    QStringList rawArgs;
    for (int i = 0; i < argc; ++i)
        rawArgs << QString::fromLocal8Bit(argv[i]);

    if (Cli::shouldRunHeadless(rawArgs)) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QGuiApplication app(argc, argv);
        app.setApplicationName("cutcap");
        app.setApplicationVersion("0.1.0");
        return Cli::run(rawArgs);
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("cutcap");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("cutcap");
    app.setWindowIcon(QIcon::fromTheme("cutcap"));

    QQuickStyle::setStyle("Material");

    // Register the module's own types before injecting our instances: registering a singleton
    // instance seals the module version, and a later module registration would then be dropped.
    extern void qml_register_types_CutCap();
    qml_register_types_CutCap();

    Theme theme;
    Design design(theme);
    qmlRegisterSingletonInstance("CutCap", 1, 0, "Design", &design);
    qmlRegisterSingletonInstance("CutCap", 1, 0, "Theme", &theme);
    Backend backend;
    qmlRegisterSingletonInstance("CutCap", 1, 0, "Backend", &backend);

    // Support `cutcap open <file>...` or `cutcap <file>...`. A desktop launcher expands `%F` to
    // nothing, so `cutcap open` on its own must open an empty project, not import a file "open".
    for (const QString &file : Cli::guiFiles(rawArgs))
        backend.importSingleMedia(file);

    QQmlApplicationEngine engine;
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
