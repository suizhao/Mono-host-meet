#pragma once

#include <QObject>
#include <QString>

namespace presentation::controllers {

class HomeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString participantName READ participantName WRITE setParticipantName NOTIFY participantNameChanged)
    Q_PROPERTY(QString roomName READ roomName WRITE setRoomName NOTIFY roomNameChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit HomeController(QObject* parent = nullptr);

    [[nodiscard]] QString participantName() const;
    void setParticipantName(const QString& value);

    [[nodiscard]] QString roomName() const;
    void setRoomName(const QString& value);

    [[nodiscard]] QString statusText() const;

    Q_INVOKABLE void startDemo();
    Q_INVOKABLE void startCustom(const QString& liveKitUrl, const QString& token);

signals:
    void participantNameChanged();
    void roomNameChanged();
    void statusTextChanged();
    void requestPreJoin(const QString& roomName, const QString& participantName);
    void requestCustomRoom(const QString& liveKitUrl, const QString& token, const QString& participantName);

private:
    QString participantName_ = QStringLiteral("guest");
    QString roomName_ = QStringLiteral("demo-room");
    QString statusText_ = QStringLiteral("就绪：项目骨架已加载");
};

}  // namespace presentation::controllers
