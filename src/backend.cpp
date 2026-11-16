#include "backend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

Backend::Backend(QObject *parent) : QObject(parent) {
    m_playbackTimer.setInterval(16);
    m_playbackTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_playbackTimer, &QTimer::timeout, this, [this] {
        const double elapsedSec = m_playbackClock.restart() / 1000.0;
        // A playing media lane is the master clock; the timer only drives gaps, images and titles.
        if (!m_playing || clockLane() != QLatin1String("timer"))
            return;
        const double total = m_project.totalDurationSec();
        const double next = m_playheadSec + elapsedSec;
        if (total <= 0.0 || next >= total) {
            setPlaying(false);
            setPlayheadSec(0.0);
        } else {
            advancePlayhead(next);
        }
    });

    connect(this, &Backend::playheadChanged, this, &Backend::previewStateChanged);
    connect(this, &Backend::previewChanged, this, &Backend::previewStateChanged);
    connect(this, &Backend::timelineChanged, this, [this] {
        emit previewStateChanged();
        refreshLanes();
        refreshActiveTexts(true);
    });

    refreshCapabilities();

    connect(&m_exportProcess, &QProcess::readyReadStandardError, this, [this] {
        const QString chunk = QString::fromUtf8(m_exportProcess.readAllStandardError());
        const QRegularExpression timeRe(R"(time=(\d+):(\d+):(\d+\.\d+))");
        auto it = timeRe.globalMatch(chunk);
        while (it.hasNext()) {
            const auto m = it.next();
            const double sec = m.captured(1).toInt() * 3600.0 + m.captured(2).toInt() * 60.0 + m.captured(3).toDouble();
            const double total = std::max(0.5, m_project.totalDurationSec());
            m_exportProgress = std::clamp(sec / total, 0.0, 0.99);
            emit exportStateChanged();
        }
    });

    connect(&m_exportProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (!m_exportTempAss.isEmpty()) {
            QFile::remove(m_exportTempAss);
            m_exportTempAss.clear();
        }
        const bool ok = (code == 0 && status == QProcess::NormalExit);
        m_exportProgress = ok ? 1.0 : 0.0;
        emit exportStateChanged();
        if (ok) {
            setStatus("Exported: " + m_exportOutputPath);
            emit exportFinished(true, "Saved to " + m_exportOutputPath);
        } else {
            const QString err = QString::fromUtf8(m_exportProcess.readAllStandardError()).right(400);
            fail("Export failed: " + err);
            emit exportFinished(false, err);
        }
    });

    connect(&m_voiceoverProcess, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        emit voiceoverStateChanged();
        if (!QFileInfo::exists(m_voiceoverPath) || QFileInfo(m_voiceoverPath).size() <= 1024) {
            setStatus("Voiceover recording cancelled or empty");
            return;
        }
        const QString path = m_voiceoverPath;
        const double startSec = m_voiceoverStartSec;
        importAssetAsync(path, [this, startSec](const QString &assetId) {
            if (assetId.isEmpty())
                return; // the import already reported the reason; leave the timeline untouched
            addAssetToTimeline(assetId, 2, startSec);
            setStatus("Voiceover recorded & added to Audio track");
        });
    });
}

Backend::~Backend() {
    if (m_exportProcess.state() != QProcess::NotRunning) {
        m_exportProcess.kill();
        m_exportProcess.waitForFinished(1000);
    }
    if (m_voiceoverProcess.state() != QProcess::NotRunning) {
        m_voiceoverProcess.terminate();
        m_voiceoverProcess.waitForFinished(1000);
    }
}

QString Backend::normalizeLocalPath(const QString &pathOrUrl) const {
    if (pathOrUrl.startsWith("file://"))
        return QUrl(pathOrUrl).toLocalFile();
    return pathOrUrl;
}

QString Backend::nextId(const QString &prefix) {
    return QString("%1_%2_%3").arg(prefix).arg(QDateTime::currentMSecsSinceEpoch() % 100000).arg(m_idCounter++);
}

// Continuous edits (slider drags, typing) of the same property share one undo step: a snapshot is
// only taken when the merge key changes or the property has been idle for a second.
void Backend::pushUndo(const QString &mergeKey) {
    const bool merge = !mergeKey.isEmpty() && mergeKey == m_undoMergeKey && !m_undoStack.isEmpty() &&
                       m_undoMergeClock.isValid() && m_undoMergeClock.elapsed() < 1000;
    m_undoMergeKey = mergeKey;
    m_undoMergeClock.start();
    if (!merge) {
        m_undoStack.append(m_project);
        if (m_undoStack.size() > 50)
            m_undoStack.removeFirst();
    }
    m_redoStack.clear();
    m_dirty = true;
    emit projectChanged();
}

void Backend::setStatus(const QString &msg) {
    m_statusMessage = msg;
    emit statusChanged();
}

void Backend::fail(const QString &msg) {
    m_statusMessage = msg;
    emit statusChanged();
    emit errorOccurred(msg);
}

QString Backend::laneName(Lane lane) {
    switch (lane) {
    case Lane::Base: return QStringLiteral("base");
    case Lane::Overlay: return QStringLiteral("overlay");
    case Lane::Audio: return QStringLiteral("audio");
    }
    return {};
}

Backend::Lane Backend::laneFromName(const QString &name) {
    if (name == QLatin1String("base"))
        return Lane::Base;
    if (name == QLatin1String("overlay"))
        return Lane::Overlay;
    return Lane::Audio;
}

void Backend::setPlayheadSec(double sec) {
    const double clamped = std::max(0.0, sec);
    if (std::abs(clamped - m_playheadSec) < 1e-4)
        return;
    m_playheadSec = clamped;
    emit playheadChanged();
    refreshLanes();
    refreshActiveTexts(false);
    emit seekRequested();
}

void Backend::advancePlayhead(double sec) {
    m_playheadSec = std::max(0.0, sec);
    emit playheadChanged();
    refreshLanes();
    refreshActiveTexts(false);
}

void Backend::refreshLanes() {
    bool changed = false;
    for (Lane lane : {Lane::Base, Lane::Overlay, Lane::Audio}) {
        const auto *clip = clipAtPlayhead(lane);
        const QString id = clip ? clip->id : QString();
        QString &cached = m_laneClipIds[int(lane)];
        if (cached != id) {
            cached = id;
            changed = true;
        }
    }
    if (changed)
        emit previewChanged();
}

void Backend::refreshActiveTexts(bool force) {
    QStringList ids;
    for (const auto &t : m_project.texts) {
        if (m_playheadSec >= t.startSec && m_playheadSec <= t.endSec())
            ids.append(t.id);
    }
    if (!force && ids == m_activeTextIds)
        return;
    m_activeTextIds = ids;
    emit activeTextsChanged();
}

void Backend::setPlaying(bool play) {
    if (m_playing == play)
        return;
    if (play) {
        const double total = m_project.totalDurationSec();
        if (total <= 0.0)
            return;
        if (m_playheadSec >= total - 0.01)
            setPlayheadSec(0.0);
    }
    m_playing = play;
    if (m_playing) {
        m_playbackClock.start();
        m_playbackTimer.start();
    } else {
        m_playbackTimer.stop();
    }
    emit playbackChanged();
}

void Backend::setZoomPxPerSec(double px) {
    const double next = std::clamp(px, 12.0, 240.0);
    if (std::abs(next - m_zoomPxPerSec) < 0.1)
        return;
    m_zoomPxPerSec = next;
    emit viewChanged();
}

const TimelineClip *Backend::clipAtPlayhead(Lane lane) const {
    const TimelineClip *found = nullptr;
    for (const auto &c : m_project.clips) {
        const bool audioTrack = (c.kind == "audio" || c.track == 2);
        const bool onLane = (lane == Lane::Audio) ? audioTrack : (!audioTrack && c.track == int(lane));
        if (!onLane)
            continue;
        if (m_playheadSec >= c.timelineStartSec && m_playheadSec < c.timelineEndSec()) {
            if (!found || c.timelineStartSec >= found->timelineStartSec)
                found = &c;
        }
    }
    return found;
}

QString Backend::clockLane() const {
    for (Lane lane : {Lane::Base, Lane::Overlay}) {
        const auto *clip = clipAtPlayhead(lane);
        if (clip && clip->kind == "video")
            return laneName(lane);
    }
    if (clipAtPlayhead(Lane::Audio))
        return laneName(Lane::Audio);
    return QStringLiteral("timer");
}

static QVariantMap transformMap(const Keyframe &kf) {
    return {{"x", kf.x}, {"y", kf.y}, {"scale", kf.scale}, {"opacity", kf.opacity}, {"rotation", kf.rotation}};
}

QVariantMap Backend::laneMap(Lane lane) const {
    const auto *clip = clipAtPlayhead(lane);
    if (!clip)
        return {{"active", false}, {"clipId", QString()}, {"url", QString()}, {"isImage", false}};

    const bool hasProcessed = !clip->processedPath.isEmpty() && QFileInfo::exists(clip->processedPath);
    const QString path = hasProcessed ? clip->processedPath : clip->sourcePath;
    const bool isImage = clip->kind == "image" || (hasProcessed && clip->processedPath.endsWith(".png"));
    const double local = std::max(0.0, m_playheadSec - clip->timelineStartSec);

    int mediaWidth = m_project.width;
    int mediaHeight = m_project.height;
    const int assetIdx = m_project.findAssetIndex(clip->assetId);
    if (assetIdx >= 0) {
        mediaWidth = m_project.assets[assetIdx].width;
        mediaHeight = m_project.assets[assetIdx].height;
    }

    return {
        {"active", true},
        {"clipId", clip->id},
        {"kind", clip->kind},
        {"url", QUrl::fromLocalFile(path).toString()},
        {"isImage", isImage},
        {"positionMs", isImage ? qint64(0) : qint64((local * clip->speed + clip->sourceInSec) * 1000.0)},
        {"volume", std::clamp(clip->volume, 0.0, 1.0)},
        {"muted", clip->muted || !m_project.laneAudible(lane == Lane::Audio ? 2 : int(lane))},
        {"speed", clip->speed},
        {"mediaWidth", mediaWidth},
        {"mediaHeight", mediaHeight},
        {"transform", transformMap(clip->evaluateAt(local))},
        {"brightness", clip->brightness},
        {"contrast", clip->contrast},
        {"saturation", clip->saturation},
        {"blur", clip->blur},
        {"vignette", clip->vignette},
        {"chromaKey", clip->bgRemovalMode == "chromakey"},
        {"chromaColor", clip->chromaColor},
        {"chromaSimilarity", clip->chromaSimilarity},
        {"chromaBlend", clip->chromaBlend},
    };
}

QVariantMap Backend::selectedClipTransform() const {
    const int idx = m_project.findClipIndex(m_selectedClipId);
    if (idx < 0)
        return transformMap(Keyframe{});
    const auto &clip = m_project.clips[idx];
    return transformMap(clip.evaluateAt(std::clamp(m_playheadSec - clip.timelineStartSec, 0.0, clip.durationSec)));
}

QVariantMap Backend::selectedClip() const {
    const int idx = m_project.findClipIndex(m_selectedClipId);
    if (idx < 0)
        return {};
    return m_project.clips[idx].toJson().toVariantMap();
}

QVariantMap Backend::selectedText() const {
    const int idx = m_project.findTextIndex(m_selectedTextId);
    if (idx < 0)
        return {};
    return m_project.texts[idx].toJson().toVariantMap();
}

QVariantList Backend::assetsList() const {
    QVariantList out;
    for (const auto &a : m_project.assets)
        out.append(a.toJson().toVariantMap());
    return out;
}

QVariantList Backend::clipsList() const {
    QVariantList out;
    for (const auto &c : m_project.clips) {
        QVariantMap map = c.toJson().toVariantMap();
        const int aIdx = m_project.findAssetIndex(c.assetId);
        if (aIdx >= 0) {
            const MediaAsset &asset = m_project.assets[aIdx];
            map["thumbnailPath"] = asset.thumbnailPath;
            map["filmstripPath"] = asset.filmstripPath;
            map["filmstripFrames"] = asset.kind == "image" ? 1 : MediaEngine::kFilmstripFrames;
            map["hasAudio"] = asset.hasAudio;
            QVariantList peaks;
            for (double p : asset.waveformPeaks)
                peaks.append(p);
            map["waveformPeaks"] = peaks;
        }
        out.append(map);
    }
    return out;
}

QVariantList Backend::textsList() const {
    QVariantList out;
    for (const auto &t : m_project.texts)
        out.append(t.toJson().toVariantMap());
    return out;
}

QVariantList Backend::activeTexts() const {
    QVariantList out;
    for (const auto &t : m_project.texts) {
        if (m_playheadSec >= t.startSec && m_playheadSec <= t.endSec())
            out.append(t.toJson().toVariantMap());
    }
    return out;
}

QVariantList Backend::transitionPoints() const {
    // Sort by lane then start so only neighbouring clips need comparing.
    QList<TimelineClip> ordered = m_project.clips;
    std::sort(ordered.begin(), ordered.end(), [](const TimelineClip &a, const TimelineClip &b) {
        return a.track == b.track ? a.timelineStartSec < b.timelineStartSec : a.track < b.track;
    });

    QVariantList out;
    for (int i = 1; i < ordered.size(); ++i) {
        const TimelineClip &first = ordered[i - 1];
        const TimelineClip &second = ordered[i];
        if (first.track != second.track)
            continue;
        // Only a true junction: the outgoing clip ends where the incoming one starts.
        if (std::abs(first.timelineEndSec() - second.timelineStartSec) > 0.02)
            continue;
        out.append(QVariantMap{{"firstId", first.id},
                               {"secondId", second.id},
                               {"track", second.track},
                               {"timeSec", second.timelineStartSec},
                               {"kind", second.transitionIn},
                               {"durationSec", second.transitionDurationSec}});
    }
    return out;
}

void Backend::setTransitionBetween(const QString &firstId, const QString &secondId,
                                   const QString &kind, double durationSec) {
    const int firstIdx = m_project.findClipIndex(firstId);
    const int secondIdx = m_project.findClipIndex(secondId);
    if (firstIdx < 0 || secondIdx < 0 || firstIdx == secondIdx)
        return;

    const TimelineClip &first = m_project.clips[firstIdx];
    const TimelineClip &second = m_project.clips[secondIdx];
    if (first.track != second.track || std::abs(first.timelineEndSec() - second.timelineStartSec) > 0.05)
        return;
    if (m_project.laneLocked(second.track)) {
        setStatus("Lane is locked");
        return;
    }

    static const QStringList kinds{"none", "fade", "dissolve", "wipeleft", "wiperight", "slideleft", "slideright"};
    if (!kinds.contains(kind))
        return;

    // A transition can never be longer than half of either clip, or it would eat the clip.
    const double maxDur = std::max(0.1, std::min(first.durationSec, second.durationSec) * 0.5);
    const double dur = std::clamp(durationSec, 0.1, maxDur);

    pushUndo();
    m_project.clips[secondIdx].transitionIn = kind;
    m_project.clips[secondIdx].transitionDurationSec = dur;
    emit timelineChanged();
    emit previewChanged();
    setStatus(kind == "none" ? QString("Removed the transition into %1").arg(second.name)
                             : QString("%1 into %2 (%3 s)").arg(kind, second.name).arg(dur, 0, 'g', 2));
}

QVariantList Backend::lanesList() const {    QVariantList out;
    const QStringList names{"V1 main", "V2 overlay", "A1 audio"};
    for (int track = 0; track < 3; ++track) {
        out.append(QVariantMap{{"track", track},
                               {"name", names[track]},
                               {"muted", m_project.lanes[track].muted},
                               {"solo", m_project.lanes[track].solo},
                               {"locked", m_project.lanes[track].locked},
                               {"audible", m_project.laneAudible(track)}});
    }
    return out;
}

QVariantMap Backend::capabilities() const {
    return m_caps.toJson().toVariantMap();
}

// Detecting the toolchain runs several PATH and ffmpeg queries, so it happens once off the GUI thread.
void Backend::refreshCapabilities() {
    auto future = QtConcurrent::run([]() { return MediaEngine::detectCapabilities(); });
    auto *watcher = new QFutureWatcher<Capabilities>(this);
    connect(watcher, &QFutureWatcher<Capabilities>::finished, this, [this, watcher] {
        m_caps = watcher->result();
        watcher->deleteLater();
        emit capabilitiesChanged();
    });
    watcher->setFuture(future);
}

void Backend::newProject() {
    m_undoStack.clear();
    m_redoStack.clear();
    m_project = Project();
    m_projectPath.clear();
    m_dirty = false;
    m_selectedClipIds.clear();
    m_selectedClipId.clear();
    m_selectionAnchorId.clear();
    m_selectedTextId.clear();
    setPlaying(false);
    setPlayheadSec(0.0);
    emit projectChanged();
    emit assetsChanged();
    emit timelineChanged();
    emit selectionChanged();
    emit previewChanged();
    setStatus("New project created");
}

bool Backend::openProject(const QString &pathOrUrl) {
    const QString path = normalizeLocalPath(pathOrUrl);
    bool ok = false;
    QString err;
    Project loaded = Project::loadFromFile(path, &ok, &err);
    if (!ok) {
        fail("Failed to open project: " + err);
        return false;
    }
    m_undoStack.clear();
    m_redoStack.clear();
    m_project = loaded;
    m_projectPath = path;
    m_dirty = false;
    m_selectedClipIds.clear();
    m_selectedClipId.clear();
    m_selectionAnchorId.clear();
    m_selectedTextId.clear();
    setPlaying(false);
    setPlayheadSec(0.0);
    emit projectChanged();
    emit assetsChanged();
    emit timelineChanged();
    emit selectionChanged();
    emit previewChanged();
    setStatus("Opened " + QFileInfo(path).fileName());
    for (const auto &asset : m_project.assets)
        startPreviewJob(asset);
    return true;
}

bool Backend::saveProject(const QString &pathOrUrl) {
    QString path = normalizeLocalPath(pathOrUrl);
    if (path.isEmpty())
        path = m_projectPath;
    if (path.isEmpty())
        return false;
    if (!path.endsWith(".cutcap"))
        path += ".cutcap";
    QString err;
    if (!m_project.saveToFile(path, &err)) {
        fail("Failed to save project: " + err);
        return false;
    }
    m_projectPath = path;
    m_dirty = false;
    emit projectChanged();
    setStatus("Saved " + QFileInfo(path).fileName());
    return true;
}

void Backend::setProjectTitle(const QString &title) {
    if (title.trimmed().isEmpty() || title == m_project.title)
        return;
    pushUndo();
    m_project.title = title.trimmed();
    emit projectChanged();
}

void Backend::setProjectResolution(int width, int height) {
    if (width <= 0 || height <= 0)
        return;
    pushUndo();
    m_project.width = width;
    m_project.height = height;
    emit projectChanged();
}

void Backend::importMedia(const QVariantList &pathsOrUrls) {
    for (const auto &v : pathsOrUrls)
        importSingleMedia(v.toString());
}

void Backend::importSingleMedia(const QString &pathOrUrl) {
    const QString path = normalizeLocalPath(pathOrUrl);
    if (path.isEmpty())
        return;
    if (path.endsWith(".cutcap", Qt::CaseInsensitive)) {
        openProject(path);
        return;
    }
    if (path.endsWith(".srt", Qt::CaseInsensitive)) {
        importSrtFile(path);
        return;
    }

    // Already in the bin: append another clip for it instead of re-probing.
    for (const auto &existing : m_project.assets) {
        if (existing.path == QFileInfo(path).absoluteFilePath()) {
            addAssetToTimeline(existing.id, existing.kind == "audio" ? 2 : 0, -1.0);
            return;
        }
    }

    importAssetAsync(path, [this](const QString &assetId) {
        const int idx = m_project.findAssetIndex(assetId);
        if (idx < 0)
            return;
        addAssetToTimeline(assetId, m_project.assets[idx].kind == "audio" ? 2 : 0, -1.0);
        setStatus("Imported " + m_project.assets[idx].name);
    });
}

void Backend::importAssetAsync(const QString &pathOrUrl, const std::function<void(const QString &)> &onReady) {
    const QString path = normalizeLocalPath(pathOrUrl);
    if (path.isEmpty()) {
        onReady({});
        return;
    }
    const QString absolute = QFileInfo(path).absoluteFilePath();
    for (const auto &existing : m_project.assets) {
        if (existing.path == absolute) {
            onReady(existing.id);
            return;
        }
    }
    if (!QFileInfo::exists(path)) {
        fail("File not found: " + path);
        onReady({});
        return;
    }

    ++m_importing;
    emit importStateChanged();
    auto future = QtConcurrent::run([path]() -> std::pair<MediaAsset, QString> {
        QString err;
        MediaAsset asset = MediaEngine::probeMedia(path, &err);
        return {asset, err};
    });
    auto *watcher = new QFutureWatcher<std::pair<MediaAsset, QString>>(this);
    connect(watcher, &QFutureWatcher<std::pair<MediaAsset, QString>>::finished, this,
            [this, watcher, onReady] {
                const auto res = watcher->result();
                watcher->deleteLater();
                --m_importing;
                emit importStateChanged();
                if (!res.second.isEmpty()) {
                    fail(res.second);
                    onReady({});
                    return;
                }
                const MediaAsset asset = res.first;
                pushUndo();
                if (m_project.findAssetIndex(asset.id) < 0) {
                    m_project.assets.append(asset);
                    emit assetsChanged();
                }
                onReady(asset.id);
                startPreviewJob(asset);
            });
    watcher->setFuture(future);
}

void Backend::startPreviewJob(const MediaAsset &asset) {
    ++m_importing;
    emit importStateChanged();
    auto future = QtConcurrent::run([asset]() {
        MediaAsset prepared = asset;
        MediaEngine::preparePreviews(prepared);
        return prepared;
    });
    auto *watcher = new QFutureWatcher<MediaAsset>(this);
    connect(watcher, &QFutureWatcher<MediaAsset>::finished, this, [this, watcher, assetId = asset.id] {
        const MediaAsset prepared = watcher->result();
        watcher->deleteLater();
        --m_importing;
        emit importStateChanged();
        const int idx = m_project.findAssetIndex(assetId);
        if (idx < 0)
            return;
        m_project.assets[idx].thumbnailPath = prepared.thumbnailPath;
        m_project.assets[idx].filmstripPath = prepared.filmstripPath;
        m_project.assets[idx].waveformPeaks = prepared.waveformPeaks;
        emit assetsChanged();
        // Clips carry their asset's previews, so the timeline model has to be rebuilt as well.
        emit timelineChanged();
    });
    watcher->setFuture(future);
}

// Files dropped onto a lane: place each on the requested track at the drop time, packing them one
// after another. Imports run one at a time so each file is packed after the real duration of the
// previous one, and the GUI thread never waits on ffprobe.
struct Backend::ImportChain {
    Backend *backend = nullptr;
    QStringList queue;
    double cursor = 0.0;
    int track = 0;
};

void Backend::importMediaAt(const QVariantList &pathsOrUrls, int track, double atSec) {
    auto chain = std::make_shared<ImportChain>();
    chain->backend = this;
    chain->track = track;
    chain->cursor = std::max(0.0, atSec);
    for (const auto &v : pathsOrUrls)
        chain->queue.append(v.toString());
    processImportChain(chain);
}

void Backend::processImportChain(const std::shared_ptr<ImportChain> &chain) {
    while (!chain->queue.isEmpty()) {
        const QString path = normalizeLocalPath(chain->queue.takeFirst());
        if (path.endsWith(".cutcap", Qt::CaseInsensitive)) {
            openProject(path);
            continue;
        }
        if (path.endsWith(".srt", Qt::CaseInsensitive)) {
            importSrtFile(path);
            continue;
        }
        importAssetAsync(path, [this, chain](const QString &assetId) {
            const int idx = m_project.findAssetIndex(assetId);
            if (idx >= 0) {
                const MediaAsset &asset = m_project.assets[idx];
                const int lane = asset.kind == "audio" ? 2 : (chain->track == 2 ? 0 : std::clamp(chain->track, 0, 1));
                addAssetToTimeline(assetId, lane, chain->cursor);
                chain->cursor += asset.durationSec;
                setStatus(QString("Placed %1 on %2").arg(asset.name, lane == 2 ? "A1" : (lane == 1 ? "V2" : "V1")));
            }
            processImportChain(chain);
        });
        return;
    }
}

void Backend::removeAsset(const QString &assetId) {
    const int idx = m_project.findAssetIndex(assetId);
    if (idx < 0)
        return;
    pushUndo();
    const QString name = m_project.assets[idx].name;
    m_project.assets.removeAt(idx);
    for (int i = m_project.clips.size() - 1; i >= 0; --i) {
        if (m_project.clips[i].assetId == assetId)
            m_project.clips.removeAt(i);
    }
    pruneClipSelection();
    emit assetsChanged();
    emit timelineChanged();
    emit selectionChanged();
    emit previewChanged();
    setStatus("Removed " + name);
}

// Timeline edits snap within this distance of a neighbour's edge, the playhead or zero.
static constexpr double kSnapSec = 0.15;

double Backend::snapTime(double sec, int track, const QString &ignoreClipId) const {
    double snapped = std::max(0.0, sec);
    if (snapped < kSnapSec)
        return 0.0;
    if (std::abs(snapped - m_playheadSec) < kSnapSec)
        return m_playheadSec;
    for (const auto &c : m_project.clips) {
        if (c.id == ignoreClipId || c.track != track)
            continue;
        if (std::abs(snapped - c.timelineEndSec()) < kSnapSec)
            return c.timelineEndSec();
        if (std::abs(snapped - c.timelineStartSec) < kSnapSec)
            return c.timelineStartSec;
    }
    return snapped;
}

QString Backend::addAssetToTimeline(const QString &assetId, int track, double atSec) {
    const int idx = m_project.findAssetIndex(assetId);
    if (idx < 0)
        return {};
    const MediaAsset &asset = m_project.assets[idx];

    pushUndo();
    TimelineClip clip;
    clip.id = nextId("clip");
    clip.assetId = asset.id;
    clip.sourcePath = asset.path;
    clip.name = asset.name;
    clip.kind = asset.kind;
    clip.track = (asset.kind == "audio") ? 2 : std::clamp(track, 0, 1);
    clip.timelineStartSec = (atSec >= 0.0) ? snapTime(atSec, clip.track) : m_project.nextTrackEndSec(clip.track);
    clip.sourceInSec = 0.0;
    clip.durationSec = asset.durationSec;
    clip.sourceDurationSec = asset.durationSec;

    m_project.clips.append(clip);
    m_selectedClipIds = QStringList{clip.id};
    m_selectedClipId = clip.id;
    m_selectionAnchorId = clip.id;
    m_selectedTextId.clear();
    emit timelineChanged();
    emit selectionChanged();
    emit previewChanged();
    return clip.id;
}

void Backend::setLaneState(int track, const QString &key, bool value) {
    if (track < 0 || track > 2)
        return;
    pushUndo();
    LaneState &lane = m_project.lanes[track];
    if (key == "muted")
        lane.muted = value;
    else if (key == "solo")
        lane.solo = value;
    else if (key == "locked") {
        lane.locked = value;
        if (value) {
            // Drop selection from a lane that just became locked, so the inspector cannot edit it.
            QStringList kept;
            for (const QString &id : m_selectedClipIds) {
                const int idx = m_project.findClipIndex(id);
                if (idx < 0 || m_project.clips[idx].track != track)
                    kept.append(id);
            }
            if (kept.size() != m_selectedClipIds.size()) {
                m_selectedClipIds = kept;
                pruneClipSelection();
            }
        }
    }
    emit timelineChanged();
    emit previewStateChanged();
    setStatus(QString("%1 %2 %3").arg(lanesList()[track].toMap().value("name").toString(), key, value ? "on" : "off"));
}

// Live snap for a drag in progress: same rule as the commit, plus the guide position.
double Backend::previewSnap(const QString &clipId, double sec, int track) {
    const double snapped = snapTime(sec, track, clipId);
    const double guide = std::abs(snapped - std::max(0.0, sec)) > 1e-4 ? snapped : -1.0;
    if (!qFuzzyCompare(guide + 2.0, m_snapGuideSec + 2.0)) {
        m_snapGuideSec = guide;
        emit snapGuideChanged();
    }
    return snapped;
}

void Backend::clearSnapGuide() {
    if (m_snapGuideSec < 0.0)
        return;
    m_snapGuideSec = -1.0;
    emit snapGuideChanged();
}

void Backend::applyLookPreset(const QString &clipId, const QString &preset) {
    static const QMap<QString, QVariantMap> presets{
        {"neutral", {{"brightness", 0.0}, {"contrast", 1.0}, {"saturation", 1.0}, {"blur", 0.0}, {"vignette", false}}},
        {"punch", {{"brightness", 0.02}, {"contrast", 1.18}, {"saturation", 1.22}, {"blur", 0.0}, {"vignette", false}}},
        {"mono", {{"brightness", 0.0}, {"contrast", 1.12}, {"saturation", 0.0}, {"blur", 0.0}, {"vignette", false}}},
        {"warm", {{"brightness", 0.03}, {"contrast", 1.06}, {"saturation", 1.12}, {"blur", 0.0}, {"vignette", true}}},
        {"soft", {{"brightness", 0.02}, {"contrast", 0.94}, {"saturation", 0.9}, {"blur", 2.0}, {"vignette", true}}},
    };
    if (!presets.contains(preset))
        return;
    const int idx = m_project.findClipIndex(clipId);
    if (idx < 0 || m_project.laneLocked(m_project.clips[idx].track))
        return;
    pushUndo();
    auto &clip = m_project.clips[idx];
    const auto values = presets.value(preset);
    clip.brightness = values.value("brightness").toDouble();
    clip.contrast = values.value("contrast").toDouble();
    clip.saturation = values.value("saturation").toDouble();
    clip.blur = values.value("blur").toDouble();
    clip.vignette = values.value("vignette").toBool();
    emit timelineChanged();
    emit previewStateChanged();
    setStatus("Look: " + preset);
}

void Backend::pruneClipSelection() {
    QStringList kept;
    for (const QString &id : m_selectedClipIds) {
        if (m_project.findClipIndex(id) >= 0 && !kept.contains(id))
            kept.append(id);
    }
    const QString primary = kept.isEmpty() ? QString() : kept.last();
    if (kept == m_selectedClipIds && primary == m_selectedClipId)
        return;
    m_selectedClipIds = kept;
    m_selectedClipId = primary;
    emit selectionChanged();
}

void Backend::selectClip(const QString &clipId) {
    if (clipId.isEmpty()) {
        clearClipSelection();
        return;
    }
    // A plain click re-anchors even when the selection set does not change, so the next
    // shift-click ranges from the clip the user just clicked.
    m_selectionAnchorId = clipId;
    if (m_selectedClipIds == QStringList{clipId} && m_selectedClipId == clipId && m_selectedTextId.isEmpty())
        return;
    m_selectedClipIds = QStringList{clipId};
    m_selectedClipId = clipId;
    m_selectedTextId.clear();
    emit selectionChanged();
    emit timelineChanged();
}

void Backend::selectClipAdditive(const QString &clipId) {
    if (clipId.isEmpty())
        return;
    if (m_selectedClipIds.contains(clipId))
        m_selectedClipIds.removeAll(clipId);
    else
        m_selectedClipIds.append(clipId);
    m_selectedClipId = m_selectedClipIds.isEmpty() ? QString() : m_selectedClipIds.last();
    m_selectionAnchorId = clipId;
    m_selectedTextId.clear();
    pruneClipSelection();
    emit selectionChanged();
    emit timelineChanged();
}

void Backend::selectClipRange(const QString &clipId) {
    const int clickedIdx = m_project.findClipIndex(clipId);
    const int anchorIdx = m_project.findClipIndex(m_selectionAnchorId);
    if (clickedIdx < 0 || anchorIdx < 0 || m_project.clips[anchorIdx].track != m_project.clips[clickedIdx].track) {
        selectClip(clipId);
        return;
    }
    const int track = m_project.clips[clickedIdx].track;
    const double anchorStart = m_project.clips[anchorIdx].timelineStartSec;
    const double clickedStart = m_project.clips[clickedIdx].timelineStartSec;
    const double from = std::min(anchorStart, clickedStart);
    const double to = std::max(anchorStart, clickedStart);

    QList<int> inRange;
    for (int i = 0; i < m_project.clips.size(); ++i) {
        const TimelineClip &c = m_project.clips[i];
        if (c.track == track && c.timelineStartSec >= from && c.timelineStartSec <= to)
            inRange.append(i);
    }
    std::sort(inRange.begin(), inRange.end(), [this](int a, int b) {
        return m_project.clips[a].timelineStartSec < m_project.clips[b].timelineStartSec;
    });

    QStringList ids;
    for (int idx : inRange)
        ids.append(m_project.clips[idx].id);
    ids.removeAll(clipId);
    ids.append(clipId);

    m_selectedClipIds = ids;
    m_selectedClipId = clipId;
    m_selectedTextId.clear();
    emit selectionChanged();
    emit timelineChanged();
}

void Backend::selectAllClips() {
    QList<int> order;
    for (int i = 0; i < m_project.clips.size(); ++i)
        order.append(i);
    std::sort(order.begin(), order.end(), [this](int a, int b) {
        const TimelineClip &ca = m_project.clips[a];
        const TimelineClip &cb = m_project.clips[b];
        return ca.track == cb.track ? ca.timelineStartSec < cb.timelineStartSec : ca.track < cb.track;
    });
    QStringList ids;
    for (int idx : order)
        ids.append(m_project.clips[idx].id);
    m_selectedClipIds = ids;
    m_selectedClipId = ids.isEmpty() ? QString() : ids.last();
    m_selectionAnchorId = m_selectedClipId;
    m_selectedTextId.clear();
    emit selectionChanged();
    emit timelineChanged();
}

void Backend::clearClipSelection() {
    if (m_selectedClipIds.isEmpty() && m_selectedClipId.isEmpty())
        return;
    m_selectedClipIds.clear();
    m_selectedClipId.clear();
    m_selectionAnchorId.clear();
    emit selectionChanged();
    emit timelineChanged();
}

void Backend::selectText(const QString &textId) {
    if (m_selectedTextId == textId && m_selectedClipId.isEmpty() && m_selectedClipIds.isEmpty())
        return;
    m_selectedTextId = textId;
    m_selectedClipId.clear();
    m_selectedClipIds.clear();
    m_selectionAnchorId.clear();
    emit selectionChanged();
    emit timelineChanged();
}

void Backend::moveClip(const QString &clipId, double newStartSec, int newTrack) {
    const int idx = m_project.findClipIndex(clipId);
    if (idx < 0 || m_project.laneLocked(m_project.clips[idx].track))
        return;
    pushUndo();
    const int targetTrack = m_project.clips[idx].kind == "audio" ? 2 : std::clamp(newTrack, 0, 1);
    m_project.clips[idx].timelineStartSec = snapTime(newStartSec, targetTrack, m_project.clips[idx].id);
    if (m_project.clips[idx].kind != "audio")
        m_project.clips[idx].track = std::clamp(newTrack, 0, 1);
    emit timelineChanged();
    emit previewChanged();
}

void Backend::trimClip(const QString &clipId, double newStartSec, double newDurationSec, double newSourceInSec) {
    const int idx = m_project.findClipIndex(clipId);
    if (idx < 0 || m_project.laneLocked(m_project.clips[idx].track))
        return;
    pushUndo();
    auto &c = m_project.clips[idx];
    c.timelineStartSec = std::max(0.0, newStartSec);
    c.sourceInSec = std::clamp(newSourceInSec, 0.0, std::max(0.0, c.sourceDurationSec - 0.1));
    c.durationSec = std::max(0.2, newDurationSec);
    emit timelineChanged();
    emit previewChanged();
}

void Backend::splitAtPlayhead() {
    auto containsPlayhead = [&](const TimelineClip &c) {
        return m_playheadSec > c.timelineStartSec + 0.1 && m_playheadSec < c.timelineEndSec() - 0.1;
    };

    // A selected clip under the playhead wins; otherwise every clip under it is split.
    QList<int> targets;
    for (int i = 0; i < m_project.clips.size(); ++i) {
        const TimelineClip &c = m_project.clips[i];
        if (containsPlayhead(c) && !m_project.laneLocked(c.track) && m_selectedClipIds.contains(c.id))
            targets.append(i);
    }
    if (targets.isEmpty()) {
        for (int i = 0; i < m_project.clips.size(); ++i) {
            const TimelineClip &c = m_project.clips[i];
            if (containsPlayhead(c) && !m_project.laneLocked(c.track))
                targets.append(i);
        }
    }
    if (targets.isEmpty()) {
        setStatus("Place playhead inside a clip to split");
        return;
    }

    pushUndo();
    QStringList rightIds;
    std::sort(targets.begin(), targets.end(), [](int a, int b) { return a > b; });
    for (int targetIdx : targets) {
        TimelineClip left = m_project.clips[targetIdx];
        TimelineClip right = left;
        const double splitOffset = m_playheadSec - left.timelineStartSec;

        left.durationSec = splitOffset;
        right.id = nextId("clip");
        right.timelineStartSec = m_playheadSec;
        right.sourceInSec = left.sourceInSec + splitOffset * left.speed;
        right.durationSec = std::max(0.1, right.durationSec - splitOffset);
        right.transitionIn = "none";

        m_project.clips[targetIdx] = left;
        m_project.clips.insert(targetIdx + 1, right);
        rightIds.prepend(right.id);
    }

    m_selectedClipIds = rightIds;
    m_selectedClipId = rightIds.last();
    m_selectionAnchorId = m_selectedClipId;
    m_selectedTextId.clear();
    emit timelineChanged();
    emit selectionChanged();
    emit previewChanged();
    setStatus("Split clip at " + formatTimecode(m_playheadSec));
}

void Backend::deleteSelected(bool ripple) {
    if (!m_selectedTextId.isEmpty()) {
        const int idx = m_project.findTextIndex(m_selectedTextId);
        if (idx >= 0) {
            pushUndo();
            m_project.texts.removeAt(idx);
            m_selectedTextId.clear();
            emit timelineChanged();
            emit selectionChanged();
            emit playheadChanged();
            setStatus("Deleted text overlay");
        }
        return;
    }

    // A clip that no longer exists, or that sits on a locked lane, is not deletable.
    QList<int> indices;
    QList<TimelineClip> removed;
    for (const QString &id : m_selectedClipIds) {
        const int idx = m_project.findClipIndex(id);
        if (idx < 0 || m_project.laneLocked(m_project.clips[idx].track))
            continue;
        indices.append(idx);
        removed.append(m_project.clips[idx]);
    }
    if (indices.isEmpty()) {
        if (!m_selectedClipIds.isEmpty())
            setStatus("Lane is locked");
        return;
    }

    pushUndo();
    std::sort(indices.begin(), indices.end(), [](int a, int b) { return a > b; });
    for (int idx : indices)
        m_project.clips.removeAt(idx);

    // Ripple shifts each lane by the removed time that lies before the survivor, read from the
    // original positions so two removals in front of one clip still add up.
    if (ripple) {
        for (auto &c : m_project.clips) {
            double shift = 0.0;
            for (const TimelineClip &gone : removed) {
                if (gone.track == c.track && gone.timelineStartSec < c.timelineStartSec)
                    shift += gone.durationSec;
            }
            c.timelineStartSec = std::max(0.0, c.timelineStartSec - shift);
        }
    }

    m_selectedClipIds.clear();
    m_selectedClipId.clear();
    m_selectionAnchorId.clear();
    emit timelineChanged();
    emit selectionChanged();
    emit previewChanged();
    const int count = indices.size();
    const QString plural = count == 1 ? QString() : QStringLiteral("s");
    setStatus(ripple ? QString("Ripple deleted %1 clip%2").arg(count).arg(plural)
                     : QString("Deleted %1 clip%2").arg(count).arg(plural));
}

bool Backend::moveSelectedClips(double deltaSec, int deltaTrack) {
    if (m_selectedClipIds.isEmpty())
        return false;

    QList<int> indices;
    double minStart = std::numeric_limits<double>::max();
    int minTrack = 1;
    int maxTrack = 0;
    bool hasAudio = false;
    for (const QString &id : m_selectedClipIds) {
        const int idx = m_project.findClipIndex(id);
        if (idx < 0)
            continue;
        const TimelineClip &c = m_project.clips[idx];
        if (m_project.laneLocked(c.track)) {
            setStatus("Lane is locked");
            return false;
        }
        indices.append(idx);
        minStart = std::min(minStart, c.timelineStartSec);
        const bool audio = c.kind == "audio" || c.track == 2;
        hasAudio = hasAudio || audio;
        if (!audio) {
            minTrack = std::min(minTrack, c.track);
            maxTrack = std::max(maxTrack, c.track);
        }
    }
    if (indices.isEmpty())
        return false;

    deltaSec = std::max(deltaSec, -minStart);
    if (hasAudio)
        deltaTrack = 0;
    else
        deltaTrack = std::clamp(deltaTrack, -minTrack, 1 - maxTrack);

    // A click that never moved must not cost an undo step.
    if (std::abs(deltaSec) < 1e-9 && deltaTrack == 0)
        return true;

    pushUndo();
    for (int idx : indices) {
        TimelineClip &c = m_project.clips[idx];
        c.timelineStartSec = std::max(0.0, c.timelineStartSec + deltaSec);
        if (c.kind != "audio" && c.track != 2)
            c.track = std::clamp(c.track + deltaTrack, 0, 1);
    }
    emit timelineChanged();
    emit previewChanged();
    return true;
}

void Backend::updateClipProperty(const QString &clipId, const QString &key, const QVariant &value) {
    const int idx = m_project.findClipIndex(clipId);
    if (idx < 0 || m_project.laneLocked(m_project.clips[idx].track))
        return;
    pushUndo("clip:" + clipId + ":" + key);
    auto &c = m_project.clips[idx];
    if (key == "volume") c.volume = std::clamp(value.toDouble(), 0.0, 2.0);
    else if (key == "muted") c.muted = value.toBool();
    else if (key == "speed") {
        const double oldSpeed = c.speed;
        c.speed = std::clamp(value.toDouble(), 0.25, 4.0);
        c.durationSec = std::max(0.2, c.durationSec * (oldSpeed / c.speed));
    }
    else if (key == "brightness") c.brightness = std::clamp(value.toDouble(), -0.5, 0.5);
    else if (key == "contrast") c.contrast = std::clamp(value.toDouble(), 0.5, 2.0);
    else if (key == "saturation") c.saturation = std::clamp(value.toDouble(), 0.0, 2.0);
    else if (key == "blur") c.blur = std::clamp(value.toDouble(), 0.0, 20.0);
    else if (key == "vignette") c.vignette = value.toBool();
    else if (key == "transitionIn") c.transitionIn = value.toString();
    else if (key == "transitionDurationSec") c.transitionDurationSec = std::clamp(value.toDouble(), 0.1, 3.0);
    else if (key == "audioDenoise") c.audioDenoise = value.toBool();
    else if (key == "audioVoiceClarity") c.audioVoiceClarity = value.toBool();
    else if (key == "audioNormalize") c.audioNormalize = value.toBool();
    else if (key == "audioCompress") c.audioCompress = value.toBool();
    else if (key == "bgRemovalMode") c.bgRemovalMode = value.toString();
    else if (key == "chromaColor") c.chromaColor = sanitizedChromaColor(value.toString());
    else if (key == "chromaSimilarity") c.chromaSimilarity = std::clamp(value.toDouble(), 0.01, 0.6);
    else if (key == "chromaBlend") c.chromaBlend = std::clamp(value.toDouble(), 0.0, 0.5);
    else if (key == "track") c.track = std::clamp(value.toInt(), 0, 2);

    emit timelineChanged();
    emit playheadChanged();
}

void Backend::addKeyframeAtPlayhead(const QString &clipId) {
    const int idx = m_project.findClipIndex(clipId);
    if (idx < 0 || m_project.laneLocked(m_project.clips[idx].track))
        return;
    pushUndo();
    auto &c = m_project.clips[idx];
    const double local = std::clamp(m_playheadSec - c.timelineStartSec, 0.0, c.durationSec);
    Keyframe kf = c.evaluateAt(local);
    kf.timeSec = local;
    for (int i = 0; i < c.keyframes.size(); ++i) {
        if (std::abs(c.keyframes[i].timeSec - local) < 0.05) {
            c.keyframes[i] = kf;
            emit timelineChanged();
            return;
        }
    }
    c.keyframes.append(kf);
    emit timelineChanged();
    setStatus(QString("Added keyframe at %1s").arg(local, 0, 'f', 2));
}

void Backend::updateTransformAtPlayhead(const QString &clipId, double x, double y,
                                        double scale, double opacity, double rotation) {
    const int idx = m_project.findClipIndex(clipId);
    if (idx < 0 || m_project.laneLocked(m_project.clips[idx].track))
        return;
    pushUndo("transform:" + clipId);
    auto &c = m_project.clips[idx];
    const double local = std::clamp(m_playheadSec - c.timelineStartSec, 0.0, c.durationSec);
    Keyframe kf{local, std::clamp(x, 0.0, 1.0), std::clamp(y, 0.0, 1.0),
                std::clamp(scale, 0.1, 3.0), std::clamp(opacity, 0.0, 1.0),
                std::clamp(rotation, -180.0, 180.0)};
    bool replaced = false;
    for (int i = 0; i < c.keyframes.size(); ++i) {
        if (std::abs(c.keyframes[i].timeSec - local) < 0.05) {
            c.keyframes[i] = kf;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        c.keyframes.append(kf);
    emit timelineChanged();
    emit playheadChanged();
}

void Backend::removeKeyframe(const QString &clipId, double timeSec) {
    const int idx = m_project.findClipIndex(clipId);
    if (idx < 0 || m_project.laneLocked(m_project.clips[idx].track))
        return;
    pushUndo();
    auto &kfs = m_project.clips[idx].keyframes;
    for (int i = 0; i < kfs.size(); ++i) {
        if (std::abs(kfs[i].timeSec - timeSec) < 0.05) {
            kfs.removeAt(i);
            break;
        }
    }
    emit timelineChanged();
    emit playheadChanged();
}

void Backend::addTextAtPlayhead(const QString &text, bool isCaption) {
    pushUndo();
    TextOverlay t;
    t.id = nextId("txt");
    t.text = text.isEmpty() ? "Title" : text;
    t.isCaption = isCaption;
    t.startSec = m_playheadSec;
    t.durationSec = 3.0;
    t.y = isCaption ? 0.84 : 0.5;
    t.fontSize = isCaption ? 38 : 48;
    m_project.texts.append(t);
    m_selectedTextId = t.id;
    m_selectedClipId.clear();
    m_selectedClipIds.clear();
    m_selectionAnchorId.clear();
    emit timelineChanged();
    emit selectionChanged();
    emit playheadChanged();
}

void Backend::updateTextProperty(const QString &textId, const QString &key, const QVariant &value) {
    const int idx = m_project.findTextIndex(textId);
    if (idx < 0)
        return;
    pushUndo("text:" + textId + ":" + key);
    auto &t = m_project.texts[idx];
    if (key == "text") t.text = value.toString();
    else if (key == "startSec") t.startSec = std::max(0.0, value.toDouble());
    else if (key == "durationSec") t.durationSec = std::max(0.2, value.toDouble());
    else if (key == "x") t.x = std::clamp(value.toDouble(), 0.05, 0.95);
    else if (key == "y") t.y = std::clamp(value.toDouble(), 0.05, 0.95);
    else if (key == "fontSize") t.fontSize = std::clamp(value.toInt(), 14, 140);
    else if (key == "color") t.color = value.toString();
    else if (key == "bgColor") t.bgColor = value.toString();
    else if (key == "bgOpacity") t.bgOpacity = std::clamp(value.toDouble(), 0.0, 1.0);
    else if (key == "bold") t.bold = value.toBool();
    emit timelineChanged();
    emit playheadChanged();
}

void Backend::moveText(const QString &textId, double newStartSec) {
    updateTextProperty(textId, "startSec", newStartSec);
}

void Backend::importSrtFile(const QString &pathOrUrl) {
    const QString path = normalizeLocalPath(pathOrUrl);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        fail("Cannot open SRT file: " + path);
        return;
    }
    const auto items = MediaEngine::parseSrt(QString::fromUtf8(f.readAll()), true);
    if (items.isEmpty()) {
        fail("No valid captions found in " + path);
        return;
    }
    pushUndo();
    m_project.texts.append(items);
    emit timelineChanged();
    emit playheadChanged();
    setStatus(QString("Imported %1 captions from SRT").arg(items.size()));
}

void Backend::exportSrtFile(const QString &pathOrUrl) {
    QString path = normalizeLocalPath(pathOrUrl);
    if (!path.endsWith(".srt"))
        path += ".srt";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail("Cannot write SRT file: " + path);
        return;
    }
    f.write(MediaEngine::exportSrt(m_project.texts).toUtf8());
    setStatus("Exported captions to " + QFileInfo(path).fileName());
}

void Backend::generateAutoCaptions(const QString &clipId, const QString &language) {
    int idx = m_project.findClipIndex(clipId);
    if (idx < 0 && !m_project.clips.isEmpty())
        idx = 0;
    if (idx < 0) {
        fail("Add a video or audio clip first to generate auto captions.");
        return;
    }
    const TimelineClip clip = m_project.clips[idx];
    setStatus("Generating local auto captions...");
    auto future = QtConcurrent::run([clip, language]() -> std::pair<QList<TextOverlay>, QString> {
        QString err;
        auto captions = MediaEngine::runAutoCaptions(clip.sourcePath, language, &err);
        for (auto &c : captions)
            c.startSec += clip.timelineStartSec;
        return {captions, err};
    });

    auto *watcher = new QFutureWatcher<std::pair<QList<TextOverlay>, QString>>(this);
    connect(watcher, &QFutureWatcher<std::pair<QList<TextOverlay>, QString>>::finished, this, [this, watcher] {
        const auto res = watcher->result();
        watcher->deleteLater();
        if (!res.second.isEmpty()) {
            fail(res.second);
            return;
        }
        if (res.first.isEmpty()) {
            setStatus("No speech detected in clip.");
            return;
        }
        pushUndo();
        m_project.texts.append(res.first);
        emit timelineChanged();
        emit playheadChanged();
        setStatus(QString("Added %1 auto captions").arg(res.first.size()));
    });
    watcher->setFuture(future);
}

void Backend::toggleVoiceoverRecording() {
    if (recordingVoiceover()) {
        m_voiceoverProcess.terminate();
        setStatus("Stopping voiceover recording...");
        return;
    }
    if (!m_caps.pwRecord) {
        fail("PipeWire recorder (`pw-record` / `parec`) not found.");
        return;
    }
    m_voiceoverStartSec = m_playheadSec;
    m_voiceoverPath = MediaEngine::cacheDir() + QString("/vo_%1.wav").arg(QDateTime::currentMSecsSinceEpoch());
    const QString cmd = !QStandardPaths::findExecutable("pw-record").isEmpty() ? "pw-record" : "parec";
    m_voiceoverProcess.start(cmd, {m_voiceoverPath});
    emit voiceoverStateChanged();
    setStatus("Recording voiceover... click again to stop");
}

void Backend::generateLocalTts(const QString &text) {
    if (text.trimmed().isEmpty()) {
        fail("Enter text for local TTS first.");
        return;
    }
    setStatus("Synthesizing local TTS...");
    const QString outPath = MediaEngine::cacheDir() + QString("/tts_%1.wav").arg(QDateTime::currentMSecsSinceEpoch());
    const double startSec = m_playheadSec;

    auto future = QtConcurrent::run([text, outPath]() -> QString {
        QString err;
        MediaEngine::runLocalTts(text, outPath, &err);
        return err;
    });
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, outPath, startSec] {
        const QString err = watcher->result();
        watcher->deleteLater();
        if (!err.isEmpty()) {
            fail(err);
            return;
        }
        importAssetAsync(outPath, [this, startSec](const QString &assetId) {
            if (assetId.isEmpty())
                return; // the import already reported the reason; no clip is moved and no success status is set
            addAssetToTimeline(assetId, 2, startSec);
            setStatus("Generated local TTS voiceover");
        });
    });
    watcher->setFuture(future);
}

void Backend::applyAiBackgroundRemoval(const QString &clipId) {
    const int idx = m_project.findClipIndex(clipId);
    if (idx < 0)
        return;
    const TimelineClip clip = m_project.clips[idx];
    const QString outPath = MediaEngine::cacheDir() + QString("/nobg_%1.png").arg(clip.id);
    setStatus("Running local AI background removal...");

    auto future = QtConcurrent::run([clip, outPath]() -> QString {
        QString err;
        MediaEngine::runAiBackgroundRemoval(clip.sourcePath, outPath, &err);
        return err;
    });
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, clipId, outPath] {
        const QString err = watcher->result();
        watcher->deleteLater();
        if (!err.isEmpty()) {
            fail(err);
            return;
        }
        const int idx2 = m_project.findClipIndex(clipId);
        if (idx2 >= 0) {
            pushUndo();
            m_project.clips[idx2].bgRemovalMode = "ai_rembg";
            m_project.clips[idx2].processedPath = outPath;
            emit timelineChanged();
            emit previewChanged();
            setStatus("Background removed locally");
        }
    });
    watcher->setFuture(future);
}

QVariantMap Backend::estimateExport(const QString &preset, const QString &codec,
                                      const QString &resolutionPreset) const {
    int width = m_project.width;
    int height = m_project.height;
    if (resolutionPreset == "4k") {
        width = 3840;
        height = 2160;
    } else if (resolutionPreset == "vertical") {
        width = 1080;
        height = 1920;
    }

    const double durationSec = m_project.totalDurationSec();
    const Capabilities &caps = m_caps;
    const QString encoder = caps.encoderNameForCodec(codec);
    const bool codecSupported = caps.ffmpeg && caps.supportsVideoCodec(codec);

    bool hasAudio = false;
    for (const auto &c : m_project.clips) {
        if (c.kind == "image" || c.muted || c.volume <= 0.001)
            continue;
        if (!m_project.laneAudible(c.kind == "audio" ? 2 : c.track))
            continue;
        if (c.kind == "audio") {
            hasAudio = true;
            break;
        }
        const int aIdx = m_project.findAssetIndex(c.assetId);
        if (aIdx >= 0 && m_project.assets[aIdx].hasAudio) {
            hasAudio = true;
            break;
        }
    }

    // Approximate video bitrate scales with pixel area, codec efficiency, and preset CRF.
    double baseKbps1080p = 6500.0;
    if (codec == "h265")
        baseKbps1080p = 4200.0;
    else if (codec == "av1")
        baseKbps1080p = 3200.0;

    double presetFactor = 1.0;
    if (preset == "medium")
        presetFactor = 0.72;
    else if (preset == "fast")
        presetFactor = 0.55;

    const double pixelScale = (double(width) * double(height)) / (1920.0 * 1080.0);
    const double videoKbps = baseKbps1080p * pixelScale * presetFactor;
    const double audioKbps = hasAudio ? 192.0 : 0.0;
    const qint64 estimatedBytes = durationSec > 0.0 && !m_project.clips.isEmpty()
                                      ? qint64(std::llround(((videoKbps + audioKbps) * 1000.0 / 8.0) * durationSec))
                                      : 0;

    QString sizeLabel = QStringLiteral("0 MB");
    if (estimatedBytes > 0) {
        const double mb = estimatedBytes / (1024.0 * 1024.0);
        sizeLabel = mb >= 1024.0
                        ? QString("~%1 GB").arg(mb / 1024.0, 0, 'f', 2)
                        : QString("~%1 MB").arg(std::max(0.1, mb), 0, 'f', mb >= 10.0 ? 0 : 1);
    }

    return QVariantMap{
        {"durationSec", durationSec},
        {"durationLabel", formatTimecode(durationSec)},
        {"width", width},
        {"height", height},
        {"resolutionLabel", QString("%1×%2").arg(width).arg(height)},
        {"encoder", encoder},
        {"codecSupported", codecSupported},
        {"hasAudio", hasAudio},
        {"estimatedBytes", estimatedBytes},
        {"estimatedSizeLabel", sizeLabel},
    };
}

void Backend::startExport(const QString &outputPathOrUrl, const QString &preset,
                          const QString &codec, const QString &resolutionPreset) {
    if (exporting())
        return;
    if (m_project.clips.isEmpty()) {
        fail("Timeline is empty — add at least one clip before exporting.");
        return;
    }

    const Capabilities &caps = m_caps;
    if (!caps.ffmpeg) {
        fail("FFmpeg is not installed — cannot export locally.");
        return;
    }
    if (!caps.supportsVideoCodec(codec)) {
        fail(QString("FFmpeg encoder %1 is not available on this system.").arg(caps.encoderNameForCodec(codec)));
        return;
    }

    QString outPath = normalizeLocalPath(outputPathOrUrl);
    if (outPath.isEmpty())
        return;
    if (!outPath.endsWith(".mp4") && !outPath.endsWith(".mkv") && !outPath.endsWith(".mov"))
        outPath += ".mp4";

    ExportOptions opts;
    opts.preset = preset;
    opts.codec = codec;
    if (resolutionPreset == "4k") {
        opts.width = 3840;
        opts.height = 2160;
    } else if (resolutionPreset == "vertical") {
        opts.width = 1080;
        opts.height = 1920;
    } else {
        opts.width = m_project.width;
        opts.height = m_project.height;
    }

    m_exportTempAss.clear();
    if (!m_project.texts.isEmpty()) {
        const QString ass = MediaEngine::cacheDir() + QString("/subs_%1.ass").arg(QDateTime::currentMSecsSinceEpoch());
        m_exportTempAss = MediaEngine::generateAssSubtitles(m_project, ass, opts.width, opts.height);
    }

    m_exportOutputPath = outPath;
    m_exportProgress = 0.0;
    const QStringList args = MediaEngine::buildExportArgs(m_project, outPath, opts, m_exportTempAss);
    m_exportProcess.start("ffmpeg", args);
    emit exportStateChanged();
    setStatus("Exporting " + QFileInfo(outPath).fileName() + "...");
}

void Backend::cancelExport() {
    if (exporting()) {
        m_exportProcess.kill();
        setStatus("Export cancelled");
    }
}

void Backend::undo() {
    if (m_undoStack.isEmpty())
        return;
    m_undoMergeKey.clear();
    m_redoStack.append(m_project);
    m_project = m_undoStack.takeLast();
    pruneClipSelection();
    m_dirty = true;
    emit projectChanged();
    emit assetsChanged();
    emit timelineChanged();
    emit selectionChanged();
    emit previewChanged();
    setStatus("Undo");
}

void Backend::redo() {
    if (m_redoStack.isEmpty())
        return;
    m_undoMergeKey.clear();
    m_undoStack.append(m_project);
    m_project = m_redoStack.takeLast();
    pruneClipSelection();
    m_dirty = true;
    emit projectChanged();
    emit assetsChanged();
    emit timelineChanged();
    emit selectionChanged();
    emit previewChanged();
    setStatus("Redo");
}

void Backend::syncPlayheadFromMedia(const QString &lane, qint64 mediaPosMs) {
    if (!m_playing || lane != clockLane())
        return;
    const auto *clip = clipAtPlayhead(laneFromName(lane));
    if (!clip)
        return;
    const double next = clip->timelineStartSec + std::max(0.0, (mediaPosMs / 1000.0 - clip->sourceInSec) / clip->speed);
    // Positions reported right after a source switch or seek belong to the previous media; never run backwards.
    if (next < m_playheadSec - 0.25)
        return;
    const double total = m_project.totalDurationSec();
    if (total > 0.0 && next >= total) {
        setPlaying(false);
        setPlayheadSec(0.0);
        return;
    }
    if (std::abs(next - m_playheadSec) < 0.004)
        return;
    advancePlayhead(next);
}

void Backend::laneReachedEnd(const QString &lane) {
    if (!m_playing || lane != clockLane())
        return;
    const auto *clip = clipAtPlayhead(laneFromName(lane));
    if (!clip)
        return;
    const double end = clip->timelineEndSec();
    const double total = m_project.totalDurationSec();
    if (total > 0.0 && end >= total - 1e-3) {
        setPlaying(false);
        setPlayheadSec(0.0);
        return;
    }
    advancePlayhead(end);
}

QString Backend::formatTimecode(double sec) const {
    sec = std::max(0.0, sec);
    const int mins = int(sec) / 60;
    const int secs = int(sec) % 60;
    const int frames = int(std::round((sec - std::floor(sec)) * 30.0)) % 30;
    return QString("%1:%2.%3")
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'))
        .arg(frames, 2, 10, QChar('0'));
}
