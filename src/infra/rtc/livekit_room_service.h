#pragma once

#include "domain/services/room_service.h"
#include <livekit/room_delegate.h>
#include <livekit/track.h>

#include <QBuffer>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QtGlobal>
#include <QVariantList>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

class QAudioSink;
class QAudioSource;
class QIODevice;
class QTimer;

namespace livekit {
class Room;
class LocalVideoTrack;
class LocalAudioTrack;
class VideoSource;
class AudioSource;
}

class LiveKitRoomService : public IRoomService, public livekit::RoomDelegate {
  Q_OBJECT
public:
  LiveKitRoomService();
  ~LiveKitRoomService() override;

  bool connect(const QString& serverUrl,
               const QString& token,
               bool audioEnabled,
               bool videoEnabled,
               const QString& audioDeviceId = "",
               const QString& videoDeviceId = "",
               const QString& e2eePassphrase = "") override;
  void disconnect() override;
  void setAudioOutputDevice(const QString& deviceId) override;
  bool setMicEnabled(bool enabled,
                      const QString& audioDeviceId = "") override;
  bool setCameraEnabled(bool enabled) override;
  bool setScreenShareEnabled(bool enabled) override;
  bool screenShareEnabled() const override;
  void refreshRemoteState() override;
  QString remoteParticipantsText() const override;
  QString remoteVideoDataUrl() const override;
  QVariantList remoteVideoTiles() const override;
  QString remoteVideoSourceText() const override;
  QString lastError() const override;

  // livekit::RoomDelegate — event-driven updates
  void onParticipantConnected(
      livekit::Room&,
      const livekit::ParticipantConnectedEvent& event) override;
  void onParticipantDisconnected(
      livekit::Room&,
      const livekit::ParticipantDisconnectedEvent& event) override;
  void onTrackUnpublished(livekit::Room&,
                          const livekit::TrackUnpublishedEvent& event) override;
  void onTrackSubscribed(livekit::Room&,
                         const livekit::TrackSubscribedEvent& event) override;

private slots:
  void doHandleParticipantConnected();
  void doHandleParticipantDisconnected(const QString& identity);
  void doHandleTrackUnpublished(const QString& identity,
                                livekit::TrackSource source);
  void doHandleTrackSubscribed();

private:
  struct StreamBinding {
    QString key;
    QString identity;
    QString displayName;
    livekit::TrackSource source = livekit::TrackSource::SOURCE_CAMERA;
    bool isLocal = false;
  };

  struct VideoTile {
    QString tileId;
    QString identity;
    QString displayName;
    QString sourceText;
    QString dataUrl;
    bool isLocal = false;
    bool micActive = false;
    qint64 lastFrameMs = 0;
  };

  struct RemoteState {
    QMutex mutex;
    QString participantsText = "远端参与者：0";
    QString videoSourceText = "当前画面来源：无";
    QString videoDataUrl;
    QHash<QString, VideoTile> tiles;
  };

  std::unique_ptr<livekit::Room> m_room;
  bool m_livekitInitialized = false;
  bool m_connected = false;
  QString m_lastError;
  std::shared_ptr<RemoteState> m_remoteState = std::make_shared<RemoteState>();
  QHash<QString, StreamBinding> m_streamBindings;
  mutable QMutex m_streamBindingsMutex;
  QSet<QString> m_remoteIdsHadTracks;

  // Mic
  void startMicCapture(const QString& deviceId);
  void stopMicCapture();
  void startCameraThread(const QString& cameraTileKey);

  std::shared_ptr<livekit::AudioSource> m_micSource;
  std::shared_ptr<livekit::LocalAudioTrack> m_micTrack;
  std::atomic<bool> m_micActive{false};
  std::thread m_micThread;
  QString m_audioInputDeviceId;

  // Audio
  QSet<QString> m_audioCallbackIds;

  // Audio output (speaker)
  void setupRemoteAudioPlayback();
  QString m_audioOutputDeviceId;
  std::unique_ptr<QAudioSink> m_audioOutput;
  QIODevice* m_audioOutputIO = nullptr;

private:
  // Camera
  std::shared_ptr<livekit::VideoSource> m_cameraSource;
  std::shared_ptr<livekit::LocalVideoTrack> m_cameraTrack;
  std::atomic<bool> m_cameraActive{false};
  std::thread m_cameraThread;

  // Screen share
  std::shared_ptr<livekit::VideoSource> m_screenShareSource;
  std::shared_ptr<livekit::LocalVideoTrack> m_screenShareTrack;
  std::atomic<bool> m_screenShareRunning{false};
  std::thread m_screenShareThread;
  mutable std::mutex m_screenShareMutex;
};
