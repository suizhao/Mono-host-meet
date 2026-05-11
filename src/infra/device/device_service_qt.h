#pragma once

#include "domain/services/device_service.h"

#include <QObject>

class DeviceServiceQt : public QObject, public IDeviceService {
  Q_OBJECT

public:
  explicit DeviceServiceQt(QObject* parent = nullptr);

  QVariantList listAudioInputs() override;
  QVariantList listVideoInputs() override;
  QVariantList listAudioOutputs() override;
  void selectAudioInput(const QString& deviceId) override;
  void selectVideoInput(const QString& deviceId) override;
  void selectAudioOutput(const QString& deviceId) override;
  QString selectedAudioInputId() const override;
  QString selectedVideoInputId() const override;
  QString selectedAudioOutputId() const override;

private:
  QVariantList listAudioInputsFallback() const;
  QVariantList listVideoInputsFallback() const;
  QVariantList listAudioOutputsFallback() const;

  QString m_selectedAudioInputId;
  QString m_selectedVideoInputId;
  QString m_selectedAudioOutputId;
};
