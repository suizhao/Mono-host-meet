#include "presentation/qml/frame_image_provider.h"

#include <QByteArray>

FrameImageProvider::FrameImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage FrameImageProvider::requestImage(const QString& id, QSize* size,
                                        const QSize& requestedSize) {
  const QString normalizedId = [&id]() {
    QString key = id;
    const int queryPos = key.indexOf('?');
    if (queryPos >= 0) {
      key = key.left(queryPos);
    }
    return QString::fromUtf8(QByteArray::fromPercentEncoding(key.toUtf8()));
  }();

  QMutexLocker locker(&m_mutex);
  const auto it = m_images.constFind(normalizedId);
  if (it == m_images.constEnd()) {
    if (size) {
      *size = QSize();
    }
    return {};
  }

  QImage image = it.value();
  if (requestedSize.isValid()) {
    image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }
  if (size) {
    *size = image.size();
  }
  return image;
}

void FrameImageProvider::setImage(const QString& id, const QImage& image) {
  QMutexLocker locker(&m_mutex);
  m_images.insert(id, image);
}

void FrameImageProvider::removeImage(const QString& id) {
  QMutexLocker locker(&m_mutex);
  m_images.remove(id);
}
