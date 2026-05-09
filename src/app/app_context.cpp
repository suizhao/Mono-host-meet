#include "app/app_context.h"

#include "app/env_config.h"
#include "infra/device/device_service_qt.h"
#include "infra/http/backend_service_qt.h"
#include "infra/rtc/livekit_room_service.h"
#include "presentation/controllers/home_controller.h"
#include "presentation/controllers/prejoin_controller.h"
#include "presentation/controllers/room_controller.h"

namespace app {

AppContext::AppContext(QObject* parent)
    : QObject(parent),
      envConfig_(std::make_unique<EnvConfig>()),
      backendService_(std::make_unique<infra::http::BackendServiceQt>(envConfig_->apiBaseUrl(), this)),
      roomService_(std::make_unique<infra::rtc::LiveKitRoomService>(this)),
      deviceService_(std::make_unique<infra::device::DeviceServiceQt>(this)),
      homeController_(std::make_unique<presentation::controllers::HomeController>(this)),
      prejoinController_(
          std::make_unique<presentation::controllers::PrejoinController>(backendService_.get(), this)),
      roomController_(std::make_unique<presentation::controllers::RoomController>(roomService_.get(), this)) {
    Q_UNUSED(deviceService_);
}

AppContext::~AppContext() = default;

QObject* AppContext::homeController() const {
    return homeController_.get();
}

QObject* AppContext::prejoinController() const {
    return prejoinController_.get();
}

QObject* AppContext::roomController() const {
    return roomController_.get();
}

}  // namespace app
