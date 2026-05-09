#pragma once

#include "domain/services/room_service.h"

#include <QObject>
#include <QString>

namespace infra::rtc {

class LiveKitRoomService final : public QObject, public domain::services::IRoomService {
    Q_OBJECT

public:
    explicit LiveKitRoomService(QObject* parent = nullptr);

    void connect(
        const QString& serverUrl,
        const QString& token,
        bool audioEnabled,
        bool videoEnabled,
        const QString& audioDeviceId = QString(),
        const QString& videoDeviceId = QString(),
        const QString& e2eePassphrase = QString()) override;
    void disconnect() override;
    void setMicEnabled(bool enabled) override;
    void setCameraEnabled(bool enabled) override;

signals:
    void connectionStateChanged(bool connected);

private:
    bool connected_ = false;
};

}  // namespace infra::rtc
