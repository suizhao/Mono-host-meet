#include "presentation/controllers/prejoin_controller.h"

PrejoinController::PrejoinController(QObject* parent) : QObject(parent) {}

QString PrejoinController::participantName() const { return m_participantName; }

void PrejoinController::setParticipantName(const QString& participantName) {
  if (m_participantName == participantName) {
    return;
  }

  m_participantName = participantName;
  emit participantNameChanged();
}
