#include "presentation/controllers/home_controller.h"



#include <QUuid>



HomeController::HomeController(IBackendService* backendService,

                               IRoomService* roomService, QObject* parent)

    : QObject(parent),

      m_backendService(backendService),

      m_roomService(roomService),

      m_statusMessage("等待开始会议"),

      m_lastRoomName("mvp-room"),

      m_lastParticipantName(

          "guest-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(4)) {

  connect(m_backendService, &IBackendService::connectionDetailsReady, this,
          [this](const QString& serverUrl, const QString& roomName,
                 const QString& participantName, const QString& participantToken) {
            clearErrorBanner();
            m_lastRoomName = roomName;
            setCurrentRoomName(roomName);
            setStatusMessage(QString("已拿到连接详情，正在连接 LiveKit：room=%1, user=%2")
                                 .arg(roomName, participantName));

            const bool connected =

                m_roomService->connect(serverUrl, participantToken, true, true);

            setJoinInProgress(false);

            if (connected) {

              setInRoom(true);

              setStatusMessage(QString("LiveKit 连接成功：%1").arg(serverUrl));

              return;

            }



            setInRoom(false);

            const QString error = QString("LiveKit 连接失败：%1").arg(m_roomService->lastError());

            setErrorBannerText(error);

            setStatusMessage(error);

            emit sessionResetRequested();

          });



  connect(m_backendService, &IBackendService::backendErrorOccurred, this,

          [this](const QString& operation, int statusCode, const QString& message) {
            if (operation == "connection-details" || operation == "room-create") {
              setJoinInProgress(false);
              setInRoom(false);
              emit sessionResetRequested();
            }

            if (operation == "record-start" || operation == "record-stop") {

              if (m_recordingRequestInFlight) {

                m_recordingRequestInFlight = false;

                emit recordingRequestInFlightChanged();

              }

              m_recordingStatusText = QString("录制状态：请求失败（%1）").arg(message);

              emit recordingStatusTextChanged();

            }

            const QString userError = buildUserFacingError(operation, statusCode, message);

            setErrorBannerText(userError);

            setStatusMessage(userError);

          });



  connect(m_backendService, &IBackendService::recordingOperationSucceeded, this,

          [this](const QString& operation, const QString& roomName) {

            if (m_recordingRequestInFlight) {

              m_recordingRequestInFlight = false;

              emit recordingRequestInFlightChanged();

            }

            if (operation == "record-start") {

              if (!m_recordingActive) {

                m_recordingActive = true;

                emit recordingActiveChanged();

              }

              m_recordingStatusText = "录制状态：进行中";

              emit recordingStatusTextChanged();

            } else if (operation == "record-stop") {

              if (m_recordingActive) {

                m_recordingActive = false;

                emit recordingActiveChanged();

              }

              m_recordingStatusText = "录制状态：已停止";

              emit recordingStatusTextChanged();

            }

            clearErrorBanner();

            setStatusMessage(

                QString("录制接口调用成功：op=%1, room=%2").arg(operation, roomName));

          });

}



QString HomeController::statusMessage() const { return m_statusMessage; }

bool HomeController::joinInProgress() const { return m_joinInProgress; }

bool HomeController::inRoom() const { return m_inRoom; }

bool HomeController::hasErrorBanner() const { return !m_errorBannerText.isEmpty(); }

QString HomeController::errorBannerText() const { return m_errorBannerText; }

bool HomeController::recordingActive() const { return m_recordingActive; }

bool HomeController::recordingRequestInFlight() const { return m_recordingRequestInFlight; }

QString HomeController::recordingStatusText() const { return m_recordingStatusText; }

QString HomeController::currentRoomName() const { return m_currentRoomName; }

bool HomeController::prejoinVisible() const { return m_prejoinVisible; }

QString HomeController::defaultParticipantName() const {
  return m_lastParticipantName;
}



void HomeController::startDemoMeeting() {
  if (m_joinInProgress) {
    setStatusMessage("正在入会中，请稍候...");
    return;
  }

  if (m_inRoom) {
    setStatusMessage("你已经在房间里了，请先点击“离开房间（SDK）”再重新入会。");
    return;
  }
  clearErrorBanner();

  const QString roomName =
      "room-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(6);
  const QString participantName = m_lastParticipantName;
  m_lastRoomName = roomName;
  setJoinInProgress(true);
  setInRoom(false);

  m_backendService->getConnectionDetails(roomName, participantName, "", "{}");

  setStatusMessage(QString("正在创建房间，房间=%1，用户=%2").arg(roomName, participantName));
}

void HomeController::openCustomMeeting() {
  if (m_joinInProgress) {
    setStatusMessage("正在入会中，请稍候...");
    return;
  }

  if (m_inRoom) {
    setStatusMessage("你已经在房间里了，请先点击“离开房间（SDK）”再重新入会。");
    return;
  }

  clearErrorBanner();
  setPrejoinVisible(true);
}

void HomeController::confirmCustomJoin(const QString& roomName,
                                       const QString& participantName) {
  if (roomName.trimmed().isEmpty()) {
    setErrorBannerText("请输入房间名");
    return;
  }

  if (m_joinInProgress) {
    setStatusMessage("正在入会中，请稍候...");
    return;
  }

  if (m_inRoom) {
    setStatusMessage("你已经在房间里了，请先点击“离开房间（SDK）”再重新入会。");
    return;
  }

  clearErrorBanner();
  setJoinInProgress(true);
  setInRoom(false);
  setPrejoinVisible(false);

  const QString name =
      participantName.trimmed().isEmpty() ? m_lastParticipantName : participantName.trimmed();
  const QString room = roomName.trimmed();

  m_backendService->getConnectionDetails(room, name, "", "{}");

  setStatusMessage(
      QString("正在请求连接详情，房间=%1，用户=%2").arg(room, name));
}

void HomeController::cancelCustomJoin() {
  setPrejoinVisible(false);
  setStatusMessage("已取消自定义入会");
}



void HomeController::leaveRoom() {
  m_roomService->disconnect();
  setJoinInProgress(false);
  setInRoom(false);
  setPrejoinVisible(false);
  setCurrentRoomName("");
  clearErrorBanner();
  emit sessionResetRequested();

  if (m_recordingActive) {

    m_recordingActive = false;

    emit recordingActiveChanged();

  }

  if (m_recordingRequestInFlight) {

    m_recordingRequestInFlight = false;

    emit recordingRequestInFlightChanged();

  }

  m_recordingStatusText = "录制状态：未开始";

  emit recordingStatusTextChanged();

  setStatusMessage("LiveKit 已断开：client_initiated");

}



void HomeController::startRecording() {

  if (m_recordingRequestInFlight) {

    setStatusMessage("录制请求进行中，请稍候...");

    return;

  }

  if (!m_inRoom) {

    setStatusMessage("请先成功入会，再发起录制。");

    return;

  }

  if (m_recordingActive) {

    setStatusMessage("录制已在进行中。");

    return;

  }

  m_recordingRequestInFlight = true;

  emit recordingRequestInFlightChanged();

  m_recordingStatusText = "录制状态：正在启动...";

  emit recordingStatusTextChanged();

  m_backendService->startRecording(m_lastRoomName);

  setStatusMessage(QString("正在请求 record/start，房间=%1").arg(m_lastRoomName));

}



void HomeController::stopRecording() {

  if (m_recordingRequestInFlight) {

    setStatusMessage("录制请求进行中，请稍候...");

    return;

  }

  if (!m_inRoom) {

    setStatusMessage("当前不在房间中，无需停止录制。");

    return;

  }

  if (!m_recordingActive) {

    setStatusMessage("当前没有进行中的录制。");

    return;

  }

  m_recordingRequestInFlight = true;

  emit recordingRequestInFlightChanged();

  m_recordingStatusText = "录制状态：正在停止...";

  emit recordingStatusTextChanged();

  m_backendService->stopRecording(m_lastRoomName);

  setStatusMessage(QString("正在请求 record/stop，房间=%1").arg(m_lastRoomName));

}



void HomeController::setStatusMessage(QString message) {

  if (m_statusMessage == message) {

    return;

  }



  m_statusMessage = std::move(message);

  emit statusMessageChanged();

}



void HomeController::setJoinInProgress(bool value) {

  if (m_joinInProgress == value) {

    return;

  }

  m_joinInProgress = value;

  emit joinInProgressChanged();

}



void HomeController::setInRoom(bool value) {

  if (m_inRoom == value) {

    return;

  }

  m_inRoom = value;

  emit inRoomChanged();

}



void HomeController::setErrorBannerText(QString message) {

  if (m_errorBannerText == message) {

    return;

  }

  m_errorBannerText = std::move(message);

  emit errorBannerChanged();

}



void HomeController::clearErrorBanner() { setErrorBannerText(""); }

void HomeController::setCurrentRoomName(QString name) {
  if (m_currentRoomName == name) {
    return;
  }

  m_currentRoomName = std::move(name);
  emit currentRoomNameChanged();
}

void HomeController::setPrejoinVisible(bool value) {
  if (m_prejoinVisible == value) {
    return;
  }

  m_prejoinVisible = value;
  emit prejoinVisibleChanged();
}



QString HomeController::buildUserFacingError(const QString& operation, int statusCode,

                                             const QString& message) const {

  if (operation == "connection-details") {
    return QString("入会失败：连接详情请求异常（HTTP %1）%2")
        .arg(statusCode)
        .arg(message.isEmpty() ? "" : QString(" - %1").arg(message));
  }
  if (operation == "room-create") {
    return QString("创建房间失败（HTTP %1）%2")
        .arg(statusCode)
        .arg(message.isEmpty() ? "" : QString(" - %1").arg(message));
  }

  if (operation == "record-start") {

    return QString("录制启动失败（HTTP %1）%2")

        .arg(statusCode)

        .arg(message.isEmpty() ? "" : QString(" - %1").arg(message));

  }

  if (operation == "record-stop") {

    return QString("录制停止失败（HTTP %1）%2")

        .arg(statusCode)

        .arg(message.isEmpty() ? "" : QString(" - %1").arg(message));

  }



  return QString("请求失败：op=%1, HTTP=%2, %3").arg(operation).arg(statusCode).arg(message);

}

