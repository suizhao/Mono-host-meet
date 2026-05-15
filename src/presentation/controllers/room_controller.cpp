#include "presentation/controllers/room_controller.h"

#include "presentation/qml/frame_image_provider.h"

#include <QByteArray>
#include <QBuffer>
#include <QDebug>
#include <QHash>
#include <QImage>
#include <QSet>

#include <algorithm>

namespace {
QImage decodeImageFromDataUrl(const QString& dataUrl) {
  const int comma = dataUrl.indexOf(',');
  if (comma <= 0) {
    return {};
  }
  const QByteArray encoded = dataUrl.mid(comma + 1).toLatin1();
  const QByteArray bytes = QByteArray::fromBase64(encoded);
  if (bytes.isEmpty()) {
    return {};
  }
  QImage image;
  image.loadFromData(bytes);
  return image;
}
} // namespace

RoomController::RoomController(IRoomService* roomService, IDeviceService* deviceService,
                               FrameImageProvider* frameImageProvider,
                               QObject* parent)
    : QObject(parent),
      m_roomService(roomService),
      m_deviceService(deviceService),
      m_frameImageProvider(frameImageProvider),
      m_remoteVideoTileModel(this) {
  refreshDisplayTexts();
  m_deviceStatusText = "设备状态：未枚举(点击“刷新设备列表”后加载真实设备)";

  m_remoteRefreshTimer.setInterval(80);
  connect(&m_remoteRefreshTimer, &QTimer::timeout, this,
          &RoomController::refreshRemoteDisplayState);
  m_remoteRefreshTimer.start();

  connect(m_roomService, &IRoomService::remoteStateDirty, this,
          &RoomController::refreshRemoteDisplayState);

  if (auto* dqs = dynamic_cast<QObject*>(m_deviceService)) {
    connect(dqs, SIGNAL(devicesReady()), this, SLOT(onDevicesReady()));
  }
}

bool RoomController::micEnabled() const { return m_micEnabled; }

bool RoomController::cameraEnabled() const { return m_cameraEnabled; }
bool RoomController::screenShareEnabled() const { return m_screenShareEnabled; }

QString RoomController::micToggleText() const { return m_micToggleText; }

QString RoomController::cameraToggleText() const { return m_cameraToggleText; }
QString RoomController::screenShareToggleText() const { return m_screenShareToggleText; }

QString RoomController::trackStatusText() const { return m_trackStatusText; }

QString RoomController::trackErrorText() const { return m_trackErrorText; }

bool RoomController::hasTrackError() const { return !m_trackErrorText.isEmpty(); }

QString RoomController::remoteParticipantsText() const {
  return m_remoteParticipantsText;
}

QObject* RoomController::remoteVideoTileModel() const {
  return const_cast<VideoTileModel*>(&m_remoteVideoTileModel);
}

int RoomController::remoteVideoTileCount() const {
  return m_remoteVideoTileModel.rowCount();
}

QVariantList RoomController::remoteVideoTiles() const { return m_remoteVideoTiles; }
QString RoomController::remoteVideoDataUrl() const { return m_remoteVideoDataUrl; }
QString RoomController::remoteVideoSourceText() const { return m_remoteVideoSourceText; }

bool RoomController::hasRemoteVideo() const { return m_remoteVideoTileModel.hasAnyFrame(); }

QVariantList RoomController::audioInputDevices() const { return m_audioInputDevices; }

QVariantList RoomController::videoInputDevices() const { return m_videoInputDevices; }

QVariantList RoomController::audioOutputDevices() const { return m_audioOutputDevices; }

QString RoomController::deviceStatusText() const { return m_deviceStatusText; }

void RoomController::toggleMic() {
  const bool nextState = !m_micEnabled;
  const QString deviceId = m_deviceService->selectedAudioInputId();
  if (!m_roomService->setMicEnabled(nextState, deviceId)) {
    setTrackError(m_roomService->lastError());
    qWarning() << "toggleMic failed:" << m_trackErrorText;
    return;
  }

  m_micEnabled = nextState;
  setTrackError("");
  refreshDisplayTexts();
  emit micEnabledChanged();
}

void RoomController::toggleCamera() {
  const bool nextState = !m_cameraEnabled;
  if (!m_roomService->setCameraEnabled(nextState)) {
    setTrackError(m_roomService->lastError());
    qWarning() << "toggleCamera failed:" << m_trackErrorText;
    return;
  }

  m_cameraEnabled = nextState;
  setTrackError("");
  refreshDisplayTexts();
  emit cameraEnabledChanged();
}

void RoomController::toggleScreenShare() {
  const bool nextState = !m_screenShareEnabled;
  if (!m_roomService->setScreenShareEnabled(nextState)) {
    setTrackError(m_roomService->lastError());
    qWarning() << "toggleScreenShare failed:" << m_trackErrorText;
    return;
  }

  m_screenShareEnabled = nextState;
  setTrackError("");
  refreshDisplayTexts();
  emit screenShareEnabledChanged();
}

void RoomController::refreshDevices() {
  // 触发探测（initialize 内部有防重入守卫）
  if (auto* dqs = dynamic_cast<QObject*>(m_deviceService)) {
    QMetaObject::invokeMethod(dqs, "initialize", Qt::QueuedConnection);
  }

  const auto nextAudioInputs = m_deviceService->listAudioInputs();
  const auto nextVideoInputs = m_deviceService->listVideoInputs();
  const auto nextAudioOutputs = m_deviceService->listAudioOutputs();

  if (m_audioInputDevices != nextAudioInputs) {
    m_audioInputDevices = nextAudioInputs;
    emit audioInputDevicesChanged();
  }

  if (m_videoInputDevices != nextVideoInputs) {
    m_videoInputDevices = nextVideoInputs;
    emit videoInputDevicesChanged();
  }

  if (m_audioOutputDevices != nextAudioOutputs) {
    m_audioOutputDevices = nextAudioOutputs;
    emit audioOutputDevicesChanged();
  }

  m_deviceStatusText = QString("设备枚举完成：mic=%1, cam=%2, spk=%3")
                           .arg(m_audioInputDevices.size())
                           .arg(m_videoInputDevices.size())
                           .arg(m_audioOutputDevices.size());
  emit deviceStatusTextChanged();
}

void RoomController::selectAudioInput(const QString& deviceId) {
  m_deviceService->selectAudioInput(deviceId);
  m_deviceStatusText = QString("已选择麦克风：%1").arg(deviceId);
  emit deviceStatusTextChanged();
}

void RoomController::selectVideoInput(const QString& deviceId) {
  m_deviceService->selectVideoInput(deviceId);
  m_deviceStatusText = QString("已选择摄像头：%1").arg(deviceId);
  emit deviceStatusTextChanged();
}

void RoomController::selectAudioOutput(const QString& deviceId) {
  m_deviceService->selectAudioOutput(deviceId);
  m_roomService->setAudioOutputDevice(deviceId);
  m_deviceStatusText = QString("已选择扬声器：%1").arg(deviceId);
  emit deviceStatusTextChanged();
}

void RoomController::resetSessionUiState() {
  bool changed = false;
  if (m_micEnabled != false) {
    m_micEnabled = false;
    emit micEnabledChanged();
    changed = true;
  }
  if (m_cameraEnabled != false) {
    m_cameraEnabled = false;
    emit cameraEnabledChanged();
    changed = true;
  }
  if (m_screenShareEnabled) {
    m_roomService->setScreenShareEnabled(false);
    m_screenShareEnabled = false;
    emit screenShareEnabledChanged();
    changed = true;
  }
  if (!m_trackErrorText.isEmpty()) {
    m_trackErrorText.clear();
    emit trackErrorTextChanged();
    emit trackErrorVisibilityChanged();
  }
  if (m_remoteParticipantsText != "远端参与者：0") {
    m_remoteParticipantsText = "远端参与者：0";
    emit remoteParticipantsTextChanged();
  }
  if (m_remoteVideoSourceText != "当前画面来源：无") {
    m_remoteVideoSourceText = "当前画面来源：无";
    emit remoteVideoSourceTextChanged();
  }
  if (!m_remoteVideoTiles.isEmpty()) {
    m_remoteVideoTiles.clear();
    emit remoteVideoTilesChanged();
  }
  m_remoteVideoTileModel.clear();
  for (auto it = m_tileFrameVersions.cbegin(); it != m_tileFrameVersions.cend(); ++it) {
    if (m_frameImageProvider) {
      m_frameImageProvider->removeImage(it.key());
    }
  }
  m_tileFrameVersions.clear();
  if (!m_remoteVideoDataUrl.isEmpty()) {
    m_remoteVideoDataUrl.clear();
    emit remoteVideoDataUrlChanged();
    emit hasRemoteVideoChanged();
  }
  if (changed) {
    refreshDisplayTexts();
  }
}

void RoomController::refreshDisplayTexts() {
  const QString nextMicText =
      m_micEnabled ? "关闭麦克风（SDK）" : "打开麦克风（SDK）";
  const QString nextCameraText =
      m_cameraEnabled ? "关闭摄像头（SDK）" : "打开摄像头（SDK）";
  const QString nextScreenShareText =
      m_screenShareEnabled ? "停止共享屏幕（SDK）" : "开始共享屏幕（SDK）";
  const QString nextTrackStatus =
      QString("本地轨道状态：mic=%1, camera=%2, share=%3")
          .arg(m_micEnabled ? "on" : "off", m_cameraEnabled ? "on" : "off",
               m_screenShareEnabled ? "on" : "off");

  if (m_micToggleText != nextMicText) {
    m_micToggleText = nextMicText;
    emit micToggleTextChanged();
  }

  if (m_cameraToggleText != nextCameraText) {
    m_cameraToggleText = nextCameraText;
    emit cameraToggleTextChanged();
  }

  if (m_screenShareToggleText != nextScreenShareText) {
    m_screenShareToggleText = nextScreenShareText;
    emit screenShareToggleTextChanged();
  }

  if (m_trackStatusText != nextTrackStatus) {
    m_trackStatusText = nextTrackStatus;
    emit trackStatusTextChanged();
  }
}

void RoomController::refreshRemoteDisplayState() {
  m_roomService->refreshRemoteState();

  const QString nextParticipantsText = m_roomService->remoteParticipantsText();
  if (m_remoteParticipantsText != nextParticipantsText) {
    m_remoteParticipantsText = nextParticipantsText;
    emit remoteParticipantsTextChanged();
  }

  const bool oldHasRemoteVideo = hasRemoteVideo();
  const QVariantList nextTiles = m_roomService->remoteVideoTiles();
  QVariantList nextLiteTiles;
  QList<VideoTileItem> modelTiles;
  QSet<QString> seenTileIds;
  QString nextRemoteVideoDataUrl;

  for (const auto& item : nextTiles) {
    const QVariantMap map = item.toMap();
    const QString tileId = map.value("tileId").toString();
    if (tileId.isEmpty()) {
      continue;
    }
    seenTileIds.insert(tileId);

    const QString dataUrl = map.value("dataUrl").toString();
    bool hasFrame = false;
    int version = m_tileFrameVersions.value(tileId, 0);
    if (!dataUrl.isEmpty()) {
      const QImage image = decodeImageFromDataUrl(dataUrl);
      if (!image.isNull() && m_frameImageProvider) {
        m_frameImageProvider->setImage(tileId, image);
        version += 1;
        m_tileFrameVersions.insert(tileId, version);
        hasFrame = true;
        if (nextRemoteVideoDataUrl.isEmpty()) {
          nextRemoteVideoDataUrl = QString("image://multilive/%1?v=%2").arg(tileId).arg(version);
        }
      }
    }

    QVariantMap lite = map;
    lite.remove("dataUrl");
    lite.insert("imageUrl", QString("image://multilive/%1?v=%2").arg(tileId).arg(version));
    lite.insert("hasFrame", hasFrame);
    nextLiteTiles.push_back(lite);

    VideoTileItem modelTile;
    modelTile.tileId = tileId;
    modelTile.identity = map.value("identity").toString();
    modelTile.displayName = map.value("displayName").toString();
    modelTile.sourceText = map.value("sourceText").toString();
    const QVariant isLocalVariant = map.value("isLocal");
    modelTile.isLocal = isLocalVariant.isValid() ? isLocalVariant.toBool() : false;
    modelTile.hasFrame = hasFrame;
    modelTile.imageUrl = lite.value("imageUrl").toString();
    modelTile.micActive = map.value("micActive").toBool();
    modelTiles.push_back(modelTile);
  }

  for (auto it = m_tileFrameVersions.begin(); it != m_tileFrameVersions.end();) {
    if (!seenTileIds.contains(it.key())) {
      if (m_frameImageProvider) {
        m_frameImageProvider->removeImage(it.key());
      }
      it = m_tileFrameVersions.erase(it);
    } else {
      ++it;
    }
  }

  // 去重与画中画：按 identity 聚合，同一参与者最多一个 tile
  {
    // 收集有真实视频轨的 identity（摄像头或屏幕共享）
    QSet<QString> identitiesWithVideo;
    for (const auto& tile : modelTiles) {
      if (tile.sourceText == "摄像头" || tile.sourceText == "屏幕共享") {
        identitiesWithVideo.insert(tile.identity);
      }
    }

    // 移除多余 tile：有视频轨的参与者不再保留占位 tile
    QList<int> indicesToRemove;
    for (int i = 0; i < modelTiles.size(); ++i) {
      if (modelTiles[i].sourceText == "无视频流" &&
          identitiesWithVideo.contains(modelTiles[i].identity)) {
        indicesToRemove.append(i);
      }
    }
    std::sort(indicesToRemove.begin(), indicesToRemove.end(),
              std::greater<int>());
    for (int idx : indicesToRemove) {
      modelTiles.removeAt(idx);
    }

    // 画中画：同一参与者的摄像头+屏幕共享同时存在时，摄像头嵌入屏幕共享 tile 右下角
    QList<int> cameraIndicesToRemove;
    for (int si = 0; si < modelTiles.size(); ++si) {
      if (modelTiles[si].sourceText != "屏幕共享") continue;
      const QString& shareIdentity = modelTiles[si].identity;
      if (shareIdentity.isEmpty()) continue;
      for (int ci = 0; ci < modelTiles.size(); ++ci) {
        if (ci == si) continue;
        if (modelTiles[ci].sourceText == "摄像头" &&
            modelTiles[ci].identity == shareIdentity) {
          modelTiles[si].pipImageUrl = modelTiles[ci].imageUrl;
          modelTiles[si].pipVisible = true;
          cameraIndicesToRemove.append(ci);
          break;
        }
      }
    }
    std::sort(cameraIndicesToRemove.begin(), cameraIndicesToRemove.end(),
              std::greater<int>());
    for (int idx : cameraIndicesToRemove) {
      modelTiles.removeAt(idx);
    }
  }

  m_remoteVideoTileModel.syncTiles(modelTiles);

  if (m_remoteVideoTiles != nextLiteTiles) {
    m_remoteVideoTiles = nextLiteTiles;
    emit remoteVideoTilesChanged();
  }

  const QString nextVideoSourceText = m_roomService->remoteVideoSourceText();
  if (m_remoteVideoSourceText != nextVideoSourceText) {
    m_remoteVideoSourceText = nextVideoSourceText;
    emit remoteVideoSourceTextChanged();
  }

  if (nextRemoteVideoDataUrl.isEmpty()) {
    nextRemoteVideoDataUrl = m_roomService->remoteVideoDataUrl();
  }
  if (m_remoteVideoDataUrl != nextRemoteVideoDataUrl) {
    m_remoteVideoDataUrl = nextRemoteVideoDataUrl;
    emit remoteVideoDataUrlChanged();
  }

  if (oldHasRemoteVideo != hasRemoteVideo()) {
    emit hasRemoteVideoChanged();
  }

}

void RoomController::setTrackError(const QString& message) {
  const bool oldHasError = hasTrackError();
  if (m_trackErrorText == message) {
    return;
  }

  m_trackErrorText = message;
  emit trackErrorTextChanged();

  if (oldHasError != hasTrackError()) {
    emit trackErrorVisibilityChanged();
  }
}


