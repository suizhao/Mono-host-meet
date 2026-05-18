#include "presentation/controllers/home_controller.h"

#include <QTimer>
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
                 const QString& participantName, const QString& participantToken,
                 bool isHost) {
            clearErrorBanner();
            m_lastRoomName = roomName;
            setCurrentRoomName(roomName);

            if (isHost != m_isHost) {
              m_isHost = isHost;
              emit isHostChanged();
            }

            setStatusMessage(QString("已拿到连接详情，正在连接 LiveKit：room=%1, user=%2%3")
                                 .arg(roomName, participantName,
                                      isHost ? QStringLiteral(" [主持人]") : QString()));

            const bool connected =
                m_roomService->connect(serverUrl, participantToken, true, true);

            setJoinInProgress(false);

            if (connected) {
              setInRoom(true);
              m_roomService->setMicEnabled(false);
              m_roomService->setCameraEnabled(false);
              m_roomService->refreshRemoteState();
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
            if (operation == "disband-room") {
              setStatusMessage(QString("解散房间失败：%1").arg(message));
            }
            const QString userError = buildUserFacingError(operation, statusCode, message);
            setErrorBannerText(userError);
            setStatusMessage(userError);
          });

  connect(m_backendService, &IBackendService::recordingStarted, this,
          [this](const QString& roomName, const QString& egressId) {
            Q_UNUSED(roomName);
            if (m_recordingRequestInFlight) {
              m_recordingRequestInFlight = false;
              emit recordingRequestInFlightChanged();
            }
            m_recordingActive = true;
            m_recordingEgressId = egressId;
            emit recordingActiveChanged();
            m_recordingStatusText = QString("录制状态：进行中 (%1)").arg(egressId.left(12));
            emit recordingStatusTextChanged();
            clearErrorBanner();
            setStatusMessage("录制已开始");
          });

  connect(m_backendService, &IBackendService::recordingStopped, this,
          [this](const QString& roomName) {
            Q_UNUSED(roomName);
            if (m_recordingRequestInFlight) {
              m_recordingRequestInFlight = false;
              emit recordingRequestInFlightChanged();
            }
            m_recordingActive = false;
            m_recordingEgressId.clear();
            emit recordingActiveChanged();
            m_recordingStatusText = "录制状态：已停止";
            emit recordingStatusTextChanged();
            clearErrorBanner();
            setStatusMessage("录制已停止");
          });

  connect(m_backendService, &IBackendService::roomDisbanded, this,
          &HomeController::onRoomDisbanded);

  connect(m_backendService, &IBackendService::muteAllCompleted, this,
          &HomeController::onMuteAllCompleted);

  connect(m_roomService, &IRoomService::hostCommandReceived, this,
          [this](const QString& cmd) {
            if (cmd == QStringLiteral("room_disbanded")) {
              // 主持人通知解散房间，guest 自动离开
              if (!m_isHost) {
                setStatusMessage("主持人已解散房间");
                leaveRoom();
              }
            } else if (cmd == QStringLiteral("mute_self")) {
              if (!m_isHost) {
                m_roomService->setMicEnabled(false);
              }
            } else if (cmd == QStringLiteral("unmute_self")) {
              if (!m_isHost) {
                m_roomService->setMicEnabled(true);
              }
            }
          });
}

QString HomeController::statusMessage() const { return m_statusMessage; }
bool HomeController::joinInProgress() const { return m_joinInProgress; }
bool HomeController::inRoom() const { return m_inRoom; }
bool HomeController::hasErrorBanner() const { return !m_errorBannerText.isEmpty(); }
QString HomeController::errorBannerText() const { return m_errorBannerText; }
bool HomeController::isHost() const { return m_isHost; }
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
    setStatusMessage("你已经在房间里了，请先点击离开房间再重新入会。");
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
    setStatusMessage("你已经在房间里了，请先点击离开房间再重新入会。");
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
    setStatusMessage("你已经在房间里了，请先点击离开房间再重新入会。");
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
  if (m_recordingActive && m_isHost) {
    m_backendService->stopRecording(m_lastRoomName, m_lastParticipantName);
  }

  m_roomService->disconnect();
  setJoinInProgress(false);
  setInRoom(false);
  setPrejoinVisible(false);
  setCurrentRoomName("");
  clearErrorBanner();
  if (m_isHost) {
    m_isHost = false;
    emit isHostChanged();
  }
  if (m_allMuted) {
    m_allMuted = false;
    emit allMutedChanged();
  }
  emit sessionResetRequested();

  if (m_recordingActive) {
    m_recordingActive = false;
    m_recordingEgressId.clear();
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

void HomeController::disbandRoom() {
  if (!m_isHost) {
    setStatusMessage("只有主持人可以解散房间");
    return;
  }
  if (!m_inRoom) {
    setStatusMessage("当前不在房间中");
    return;
  }
  setStatusMessage("正在解散房间...");
  // 通过 data channel 广播解散指令给所有参与者
  m_roomService->publishData(
      QByteArray(R"({"cmd":"room_disbanded"})"), QStringLiteral("host"));
  // 同时调用后端 API（如果服务器支持）
  m_backendService->disbandRoom(m_lastRoomName, m_lastParticipantName);
  // 短暂延迟后主持人也离开，给指令传输留时间
  QTimer::singleShot(500, this, &HomeController::leaveRoom);
}

bool HomeController::allMuted() const { return m_allMuted; }

void HomeController::toggleMuteAll() {
  if (!m_isHost) {
    setStatusMessage("只有主持人可以操作全员静音");
    return;
  }
  if (!m_inRoom) {
    setStatusMessage("当前不在房间中");
    return;
  }

  m_allMuted = !m_allMuted;
  emit allMutedChanged();

  const QByteArray cmd = m_allMuted
      ? QByteArray(R"({"cmd":"mute_self"})")
      : QByteArray(R"({"cmd":"unmute_self"})");
  const QString msg = m_allMuted ? "全员静音" : "解除全员静音";

  m_roomService->publishData(cmd, QStringLiteral("host"));
  setStatusMessage(QString("已广播指令：%1").arg(msg));
  clearErrorBanner();
}

void HomeController::onRoomDisbanded(const QString& roomName) {
  if (roomName != m_lastRoomName) return;
  setStatusMessage("房间已解散");
  leaveRoom();
}

void HomeController::onMuteAllCompleted(const QString& roomName, int mutedCount) {
  if (roomName != m_lastRoomName) return;
  setStatusMessage(QString("全员静音完成：已静音 %1 个麦克风").arg(mutedCount));
  clearErrorBanner();
}

void HomeController::startRecording(const QVariantList& audioVar,
                                     const QVariantList& videoVar) {
  QStringList audioSids, videoSids;
  for (const auto& v : audioVar) audioSids << v.toString();
  for (const auto& v : videoVar) videoSids << v.toString();
  if (!m_isHost) {
    setStatusMessage("只有主持人可以开始录制");
    return;
  }
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
  m_backendService->startRecording(m_lastRoomName, m_lastParticipantName,
                                   audioSids, videoSids);
  setStatusMessage(QString("正在请求开始录制，房间=%1").arg(m_lastRoomName));
}

void HomeController::stopRecording() {
  if (!m_isHost) {
    setStatusMessage("只有主持人可以停止录制");
    return;
  }
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
  m_backendService->stopRecording(m_lastRoomName, m_lastParticipantName);
  setStatusMessage(QString("正在请求停止录制，房间=%1").arg(m_lastRoomName));
}

void HomeController::setStatusMessage(QString message) {
  if (m_statusMessage == message) return;
  m_statusMessage = std::move(message);
  emit statusMessageChanged();
}

void HomeController::setJoinInProgress(bool value) {
  if (m_joinInProgress == value) return;
  m_joinInProgress = value;
  emit joinInProgressChanged();
}

void HomeController::setInRoom(bool value) {
  if (m_inRoom == value) return;
  m_inRoom = value;
  emit inRoomChanged();
}

void HomeController::setErrorBannerText(QString message) {
  if (m_errorBannerText == message) return;
  m_errorBannerText = std::move(message);
  emit errorBannerChanged();
}

void HomeController::clearErrorBanner() { setErrorBannerText(""); }

void HomeController::setCurrentRoomName(QString name) {
  if (m_currentRoomName == name) return;
  m_currentRoomName = std::move(name);
  emit currentRoomNameChanged();
}

void HomeController::setPrejoinVisible(bool value) {
  if (m_prejoinVisible == value) return;
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
