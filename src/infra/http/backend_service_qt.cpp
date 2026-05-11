#include "infra/http/backend_service_qt.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

BackendServiceQt::BackendServiceQt(QString baseUrl, QObject* parent)
    : IBackendService(parent), m_baseUrl(std::move(baseUrl)) {}

void BackendServiceQt::getConnectionDetails(const QString& roomName,
                                            const QString& participantName,
                                            const QString& region,
                                            const QString& metadata) {
  QUrl url(buildUrl("/api/connection-details"));
  QUrlQuery query;
  query.addQueryItem("roomName", roomName);
  query.addQueryItem("participantName", participantName);
  if (!region.isEmpty()) {
    query.addQueryItem("region", region);
  }
  if (!metadata.isEmpty()) {
    query.addQueryItem("metadata", metadata);
  }
  url.setQuery(query);

  QNetworkRequest request(url);
  const auto reply = m_networkAccessManager.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray responseBody = reply->readAll();
    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    parseConnectionDetailsResponse("connection-details", responseBody, statusCode,
                                   reply->errorString());
    reply->deleteLater();
  });
}

void BackendServiceQt::createRoom(const QString& participantName) {
  QUrl url(buildUrl("/api/rooms"));
  QUrlQuery query;
  query.addQueryItem("participantName", participantName);
  url.setQuery(query);

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  const auto reply = m_networkAccessManager.post(request, QByteArray());
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray responseBody = reply->readAll();
    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    parseConnectionDetailsResponse("room-create", responseBody, statusCode,
                                   reply->errorString());
    reply->deleteLater();
  });
}

void BackendServiceQt::parseConnectionDetailsResponse(
    const QString& operation, const QByteArray& responseBody, int statusCode,
    const QString& errorString) {
  if (statusCode >= 400) {
    emitNetworkError(operation, statusCode, errorString, responseBody);
    return;
  }

  const auto doc = QJsonDocument::fromJson(responseBody);
  if (!doc.isObject()) {
    emit backendErrorOccurred(operation, statusCode,
                              "响应不是合法 JSON 对象");
    return;
  }

  const QJsonObject obj = doc.object();
  const QString serverUrl = obj.value("serverUrl").toString();
  const QString room = obj.value("roomName").toString();
  const QString participant = obj.value("participantName").toString();
  const QString token = obj.value("participantToken").toString();

  if (serverUrl.isEmpty() || room.isEmpty() || participant.isEmpty() ||
      token.isEmpty()) {
    emit backendErrorOccurred(operation, statusCode,
                              "响应缺少必要字段(serverUrl/roomName/participantName/participantToken)");
    return;
  }

  emit connectionDetailsReady(serverUrl, room, participant, token);
}

void BackendServiceQt::startRecording(const QString& roomName) {
  QUrl url(buildUrl("/api/record/start"));
  QUrlQuery query;
  query.addQueryItem("roomName", roomName);
  url.setQuery(query);

  QNetworkRequest request(url);
  const auto reply = m_networkAccessManager.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, roomName]() {
    const QByteArray responseBody = reply->readAll();
    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString operation = "record-start";

    if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
      emitNetworkError(operation, statusCode, reply->errorString(), responseBody);
      reply->deleteLater();
      return;
    }

    emit recordingOperationSucceeded(operation, roomName);
    reply->deleteLater();
  });
}

void BackendServiceQt::stopRecording(const QString& roomName) {
  QUrl url(buildUrl("/api/record/stop"));
  QUrlQuery query;
  query.addQueryItem("roomName", roomName);
  url.setQuery(query);

  QNetworkRequest request(url);
  const auto reply = m_networkAccessManager.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, roomName]() {
    const QByteArray responseBody = reply->readAll();
    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString operation = "record-stop";

    if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
      emitNetworkError(operation, statusCode, reply->errorString(), responseBody);
      reply->deleteLater();
      return;
    }

    emit recordingOperationSucceeded(operation, roomName);
    reply->deleteLater();
  });
}

QString BackendServiceQt::buildUrl(const QString& path) const {
  QString normalizedBase = m_baseUrl;
  if (normalizedBase.endsWith('/')) {
    normalizedBase.chop(1);
  }

  QString normalizedPath = path;
  if (!normalizedPath.startsWith('/')) {
    normalizedPath.prepend('/');
  }

  return normalizedBase + normalizedPath;
}

void BackendServiceQt::emitNetworkError(const QString& operation, int statusCode,
                                        const QString& fallbackMessage,
                                        const QByteArray& responseBody) {
  const auto doc = QJsonDocument::fromJson(responseBody);
  QString message;
  if (doc.isObject()) {
    message = doc.object().value("message").toString();
  }

  if (message.isEmpty()) {
    message = fallbackMessage.isEmpty() ? "未知后端错误" : fallbackMessage;
  }

  emit backendErrorOccurred(operation, statusCode, message);
}
