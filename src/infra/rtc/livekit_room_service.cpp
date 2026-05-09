#include "infra/rtc/livekit_room_service.h"

#include <QDebug>

namespace infra::rtc {

LiveKitRoomService::LiveKitRoomService(QObject* parent)
    : QObject(parent) {}

void LiveKitRoomService::connect(
    const QString& serverUrl,
    const QString& token,
    bool audioEnabled,
    bool videoEnabled,
    const QString& audioDeviceId,
    const QString& videoDeviceId,
    const QString& e2eePassphrase) {
    Q_UNUSED(token);
    Q_UNUSED(audioEnabled);
    Q_UNUSED(videoEnabled);
    Q_UNUSED(audioDeviceId);
    Q_UNUSED(videoDeviceId);
    Q_UNUSED(e2eePassphrase);

    qInfo().noquote() << QStringLiteral("[MVP][RoomService] connect serverUrl=%1").arg(serverUrl);
    connected_ = true;
    emit connectionStateChanged(connected_);
}

void LiveKitRoomService::disconnect() {
    qInfo().noquote() << QStringLiteral("[MVP][RoomService] disconnect");
    connected_ = false;
    emit connectionStateChanged(connected_);
}

void LiveKitRoomService::setMicEnabled(bool enabled) {
    qInfo().noquote() << QStringLiteral("[MVP][RoomService] setMicEnabled=%1")
                             .arg(enabled ? "true" : "false");
}

void LiveKitRoomService::setCameraEnabled(bool enabled) {
    qInfo().noquote() << QStringLiteral("[MVP][RoomService] setCameraEnabled=%1")
                             .arg(enabled ? "true" : "false");
}

}  // namespace infra::rtc
