#include "app/app_context.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    app::AppContext appContext;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appContext", &appContext);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("MultiLive", "App");

    return QCoreApplication::exec();
}
