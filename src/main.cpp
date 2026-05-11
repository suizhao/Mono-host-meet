#include "app/app_context.h"
#include "presentation/controllers/prejoin_controller.h"
#include "presentation/controllers/room_controller.h"
#include "presentation/qml/frame_image_provider.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  QQmlApplicationEngine engine;

  qmlRegisterUncreatableType<HomeController>("MultiLive", 1, 0, "HomeController",
      "HomeController cannot be created from QML");
  qmlRegisterUncreatableType<RoomController>("MultiLive", 1, 0, "RoomController",
      "RoomController cannot be created from QML");
  qmlRegisterUncreatableType<PrejoinController>("MultiLive", 1, 0, "PrejoinController",
      "PrejoinController cannot be created from QML");

  auto* frameImageProvider = new FrameImageProvider();
  engine.addImageProvider("multilive", frameImageProvider);

  AppContext appContext(frameImageProvider);
  engine.rootContext()->setContextProperty("appContext", &appContext);
  engine.rootContext()->setContextProperty("homeController",
                                           static_cast<QObject*>(appContext.homeController()));
  engine.rootContext()->setContextProperty(
      "prejoinController", static_cast<QObject*>(appContext.prejoinController()));
  engine.rootContext()->setContextProperty("roomController",
                                           static_cast<QObject*>(appContext.roomController()));

  QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                   []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

  engine.loadFromModule("MultiLive", "App");
  return app.exec();
}
