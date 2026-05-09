#pragma once

#include <QObject>

#include <memory>

namespace app {
class EnvConfig;
}

namespace infra::http {
class BackendServiceQt;
}

namespace infra::rtc {
class LiveKitRoomService;
}

namespace infra::device {
class DeviceServiceQt;
}

namespace presentation::controllers {
class HomeController;
class PrejoinController;
class RoomController;
}

namespace app {

class AppContext final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* homeController READ homeController CONSTANT)
    Q_PROPERTY(QObject* prejoinController READ prejoinController CONSTANT)
    Q_PROPERTY(QObject* roomController READ roomController CONSTANT)

public:
    explicit AppContext(QObject* parent = nullptr);
    ~AppContext() override;

    [[nodiscard]] QObject* homeController() const;
    [[nodiscard]] QObject* prejoinController() const;
    [[nodiscard]] QObject* roomController() const;

private:
    std::unique_ptr<EnvConfig> envConfig_;
    std::unique_ptr<infra::http::BackendServiceQt> backendService_;
    std::unique_ptr<infra::rtc::LiveKitRoomService> roomService_;
    std::unique_ptr<infra::device::DeviceServiceQt> deviceService_;
    std::unique_ptr<presentation::controllers::HomeController> homeController_;
    std::unique_ptr<presentation::controllers::PrejoinController> prejoinController_;
    std::unique_ptr<presentation::controllers::RoomController> roomController_;
};

}  // namespace app
