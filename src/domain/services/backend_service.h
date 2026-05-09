#pragma once

#include <QString>

namespace domain::services {

class IBackendService {
public:
    virtual ~IBackendService() = default;

    virtual void getConnectionDetails(
        const QString& roomName,
        const QString& participantName,
        const QString& region = QString(),
        const QString& metadata = QString()) = 0;

    virtual void startRecording(const QString& roomName) = 0;
    virtual void stopRecording(const QString& roomName) = 0;
};

}  // namespace domain::services
