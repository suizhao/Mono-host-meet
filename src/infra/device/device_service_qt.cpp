#include "infra/device/device_service_qt.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

namespace {
QVariantMap buildDeviceInfo(const QString& id, const QString& name, bool isDefault) {
  QVariantMap item;
  item.insert("id", id);
  item.insert("name", name);
  item.insert("isDefault", isDefault);
  return item;
}

QVariantList parseDeviceProbeResult(const QByteArray& jsonBytes) {
  QVariantList list;
  const auto doc = QJsonDocument::fromJson(jsonBytes);
  if (!doc.isArray()) {
    return list;
  }

  const QJsonArray arr = doc.array();
  for (const auto& v : arr) {
    if (!v.isObject()) {
      continue;
    }
    const auto o = v.toObject();
    list.push_back(buildDeviceInfo(o.value("id").toString(), o.value("name").toString(),
                                   o.value("isDefault").toBool(false)));
  }
  return list;
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

QVariantList probeDevicesViaChildProcess(const QString& kind) {
  const QString helper = deviceProbeProgramPath();
  if (helper.isEmpty()) {
    return {};
  }

  QProcess process;
  process.setProgram(helper);
  process.setArguments({kind});
  process.start();
  if (!process.waitForFinished(4000)) {
    process.kill();
    process.waitForFinished(500);
    return {};
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    return {};
  }

  return parseDeviceProbeResult(process.readAllStandardOutput());
}
} // namespace

DeviceServiceQt::DeviceServiceQt(QObject* parent) : QObject(parent) {}

QVariantList DeviceServiceQt::listAudioInputs() {
  const auto items = probeDevicesViaChildProcess("audio-inputs");
  return items.isEmpty() ? listAudioInputsFallback() : items;
}

QVariantList DeviceServiceQt::listVideoInputs() {
  const auto items = probeDevicesViaChildProcess("video-inputs");
  return items.isEmpty() ? listVideoInputsFallback() : items;
}

QVariantList DeviceServiceQt::listAudioOutputs() {
  const auto items = probeDevicesViaChildProcess("audio-outputs");
  return items.isEmpty() ? listAudioOutputsFallback() : items;
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

QString DeviceServiceQt::selectedAudioInputId() const { return m_selectedAudioInputId; }

QString DeviceServiceQt::selectedVideoInputId() const { return m_selectedVideoInputId; }

QString DeviceServiceQt::selectedAudioOutputId() const { return m_selectedAudioOutputId; }

QVariantList DeviceServiceQt::listAudioInputsFallback() const {
  return {buildDeviceInfo("default-audio-input", "默认麦克风（稳态模式）", true)};
}

QVariantList DeviceServiceQt::listVideoInputsFallback() const {
  return {buildDeviceInfo("default-video-input", "默认摄像头（稳态模式）", true)};
}

QVariantList DeviceServiceQt::listAudioOutputsFallback() const {
  return {buildDeviceInfo("default-audio-output", "默认扬声器（稳态模式）", true)};
}
