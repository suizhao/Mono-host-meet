#pragma once

#include "domain/services/device_service.h"

#include <QAudioDevice>
#include <QObject>

class DeviceServiceQt : public QObject, public IDeviceService {
  Q_OBJECT

public:
  explicit DeviceServiceQt(QObject* parent = nullptr);

  Q_INVOKABLE void initialize();
  bool isReady() const;

  QVariantList listAudioInputs() override;
  QVariantList listVideoInputs() override;
  QVariantList listAudioOutputs() override;
  void selectAudioInput(const QString& deviceId) override;
  void selectVideoInput(const QString& deviceId) override;
  void selectAudioOutput(const QString& deviceId) override;
  QString selectedAudioInputId() const override;
  QString selectedVideoInputId() const override;
  QString selectedAudioOutputId() const override;

  QAudioDevice resolveAudioInputDevice(const QString& deviceId) const;

signals:
  void devicesReady();

private:
  void runProbeProcess();
  void onProbeFinished();
  QVariantList parseDeviceProbeResult(const QByteArray& jsonBytes) const;
  QVariantList parseDeviceProbeResult(const QJsonArray& arr) const;

  QVariantList listAudioInputsFallback() const;
  QVariantList listVideoInputsFallback() const;
  QVariantList listAudioOutputsFallback() const;

  QString m_selectedAudioInputId;
  QString m_selectedVideoInputId;
  QString m_selectedAudioOutputId;

  QVariantList m_cachedAudioInputs;
  QVariantList m_cachedVideoInputs;
  QVariantList m_cachedAudioOutputs;
  bool m_ready = false;
  bool m_probeRunning = false;

  class QProcess* m_probeProcess = nullptr;
};
