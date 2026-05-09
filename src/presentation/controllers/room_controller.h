#pragma once

#include <QObject>
#include <QString>

namespace domain::services {
class IRoomService;
}

namespace presentation::controllers {

class RoomController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool micEnabled READ micEnabled WRITE setMicEnabled NOTIFY micEnabledChanged)
    Q_PROPERTY(bool cameraEnabled READ cameraEnabled WRITE setCameraEnabled NOTIFY cameraEnabledChanged)
    Q_PROPERTY(QString stateText READ stateText NOTIFY stateTextChanged)

public:
    explicit RoomController(domain::services::IRoomService* roomService, QObject* parent = nullptr);

    [[nodiscard]] bool micEnabled() const;
    void setMicEnabled(bool value);

    [[nodiscard]] bool cameraEnabled() const;
    void setCameraEnabled(bool value);

    [[nodiscard]] QString stateText() const;

    Q_INVOKABLE void join(const QString& liveKitUrl, const QString& token);
    Q_INVOKABLE void leave();

signals:
    void micEnabledChanged();
    void cameraEnabledChanged();
    void stateTextChanged();

private:
    domain::services::IRoomService* roomService_ = nullptr;
    bool micEnabled_ = true;
    bool cameraEnabled_ = true;
    QString stateText_ = QStringLiteral("未连接");
};

}  // namespace presentation::controllers
