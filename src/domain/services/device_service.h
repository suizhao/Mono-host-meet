#pragma once

#include <QVariantList>
#include <QString>

namespace domain::services {

class IDeviceService {
public:
    virtual ~IDeviceService() = default;

    virtual QVariantList listAudioInputs() = 0;
    virtual QVariantList listVideoInputs() = 0;
    virtual QVariantList listAudioOutputs() = 0;

    virtual void selectAudioInput(const QString& deviceId) = 0;
    virtual void selectVideoInput(const QString& deviceId) = 0;
    virtual void selectAudioOutput(const QString& deviceId) = 0;
};

}  // namespace domain::services
