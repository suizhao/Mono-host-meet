#pragma once

#include "domain/services/backend_service.h"

#include <QObject>
#include <QString>

namespace infra::http {

class BackendServiceQt final : public QObject, public domain::services::IBackendService {
    Q_OBJECT

public:
    explicit BackendServiceQt(QString apiBaseUrl, QObject* parent = nullptr);

    void getConnectionDetails(
        const QString& roomName,
        const QString& participantName,
        const QString& region = QString(),
        const QString& metadata = QString()) override;
    void startRecording(const QString& roomName) override;
    void stopRecording(const QString& roomName) override;

signals:
    void requestAccepted(const QString& operation);

private:
    QString apiBaseUrl_;
};

}  // namespace infra::http
