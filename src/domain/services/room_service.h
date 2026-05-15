#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class IRoomService : public QObject {
  Q_OBJECT
public:
  explicit IRoomService(QObject* parent = nullptr) : QObject(parent) {}
  ~IRoomService() override = default;

  virtual bool connect(const QString& serverUrl,
                       const QString& token,
                       bool audioEnabled,
                       bool videoEnabled,
                       const QString& audioDeviceId = "",
                       const QString& videoDeviceId = "",
                       const QString& e2eePassphrase = "") = 0;
  virtual void disconnect() = 0;
  virtual void setAudioOutputDevice(const QString& deviceId) = 0;
  virtual bool setMicEnabled(bool enabled,
                              const QString& audioDeviceId = "") = 0;
  virtual bool setCameraEnabled(bool enabled) = 0;
  virtual bool setScreenShareEnabled(bool enabled) = 0;
  virtual bool screenShareEnabled() const = 0;
  virtual void refreshRemoteState() = 0;
  virtual QString remoteParticipantsText() const = 0;
  virtual QString remoteVideoDataUrl() const = 0;
  virtual QVariantList remoteVideoTiles() const = 0;
  virtual QString remoteVideoSourceText() const = 0;
  virtual QString lastError() const = 0;

signals:
  void remoteStateDirty();
};
