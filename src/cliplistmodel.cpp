#include "cliplistmodel.h"

#include "backend.h"

namespace {
// One role per key Backend::clipsList() puts into a clip map: the TimelineClip::toJson() fields
// plus the asset previews (thumbnail, filmstrip, waveform) the timeline delegates bind to.
const QList<QByteArray> kRoleNames = {
    "id", "assetId", "sourcePath", "name", "kind", "track",
    "timelineStartSec", "sourceInSec", "durationSec", "sourceDurationSec",
    "speed", "volume", "muted",
    "brightness", "contrast", "saturation", "blur", "vignette",
    "transitionIn", "transitionDurationSec",
    "audioDenoise", "audioVoiceClarity", "audioNormalize", "audioCompress",
    "bgRemovalMode", "chromaColor", "chromaSimilarity", "chromaBlend", "processedPath",
    "keyframes",
    "thumbnailPath", "filmstripPath", "filmstripFrames",
    "filmstripTileWidth", "filmstripTileHeight", "hasAudio", "waveformPeaks",
};
}

ClipListModel::ClipListModel(Backend *backend, QObject *parent)
    : QAbstractListModel(parent), m_backend(backend) {
    int role = Qt::UserRole;
    for (const QByteArray &name : kRoleNames) {
        m_roles.insert(role, name);
        m_allRoles.append(role);
        ++role;
    }
    connect(m_backend, &Backend::timelineChanged, this, &ClipListModel::syncFromBackend);
}

int ClipListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant ClipListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const QByteArray name = m_roles.value(role);
    if (name.isEmpty())
        return {};
    return m_rows.at(index.row()).value(QString::fromLatin1(name));
}

void ClipListModel::syncFromBackend() {
    // One list build per change — the same budget Backend::clipsListBuilds() counts — and the
    // diff below decides whether this was a row edit or a structural change.
    const QVariantList fresh = m_backend->clipsList();
    QStringList freshIds;
    QVector<QVariantMap> freshRows;
    freshIds.reserve(fresh.size());
    freshRows.reserve(fresh.size());
    for (const QVariant &v : fresh) {
        const QVariantMap map = v.toMap();
        freshRows.append(map);
        freshIds.append(map.value(QStringLiteral("id")).toString());
    }

    if (freshIds == m_ids) {
        // Same clips in the same order: forward only the rows whose data actually changed.
        QVector<int> changed;
        for (int i = 0; i < freshRows.size(); ++i) {
            if (freshRows.at(i) != m_rows.at(i))
                changed.append(i);
        }
        m_rows = freshRows;
        for (int row : changed)
            emit dataChanged(index(row), index(row), m_allRoles);
    } else {
        beginResetModel();
        m_ids = freshIds;
        m_rows = freshRows;
        endResetModel();
    }
}
