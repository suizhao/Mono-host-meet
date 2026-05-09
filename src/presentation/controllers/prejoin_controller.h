#pragma once

#include <QObject>
#include <QString>

namespace domain::services {
class IBackendService;
}

namespace presentation::controllers {

class PrejoinController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool audioEnabled READ audioEnabled WRITE setAudioEnabled NOTIFY audioEnabledChanged)
    Q_PROPERTY(bool videoEnabled READ videoEnabled WRITE setVideoEnabled NOTIFY videoEnabledChanged)
    Q_PROPERTY(QString lastAction READ lastAction NOTIFY lastActionChanged)

public:
    explicit PrejoinController(domain::services::IBackendService* backendService, QObject* parent = nullptr);

    [[nodiscard]] bool audioEnabled() const;
    void setAudioEnabled(bool value);

    [[nodiscard]] bool videoEnabled() const;
    void setVideoEnabled(bool value);

    [[nodiscard]] QString lastAction() const;

    Q_INVOKABLE void prepareJoin(const QString& roomName, const QString& participantName);

signals:
    void audioEnabledChanged();
    void videoEnabledChanged();
    void lastActionChanged();

private:
    domain::services::IBackendService* backendService_ = nullptr;
    bool audioEnabled_ = true;
    bool videoEnabled_ = true;
    QString lastAction_ = QStringLiteral("尚未请求 connection-details");
};

}  // namespace presentation::controllers
