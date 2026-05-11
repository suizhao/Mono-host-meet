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
  virtual void startRecording(const QString& roomName) = 0;
  virtual void stopRecording(const QString& roomName) = 0;

signals:
  void connectionDetailsReady(const QString& serverUrl, const QString& roomName,
                              const QString& participantName,
                              const QString& participantToken);
  void recordingOperationSucceeded(const QString& operation,
                                   const QString& roomName);
  void backendErrorOccurred(const QString& operation, int statusCode,
                            const QString& message);
};
