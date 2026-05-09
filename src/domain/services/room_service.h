#pragma once

#include <QString>

namespace domain::services {

class IRoomService {
public:
    virtual ~IRoomService() = default;

    virtual void connect(
        const QString& serverUrl,
        const QString& token,
        bool audioEnabled,
        bool videoEnabled,
        const QString& audioDeviceId = QString(),
        const QString& videoDeviceId = QString(),
        const QString& e2eePassphrase = QString()) = 0;

    virtual void disconnect() = 0;
    virtual void setMicEnabled(bool enabled) = 0;
    virtual void setCameraEnabled(bool enabled) = 0;
};

}  // namespace domain::services
