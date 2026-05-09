#include "infra/device/device_service_qt.h"

#include <QDebug>
#include <QVariantMap>

namespace {

QVariantMap makeDevice(const QString& id, const QString& name) {
    return QVariantMap{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
    };
}

}  // namespace

namespace infra::device {

DeviceServiceQt::DeviceServiceQt(QObject* parent)
    : QObject(parent) {}

QVariantList DeviceServiceQt::listAudioInputs() {
    return QVariantList{makeDevice(QStringLiteral("default-audio-in"), QStringLiteral("默认麦克风"))};
}

QVariantList DeviceServiceQt::listVideoInputs() {
    return QVariantList{makeDevice(QStringLiteral("default-video-in"), QStringLiteral("默认摄像头"))};
}

QVariantList DeviceServiceQt::listAudioOutputs() {
    return QVariantList{makeDevice(QStringLiteral("default-audio-out"), QStringLiteral("默认扬声器"))};
}

void DeviceServiceQt::selectAudioInput(const QString& deviceId) {
    qInfo().noquote() << QStringLiteral("[MVP][DeviceService] selectAudioInput=%1").arg(deviceId);
}

void DeviceServiceQt::selectVideoInput(const QString& deviceId) {
    qInfo().noquote() << QStringLiteral("[MVP][DeviceService] selectVideoInput=%1").arg(deviceId);
}

void DeviceServiceQt::selectAudioOutput(const QString& deviceId) {
    qInfo().noquote() << QStringLiteral("[MVP][DeviceService] selectAudioOutput=%1").arg(deviceId);
}

}  // namespace infra::device
