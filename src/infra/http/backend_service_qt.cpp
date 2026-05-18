#include "infra/http/backend_service_qt.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

#include <functional>

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
  const bool isHost = obj.value("isHost").toBool(false);

  if (serverUrl.isEmpty() || room.isEmpty() || participant.isEmpty() ||
      token.isEmpty()) {
    emit backendErrorOccurred(operation, statusCode,
                              "响应缺少必要字段(serverUrl/roomName/participantName/participantToken)");
    return;
  }

  emit connectionDetailsReady(serverUrl, room, participant, token, isHost);
}

void BackendServiceQt::startRecording(const QString& roomName,
                                       const QString& participantName,
                                       const QStringList& audioTrackSids,
                                       const QStringList& videoTrackSids) {
  QJsonObject body;
  body["roomName"] = roomName;
  body["participantName"] = participantName;
  QJsonArray audioArr;
  for (const auto& s : audioTrackSids) audioArr.push_back(s);
  body["audioTrackSids"] = audioArr;
  QJsonArray videoArr;
  for (const auto& s : videoTrackSids) videoArr.push_back(s);
  body["videoTrackSids"] = videoArr;

  postJson("/api/recording/start", body,
           [this, roomName](const QJsonDocument& doc, int statusCode) {
             if (statusCode >= 400) {
               const QString msg = doc.isObject()
                                       ? doc.object().value("message").toString()
                                       : "开始录制失败";
               emit backendErrorOccurred("record-start", statusCode, msg);
               return;
             }
             const QString egressId = doc.isObject()
                                          ? doc.object().value("egressId").toString()
                                          : QString();
             emit recordingStarted(roomName, egressId);
           });
}

void BackendServiceQt::stopRecording(const QString& roomName,
                                      const QString& participantName) {
  QJsonObject body;
  body["roomName"] = roomName;
  body["participantName"] = participantName;

  postJson("/api/recording/stop", body,
           [this, roomName](const QJsonDocument& doc, int statusCode) {
             if (statusCode >= 400) {
               const QString msg = doc.isObject()
                                       ? doc.object().value("message").toString()
                                       : "停止录制失败";
               emit backendErrorOccurred("record-stop", statusCode, msg);
               return;
             }
             emit recordingStopped(roomName);
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

void BackendServiceQt::postJson(
    const QString& path, const QJsonObject& body,
    std::function<void(const QJsonDocument&, int)> onReply) {
  QUrl url(buildUrl(path));
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  const QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
  const auto reply = m_networkAccessManager.post(request, data);
  connect(reply, &QNetworkReply::finished, this, [this, reply, onReply]() {
    const QByteArray responseBody = reply->readAll();
    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto doc = QJsonDocument::fromJson(responseBody);
    onReply(doc, statusCode);
    reply->deleteLater();
  });
}

void BackendServiceQt::disbandRoom(const QString& roomName,
                                   const QString& participantName) {
  QJsonObject body;
  body["roomName"] = roomName;
  body["participantName"] = participantName;

  postJson("/api/room/disband", body,
           [this, roomName](const QJsonDocument& doc, int statusCode) {
             if (statusCode >= 400) {
               const QString msg = doc.isObject()
                                       ? doc.object().value("message").toString()
                                       : "解散房间失败";
               emit backendErrorOccurred("disband-room", statusCode, msg);
               return;
             }
             emit roomDisbanded(roomName);
           });
}

void BackendServiceQt::muteAllParticipants(const QString& roomName,
                                           const QString& participantName) {
  QJsonObject body;
  body["roomName"] = roomName;
  body["participantName"] = participantName;

  postJson("/api/room/mute-all", body,
           [this, roomName](const QJsonDocument& doc, int statusCode) {
             if (statusCode >= 400) {
               const QString msg = doc.isObject()
                                       ? doc.object().value("message").toString()
                                       : "全员静音失败";
               emit backendErrorOccurred("mute-all", statusCode, msg);
               return;
             }
             const int count =
                 doc.isObject() ? doc.object().value("mutedCount").toInt(0) : 0;
             emit muteAllCompleted(roomName, count);
           });
}
