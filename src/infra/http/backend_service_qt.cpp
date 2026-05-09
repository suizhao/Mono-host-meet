#include "infra/http/backend_service_qt.h"

#include <QDebug>

namespace infra::http {

BackendServiceQt::BackendServiceQt(QString apiBaseUrl, QObject* parent)
    : QObject(parent), apiBaseUrl_(std::move(apiBaseUrl)) {}

void BackendServiceQt::getConnectionDetails(
    const QString& roomName,
    const QString& participantName,
    const QString& region,
    const QString& metadata) {
    Q_UNUSED(roomName);
    Q_UNUSED(participantName);
    Q_UNUSED(region);
    Q_UNUSED(metadata);

    qInfo().noquote() << QStringLiteral("[MVP][BackendService] getConnectionDetails baseUrl=%1")
                             .arg(apiBaseUrl_);
    emit requestAccepted(QStringLiteral("connection-details"));
}

void BackendServiceQt::startRecording(const QString& roomName) {
    Q_UNUSED(roomName);
    qInfo().noquote() << QStringLiteral("[MVP][BackendService] startRecording baseUrl=%1")
                             .arg(apiBaseUrl_);
    emit requestAccepted(QStringLiteral("record-start"));
}

void BackendServiceQt::stopRecording(const QString& roomName) {
    Q_UNUSED(roomName);
    qInfo().noquote() << QStringLiteral("[MVP][BackendService] stopRecording baseUrl=%1")
                             .arg(apiBaseUrl_);
    emit requestAccepted(QStringLiteral("record-stop"));
}

}  // namespace infra::http
