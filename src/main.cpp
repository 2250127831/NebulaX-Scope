#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QUrl>

#include "SharedMemoryMonitor.h"

/*
 * NebulaX-Scope — QML frontend entry point.
 *
 * Creates the SharedMemoryMonitor and registers it as a QML singleton
 * (Monitor) for the ScopeWindow frontend.
 */
int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("NebulaX-Scope");
    app.setApplicationVersion("0.1.0");

    SharedMemoryMonitor monitor;
    monitor.start();
    qmlRegisterSingletonInstance("NebulaX.Scope", 1, 0, "Monitor", &monitor);

    QQmlApplicationEngine engine;
    engine.addImportPath("qrc:/");
    engine.load(QUrl(QStringLiteral("qrc:/NebulaX/Scope/qml/ScopeWindow.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
