#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QString>

class FrameImageProvider : public QQuickImageProvider {
public:
  FrameImageProvider();

  QImage requestImage(const QString& id, QSize* size,
                      const QSize& requestedSize) override;

  void setImage(const QString& id, const QImage& image);
  void removeImage(const QString& id);

private:
  mutable QMutex m_mutex;
  QHash<QString, QImage> m_images;
};
