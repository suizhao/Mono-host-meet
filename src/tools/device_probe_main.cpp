#include <QAudioDevice>
#include <QCameraDevice>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QTextStream>

namespace {

QJsonObject toDeviceJson(const QString& id, const QString& name, bool isDefault) {
  QJsonObject obj;
  obj.insert("id", id);
  obj.insert("name", name);
  obj.insert("isDefault", isDefault);
  return obj;
}

QJsonArray probeAudioInputs() {
  QJsonArray array;
  const auto devices = QMediaDevices::audioInputs();
  const auto defaultDevice = QMediaDevices::defaultAudioInput();
  for (const auto& device : devices) {
    array.push_back(toDeviceJson(QString::fromLatin1(device.id()),
                                 device.description(),
                                 device == defaultDevice));
  }
  return array;
}

QJsonArray probeVideoInputs() {
  QJsonArray array;
  const auto devices = QMediaDevices::videoInputs();
  const auto defaultDevice = QMediaDevices::defaultVideoInput();
  for (const auto& device : devices) {
    array.push_back(toDeviceJson(QString::fromLatin1(device.id()),
                                 device.description(),
                                 device == defaultDevice));
  }
  return array;
}

QJsonArray probeAudioOutputs() {
  QJsonArray array;
  const auto devices = QMediaDevices::audioOutputs();
  const auto defaultDevice = QMediaDevices::defaultAudioOutput();
  for (const auto& device : devices) {
    array.push_back(toDeviceJson(QString::fromLatin1(device.id()),
                                 device.description(),
                                 device == defaultDevice));
  }
  return array;
}

void printSingleKind(const QJsonArray& array) {
  QTextStream(stdout) << QString::fromUtf8(
      QJsonDocument(array).toJson(QJsonDocument::Compact));
}

int runProbe(const QString& kind) {
  if (kind == "--all") {
    QJsonObject root;
    root.insert("audioInputs", probeAudioInputs());
    root.insert("videoInputs", probeVideoInputs());
    root.insert("audioOutputs", probeAudioOutputs());
    QTextStream(stdout) << QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
    return 0;
  }

  if (kind == "audio-inputs") {
    printSingleKind(probeAudioInputs());
  } else if (kind == "video-inputs") {
    printSingleKind(probeVideoInputs());
  } else if (kind == "audio-outputs") {
    printSingleKind(probeAudioOutputs());
  } else {
    QTextStream(stderr) << "unknown probe kind: " << kind
                        << "\nusage: multilive_device_probe <--all|audio-inputs|video-inputs|audio-outputs>\n";
    return 3;
  }
  return 0;
}

} // namespace

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  const QStringList args = app.arguments();
  if (args.size() < 2) {
    QTextStream(stderr)
        << "usage: multilive_device_probe <--all|audio-inputs|video-inputs|audio-outputs>\n";
    return 2;
  }
  return runProbe(args.at(1));
}
