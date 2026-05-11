#pragma once

#include <QObject>
#include <QString>

class PrejoinController : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString participantName READ participantName WRITE setParticipantName NOTIFY participantNameChanged)

public:
  explicit PrejoinController(QObject* parent = nullptr);

  QString participantName() const;
  void setParticipantName(const QString& participantName);

signals:
  void participantNameChanged();

private:
  QString m_participantName = "guest";
};
