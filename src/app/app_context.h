#pragma once

#include "app/env_config.h"
#include "presentation/controllers/home_controller.h"
#include "presentation/controllers/prejoin_controller.h"
#include "presentation/controllers/room_controller.h"

#include <QObject>
#include <memory>

class IBackendService;
class IRoomService;
class IDeviceService;
class BackendServiceQt;
class LiveKitRoomService;
class DeviceServiceQt;
class FrameImageProvider;

class AppContext : public QObject {
  Q_OBJECT
  Q_PROPERTY(HomeController* homeController READ homeController CONSTANT)
  Q_PROPERTY(PrejoinController* prejoinController READ prejoinController CONSTANT)
  Q_PROPERTY(RoomController* roomController READ roomController CONSTANT)

public:
  explicit AppContext(FrameImageProvider* frameImageProvider, QObject* parent = nullptr);
  ~AppContext() override;

  HomeController* homeController() const;
  PrejoinController* prejoinController() const;
  RoomController* roomController() const;

private:
  EnvConfig m_envConfig;
  std::unique_ptr<BackendServiceQt> m_backendService;
  std::unique_ptr<LiveKitRoomService> m_roomService;
  std::unique_ptr<DeviceServiceQt> m_deviceService;
  std::unique_ptr<HomeController> m_homeController;
  std::unique_ptr<PrejoinController> m_prejoinController;
  std::unique_ptr<RoomController> m_roomController;
};
