#include "infra/device/device_service_qt.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QProcess>

namespace {

QVariantMap buildDeviceInfo(const QString& id, const QString& name, bool isDefault) {
  QVariantMap item;
  item.insert("id", id);
  item.insert("name", name);
  item.insert("isDefault", isDefault);
  return item;
}

QString deviceProbeProgramPath() {
  const QString appPath = QCoreApplication::applicationFilePath();
  if (appPath.isEmpty()) {
    return {};
  }

  const QFileInfo appInfo(appPath);
  const QDir dir = appInfo.dir();

#ifdef Q_OS_WIN
  const QString helperName = "multilive_device_probe.exe";
#else
  const QString helperName = "multilive_device_probe";
#endif
  const QString helperPath = dir.filePath(helperName);
  if (!QFileInfo::exists(helperPath)) {
    return {};
  }
  return helperPath;
}

} // namespace

DeviceServiceQt::DeviceServiceQt(QObject* parent) : QObject(parent) {}

void DeviceServiceQt::initialize() {
  if (m_ready || m_probeRunning) return;
  runProbeProcess();
}

bool DeviceServiceQt::isReady() const { return m_ready; }

void DeviceServiceQt::runProbeProcess() {
  const QString helper = deviceProbeProgramPath();
  if (helper.isEmpty()) {
    qWarning() << "DeviceServiceQt: probe binary not found, using fallback devices";
    // Use fallback immediately since we can't probe
    m_cachedAudioInputs = listAudioInputsFallback();
    m_cachedVideoInputs = listVideoInputsFallback();
    m_cachedAudioOutputs = listAudioOutputsFallback();
    m_ready = true;
    emit devicesReady();
    return;
  }

  m_probeRunning = true;
  // Run all three probes sequentially in a single process invocation,
  // or use separate invocations. Using --all mode for a single process.
  auto* process = new QProcess(this);
  process->setProgram(helper);
  process->setArguments({"--all"});
  connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, &DeviceServiceQt::onProbeFinished);
  process->start();
}

void DeviceServiceQt::onProbeFinished() {
  auto* process = qobject_cast<QProcess*>(sender());
  if (!process) return;

  m_probeRunning = false;
  bool ok = (process->exitStatus() == QProcess::NormalExit &&
             process->exitCode() == 0);

  if (ok) {
    const QByteArray jsonBytes = process->readAllStandardOutput();
    const auto doc = QJsonDocument::fromJson(jsonBytes);
    if (doc.isObject()) {
      const auto obj = doc.object();
      m_cachedAudioInputs = parseDeviceProbeResult(
          obj.value("audioInputs").toArray());
      m_cachedVideoInputs = parseDeviceProbeResult(
          obj.value("videoInputs").toArray());
      m_cachedAudioOutputs = parseDeviceProbeResult(
          obj.value("audioOutputs").toArray());
    }
  }

  // Fallback on any empty list
  if (m_cachedAudioInputs.isEmpty())
    m_cachedAudioInputs = listAudioInputsFallback();
  if (m_cachedVideoInputs.isEmpty())
    m_cachedVideoInputs = listVideoInputsFallback();
  if (m_cachedAudioOutputs.isEmpty())
    m_cachedAudioOutputs = listAudioOutputsFallback();

  m_ready = true;
  process->deleteLater();
  emit devicesReady();

  // Auto-select default device if nothing selected yet
  if (m_selectedAudioInputId.isEmpty() && !m_cachedAudioInputs.isEmpty()) {
    const QVariantMap first = m_cachedAudioInputs.first().toMap();
    m_selectedAudioInputId = first.value("id").toString();
  }
  if (m_selectedVideoInputId.isEmpty() && !m_cachedVideoInputs.isEmpty()) {
    const QVariantMap first = m_cachedVideoInputs.first().toMap();
    m_selectedVideoInputId = first.value("id").toString();
  }
  if (m_selectedAudioOutputId.isEmpty() && !m_cachedAudioOutputs.isEmpty()) {
    const QVariantMap first = m_cachedAudioOutputs.first().toMap();
    m_selectedAudioOutputId = first.value("id").toString();
  }
}

QVariantList DeviceServiceQt::parseDeviceProbeResult(
    const QByteArray& jsonBytes) const {
  QVariantList list;
  const auto doc = QJsonDocument::fromJson(jsonBytes);
  if (!doc.isArray()) return list;

  const QJsonArray arr = doc.array();
  for (const auto& v : arr) {
    if (!v.isObject()) continue;
    const auto o = v.toObject();
    list.push_back(buildDeviceInfo(o.value("id").toString(),
                                   o.value("name").toString(),
                                   o.value("isDefault").toBool(false)));
  }
  return list;
}

QVariantList DeviceServiceQt::parseDeviceProbeResult(
    const QJsonArray& arr) const {
  QVariantList list;
  for (const auto& v : arr) {
    if (!v.isObject()) continue;
    const auto o = v.toObject();
    list.push_back(buildDeviceInfo(o.value("id").toString(),
                                   o.value("name").toString(),
                                   o.value("isDefault").toBool(false)));
  }
  return list;
}

QVariantList DeviceServiceQt::listAudioInputs() {
  return m_cachedAudioInputs.isEmpty() ? listAudioInputsFallback()
                                       : m_cachedAudioInputs;
}

QVariantList DeviceServiceQt::listVideoInputs() {
  return m_cachedVideoInputs.isEmpty() ? listVideoInputsFallback()
                                       : m_cachedVideoInputs;
}

QVariantList DeviceServiceQt::listAudioOutputs() {
  return m_cachedAudioOutputs.isEmpty() ? listAudioOutputsFallback()
                                        : m_cachedAudioOutputs;
}

void DeviceServiceQt::selectAudioInput(const QString& deviceId) {
  m_selectedAudioInputId = deviceId;
  qInfo() << "Selected audio input device id =" << deviceId;
}

void DeviceServiceQt::selectVideoInput(const QString& deviceId) {
  m_selectedVideoInputId = deviceId;
  qInfo() << "Selected video input device id =" << deviceId;
}

void DeviceServiceQt::selectAudioOutput(const QString& deviceId) {
  m_selectedAudioOutputId = deviceId;
  qInfo() << "Selected audio output device id =" << deviceId;
}

QString DeviceServiceQt::selectedAudioInputId() const {
  return m_selectedAudioInputId;
}

QString DeviceServiceQt::selectedVideoInputId() const {
  return m_selectedVideoInputId;
}

QString DeviceServiceQt::selectedAudioOutputId() const {
  return m_selectedAudioOutputId;
}

QAudioDevice DeviceServiceQt::resolveAudioInputDevice(
    const QString& deviceId) const {
  if (deviceId.isEmpty()) return {};
  const auto devices = QMediaDevices::audioInputs();
  for (const auto& device : devices) {
    if (QString::fromLatin1(device.id()) == deviceId) return device;
  }
  return {};
}

QVariantList DeviceServiceQt::listAudioInputsFallback() const {
  return {buildDeviceInfo("default-audio-input",
                          "默认麦克风（稳态模式）", true)};
}

QVariantList DeviceServiceQt::listVideoInputsFallback() const {
  return {buildDeviceInfo("default-video-input",
                          "默认摄像头（稳态模式）", true)};
}

QVariantList DeviceServiceQt::listAudioOutputsFallback() const {
  return {buildDeviceInfo("default-audio-output",
                          "默认扬声器（稳态模式）", true)};
}
