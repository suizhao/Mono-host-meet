#pragma once

#include "domain/services/backend_service.h"

#include <QNetworkAccessManager>

class BackendServiceQt : public IBackendService {
public:
  BackendServiceQt(QString baseUrl, QObject* parent = nullptr);

  void getConnectionDetails(const QString& roomName,
                            const QString& participantName,
                            const QString& region = "",
                            const QString& metadata = "") override;
  void createRoom(const QString& participantName) override;
  void startRecording(const QString& roomName) override;
  void stopRecording(const QString& roomName) override;

private:
  QString buildUrl(const QString& path) const;
  void emitNetworkError(const QString& operation, int statusCode,
                        const QString& fallbackMessage,
                        const QByteArray& responseBody);
  void parseConnectionDetailsResponse(const QString& operation,
                                      const QByteArray& responseBody,
                                      int statusCode,
                                      const QString& errorString);

  QString m_baseUrl;
  QNetworkAccessManager m_networkAccessManager;
};
