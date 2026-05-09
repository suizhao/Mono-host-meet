#include "presentation/controllers/home_controller.h"

namespace presentation::controllers {

HomeController::HomeController(QObject* parent)
    : QObject(parent) {}

QString HomeController::participantName() const {
    return participantName_;
}

void HomeController::setParticipantName(const QString& value) {
    if (participantName_ == value) {
        return;
    }
    participantName_ = value;
    emit participantNameChanged();
}

QString HomeController::roomName() const {
    return roomName_;
}

void HomeController::setRoomName(const QString& value) {
    if (roomName_ == value) {
        return;
    }
    roomName_ = value;
    emit roomNameChanged();
}

QString HomeController::statusText() const {
    return statusText_;
}

void HomeController::startDemo() {
    statusText_ = QStringLiteral("MVP: 已触发 Demo 入会流程（下一步接 PreJoin）");
    emit statusTextChanged();
    emit requestPreJoin(roomName_, participantName_);
}

void HomeController::startCustom(const QString& liveKitUrl, const QString& token) {
    statusText_ = QStringLiteral("MVP: 已触发 Custom 入会流程（下一步接真实连接）");
    emit statusTextChanged();
    emit requestCustomRoom(liveKitUrl, token, participantName_);
}

}  // namespace presentation::controllers
