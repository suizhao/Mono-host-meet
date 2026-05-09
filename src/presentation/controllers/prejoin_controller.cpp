#include "presentation/controllers/prejoin_controller.h"

#include "domain/services/backend_service.h"

namespace presentation::controllers {

PrejoinController::PrejoinController(domain::services::IBackendService* backendService, QObject* parent)
    : QObject(parent), backendService_(backendService) {}

bool PrejoinController::audioEnabled() const {
    return audioEnabled_;
}

void PrejoinController::setAudioEnabled(bool value) {
    if (audioEnabled_ == value) {
        return;
    }
    audioEnabled_ = value;
    emit audioEnabledChanged();
}

bool PrejoinController::videoEnabled() const {
    return videoEnabled_;
}

void PrejoinController::setVideoEnabled(bool value) {
    if (videoEnabled_ == value) {
        return;
    }
    videoEnabled_ = value;
    emit videoEnabledChanged();
}

QString PrejoinController::lastAction() const {
    return lastAction_;
}

void PrejoinController::prepareJoin(const QString& roomName, const QString& participantName) {
    if (backendService_) {
        backendService_->getConnectionDetails(roomName, participantName);
    }
    lastAction_ = QStringLiteral("已请求 connection-details（MVP 占位）");
    emit lastActionChanged();
}

}  // namespace presentation::controllers
