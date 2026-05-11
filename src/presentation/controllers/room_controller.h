#pragma once

#include "presentation/controllers/video_tile_model.h"
#include "domain/services/device_service.h"
#include "domain/services/room_service.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QTimer>
#include <QVariantList>

class FrameImageProvider;

class RoomController : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool micEnabled READ micEnabled NOTIFY micEnabledChanged)
  Q_PROPERTY(bool cameraEnabled READ cameraEnabled NOTIFY cameraEnabledChanged)
  Q_PROPERTY(bool screenShareEnabled READ screenShareEnabled NOTIFY screenShareEnabledChanged)
  Q_PROPERTY(QString micToggleText READ micToggleText NOTIFY micToggleTextChanged)
  Q_PROPERTY(QString cameraToggleText READ cameraToggleText NOTIFY cameraToggleTextChanged)
  Q_PROPERTY(QString screenShareToggleText READ screenShareToggleText NOTIFY screenShareToggleTextChanged)
  Q_PROPERTY(QString trackStatusText READ trackStatusText NOTIFY trackStatusTextChanged)
  Q_PROPERTY(QString trackErrorText READ trackErrorText NOTIFY trackErrorTextChanged)
  Q_PROPERTY(bool hasTrackError READ hasTrackError NOTIFY trackErrorVisibilityChanged)
  Q_PROPERTY(QString remoteParticipantsText READ remoteParticipantsText NOTIFY remoteParticipantsTextChanged)
  Q_PROPERTY(QObject* remoteVideoTileModel READ remoteVideoTileModel CONSTANT)
  Q_PROPERTY(int remoteVideoTileCount READ remoteVideoTileCount NOTIFY remoteVideoTilesChanged)
  Q_PROPERTY(QVariantList remoteVideoTiles READ remoteVideoTiles NOTIFY remoteVideoTilesChanged)
  Q_PROPERTY(QString remoteVideoDataUrl READ remoteVideoDataUrl NOTIFY remoteVideoDataUrlChanged)
  Q_PROPERTY(QString remoteVideoSourceText READ remoteVideoSourceText NOTIFY remoteVideoSourceTextChanged)
  Q_PROPERTY(bool hasRemoteVideo READ hasRemoteVideo NOTIFY hasRemoteVideoChanged)
  Q_PROPERTY(QVariantList audioInputDevices READ audioInputDevices NOTIFY audioInputDevicesChanged)
  Q_PROPERTY(QVariantList videoInputDevices READ videoInputDevices NOTIFY videoInputDevicesChanged)
  Q_PROPERTY(QVariantList audioOutputDevices READ audioOutputDevices NOTIFY audioOutputDevicesChanged)
  Q_PROPERTY(QString deviceStatusText READ deviceStatusText NOTIFY deviceStatusTextChanged)

public:
  explicit RoomController(IRoomService* roomService, IDeviceService* deviceService,
                          FrameImageProvider* frameImageProvider,
                          QObject* parent = nullptr);

  bool micEnabled() const;
  bool cameraEnabled() const;
  bool screenShareEnabled() const;
  QString micToggleText() const;
  QString cameraToggleText() const;
  QString screenShareToggleText() const;
  QString trackStatusText() const;
  QString trackErrorText() const;
  bool hasTrackError() const;
  QString remoteParticipantsText() const;
  QObject* remoteVideoTileModel() const;
  int remoteVideoTileCount() const;
  QVariantList remoteVideoTiles() const;
  QString remoteVideoDataUrl() const;
  QString remoteVideoSourceText() const;
  bool hasRemoteVideo() const;
  QVariantList audioInputDevices() const;
  QVariantList videoInputDevices() const;
  QVariantList audioOutputDevices() const;
  QString deviceStatusText() const;

  Q_INVOKABLE void toggleMic();
  Q_INVOKABLE void toggleCamera();
  Q_INVOKABLE void toggleScreenShare();
  Q_INVOKABLE void refreshDevices();
  Q_INVOKABLE void selectAudioInput(const QString& deviceId);
  Q_INVOKABLE void selectVideoInput(const QString& deviceId);
  Q_INVOKABLE void selectAudioOutput(const QString& deviceId);
  Q_INVOKABLE void resetSessionUiState();

signals:
  void micEnabledChanged();
  void cameraEnabledChanged();
  void screenShareEnabledChanged();
  void micToggleTextChanged();
  void cameraToggleTextChanged();
  void screenShareToggleTextChanged();
  void trackStatusTextChanged();
  void trackErrorTextChanged();
  void trackErrorVisibilityChanged();
  void remoteParticipantsTextChanged();
  void remoteVideoTilesChanged();
  void remoteVideoDataUrlChanged();
  void remoteVideoSourceTextChanged();
  void hasRemoteVideoChanged();
  void audioInputDevicesChanged();
  void videoInputDevicesChanged();
  void audioOutputDevicesChanged();
  void deviceStatusTextChanged();

private:
  void refreshDisplayTexts();
  void refreshRemoteDisplayState();
  void setTrackError(const QString& message);

  IRoomService* m_roomService;
  IDeviceService* m_deviceService;
  FrameImageProvider* m_frameImageProvider;
  bool m_micEnabled = true;
  bool m_cameraEnabled = true;
  bool m_screenShareEnabled = false;
  QString m_micToggleText;
  QString m_cameraToggleText;
  QString m_screenShareToggleText;
  QString m_trackStatusText;
  QString m_trackErrorText;
  QString m_remoteParticipantsText = "远端参与者：0";
  VideoTileModel m_remoteVideoTileModel;
  QVariantList m_remoteVideoTiles;
  QHash<QString, int> m_tileFrameVersions;
  QString m_remoteVideoDataUrl;
  QString m_remoteVideoSourceText = "当前画面来源：无";
  QVariantList m_audioInputDevices;
  QVariantList m_videoInputDevices;
  QVariantList m_audioOutputDevices;
  QString m_deviceStatusText = "设备状态：未选择";
  QTimer m_remoteRefreshTimer;
};
