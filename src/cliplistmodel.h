#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class Backend;

// Row-level view of the project's clips. The timeline lanes bind to this model instead of
// re-reading Backend::clipsList() (a fresh QVariantList of every clip) on each timelineChanged,
// so a single-clip edit arrives as dataChanged for the changed row and the existing delegates
// are updated in place; only a structural change (clip added, removed or re-ordered) resets
// the model. Backend::clipsList() stays the single source of the row data.
class ClipListModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit ClipListModel(Backend *backend, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override { return m_roles; }

private slots:
    // Re-reads the clip list once per timelineChanged, then forwards only what actually changed.
    void syncFromBackend();

private:
    Backend *m_backend;
    QHash<int, QByteArray> m_roles;
    QVector<int> m_allRoles;
    QVector<QVariantMap> m_rows;
    QStringList m_ids;
};
