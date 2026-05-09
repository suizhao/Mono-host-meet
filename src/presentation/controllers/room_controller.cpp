#include "presentation/controllers/room_controller.h"

#include "domain/services/room_service.h"

namespace presentation::controllers {

RoomController::RoomController(domain::services::IRoomService* roomService, QObject* parent)
    : QObject(parent), roomService_(roomService) {}

bool RoomController::micEnabled() const {
    return micEnabled_;
}

void RoomController::setMicEnabled(bool value) {
    if (micEnabled_ == value) {
        return;
    }
    micEnabled_ = value;
    if (roomService_) {
        roomService_->setMicEnabled(value);
    }
    emit micEnabledChanged();
}

bool RoomController::cameraEnabled() const {
    return cameraEnabled_;
}

void RoomController::setCameraEnabled(bool value) {
    if (cameraEnabled_ == value) {
        return;
    }
    cameraEnabled_ = value;
    if (roomService_) {
        roomService_->setCameraEnabled(value);
    }
    emit cameraEnabledChanged();
}

QString RoomController::stateText() const {
    return stateText_;
}

void RoomController::join(const QString& liveKitUrl, const QString& token) {
    if (roomService_) {
        roomService_->connect(liveKitUrl, token, micEnabled_, cameraEnabled_);
    }
    stateText_ = QStringLiteral("MVP: 已触发连接（占位）");
    emit stateTextChanged();
}

void RoomController::leave() {
    if (roomService_) {
        roomService_->disconnect();
    }
    stateText_ = QStringLiteral("已离开房间");
    emit stateTextChanged();
}

}  // namespace presentation::controllers
