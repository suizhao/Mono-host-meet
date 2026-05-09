#pragma once

#include "domain/services/device_service.h"

#include <QObject>

namespace infra::device {

class DeviceServiceQt final : public QObject, public domain::services::IDeviceService {
    Q_OBJECT

public:
    explicit DeviceServiceQt(QObject* parent = nullptr);

    QVariantList listAudioInputs() override;
    QVariantList listVideoInputs() override;
    QVariantList listAudioOutputs() override;

    void selectAudioInput(const QString& deviceId) override;
    void selectVideoInput(const QString& deviceId) override;
    void selectAudioOutput(const QString& deviceId) override;
};

}  // namespace infra::device
