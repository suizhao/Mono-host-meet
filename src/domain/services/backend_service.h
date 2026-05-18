#pragma once

#include <QString>
#include <QObject>

class IBackendService : public QObject {
  Q_OBJECT

public:
  explicit IBackendService(QObject* parent = nullptr) : QObject(parent) {}
  virtual ~IBackendService() = default;

  virtual void getConnectionDetails(const QString& roomName,
                                    const QString& participantName,
                                    const QString& region = "",
                                    const QString& metadata = "") = 0;
  virtual void createRoom(const QString& participantName) = 0;
  virtual void startRecording(const QString& roomName,
                              const QString& participantName,
                              const QStringList& audioTrackSids = {},
                              const QStringList& videoTrackSids = {}) = 0;
  virtual void stopRecording(const QString& roomName,
                             const QString& participantName) = 0;
  virtual void disbandRoom(const QString& roomName,
                           const QString& participantName) = 0;
  virtual void muteAllParticipants(const QString& roomName,
                                   const QString& participantName) = 0;

signals:
  void connectionDetailsReady(const QString& serverUrl, const QString& roomName,
                              const QString& participantName,
                              const QString& participantToken,
                              bool isHost);
  void recordingStarted(const QString& roomName, const QString& egressId);
  void recordingStopped(const QString& roomName);
  void backendErrorOccurred(const QString& operation, int statusCode,
                            const QString& message);
  void roomDisbanded(const QString& roomName);
  void muteAllCompleted(const QString& roomName, int mutedCount);
};
