#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

struct VideoTileItem {
  QString tileId;
  QString identity;
  QString displayName;
  QString sourceText;
  QString imageUrl;
  QString pipImageUrl;
  bool isLocal = false;
  bool hasFrame = false;
  bool pipVisible = false;
  bool micActive = false;
};

class VideoTileModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum Roles {
    TileIdRole = Qt::UserRole + 1,
    IdentityRole,
    DisplayNameRole,
    SourceTextRole,
    ImageUrlRole,
    IsLocalRole,
    HasFrameRole,
    PipImageUrlRole,
    PipVisibleRole,
    MicActiveRole,
  };

  explicit VideoTileModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  void syncTiles(const QList<VideoTileItem>& tiles);
  void clear();
  bool hasAnyFrame() const;

private:
  QList<VideoTileItem> m_tiles;
};
