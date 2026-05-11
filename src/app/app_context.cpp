#include "app/app_context.h"

#include "infra/device/device_service_qt.h"
#include "infra/http/backend_service_qt.h"
#include "infra/rtc/livekit_room_service.h"
#include "presentation/controllers/home_controller.h"
#include "presentation/controllers/prejoin_controller.h"
#include "presentation/controllers/room_controller.h"
#include "presentation/qml/frame_image_provider.h"

AppContext::AppContext(FrameImageProvider* frameImageProvider, QObject* parent)
    : QObject(parent), m_envConfig(EnvConfig::fromEnvironment()) {
  // 使用 unique_ptr 作为唯一所有权，避免与 QObject parent 形成双重释放。
  m_backendService =
      std::make_unique<BackendServiceQt>(m_envConfig.backendBaseUrl, nullptr);
  m_roomService = std::make_unique<LiveKitRoomService>();
  m_deviceService = std::make_unique<DeviceServiceQt>(nullptr);

  m_homeController =
      std::make_unique<HomeController>(m_backendService.get(), m_roomService.get(), nullptr);
  m_prejoinController = std::make_unique<PrejoinController>(nullptr);
  m_roomController = std::make_unique<RoomController>(m_roomService.get(), m_deviceService.get(),
                                                      frameImageProvider, nullptr);
}

AppContext::~AppContext() = default;

HomeController* AppContext::homeController() const { return m_homeController.get(); }

PrejoinController* AppContext::prejoinController() const { return m_prejoinController.get(); }

RoomController* AppContext::roomController() const { return m_roomController.get(); }
