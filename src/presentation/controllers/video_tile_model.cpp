#include "presentation/controllers/video_tile_model.h"

VideoTileModel::VideoTileModel(QObject* parent) : QAbstractListModel(parent) {}

int VideoTileModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return m_tiles.size();
}

QVariant VideoTileModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_tiles.size()) {
    return {};
  }
  const auto& tile = m_tiles.at(index.row());
  switch (role) {
    case TileIdRole:
      return tile.tileId;
    case IdentityRole:
      return tile.identity;
    case DisplayNameRole:
      return tile.displayName;
    case SourceTextRole:
      return tile.sourceText;
    case ImageUrlRole:
      return tile.imageUrl;
    case IsLocalRole:
      return tile.isLocal;
    case HasFrameRole:
      return tile.hasFrame;
    case PipImageUrlRole:
      return tile.pipImageUrl;
    case PipVisibleRole:
      return tile.pipVisible;
    case MicActiveRole:
      return tile.micActive;
    default:
      return {};
  }
}

QHash<int, QByteArray> VideoTileModel::roleNames() const {
  return {
      {TileIdRole, "tileId"},         {IdentityRole, "identity"},
      {DisplayNameRole, "displayName"},
      {SourceTextRole, "sourceText"}, {ImageUrlRole, "imageUrl"},
      {IsLocalRole, "isLocal"},       {HasFrameRole, "hasFrame"},
      {PipImageUrlRole, "pipImageUrl"}, {PipVisibleRole, "pipVisible"},
      {MicActiveRole, "micActive"},
  };
}

void VideoTileModel::syncTiles(const QList<VideoTileItem>& tiles) {
  bool needReset = m_tiles.size() != tiles.size();
  if (!needReset) {
    for (int i = 0; i < m_tiles.size(); ++i) {
      if (m_tiles[i].tileId != tiles[i].tileId) {
        needReset = true;
        break;
      }
    }
  }

  if (needReset) {
    beginResetModel();
    m_tiles = tiles;
    endResetModel();
    return;
  }

  for (int i = 0; i < m_tiles.size(); ++i) {
    if (m_tiles[i].identity == tiles[i].identity &&
        m_tiles[i].displayName == tiles[i].displayName &&
        m_tiles[i].sourceText == tiles[i].sourceText &&
        m_tiles[i].imageUrl == tiles[i].imageUrl &&
        m_tiles[i].isLocal == tiles[i].isLocal &&
        m_tiles[i].hasFrame == tiles[i].hasFrame &&
        m_tiles[i].pipImageUrl == tiles[i].pipImageUrl &&
        m_tiles[i].pipVisible == tiles[i].pipVisible &&
        m_tiles[i].micActive == tiles[i].micActive) {
      continue;
    }

    m_tiles[i] = tiles[i];
    const QModelIndex idx = index(i);
    emit dataChanged(idx, idx);
  }
}

void VideoTileModel::clear() {
  if (m_tiles.isEmpty()) {
    return;
  }
  beginResetModel();
  m_tiles.clear();
  endResetModel();
}

bool VideoTileModel::hasAnyFrame() const {
  for (const auto& tile : m_tiles) {
    if (tile.hasFrame) {
      return true;
    }
  }
  return false;
}
