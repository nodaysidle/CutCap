#pragma once

#include "mediaengine.h"
#include "project.h"
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString projectTitle READ projectTitle NOTIFY projectChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY projectChanged)
    Q_PROPERTY(double totalDuration READ totalDuration NOTIFY timelineChanged)
    Q_PROPERTY(double playheadSec READ playheadSec WRITE setPlayheadSec NOTIFY playheadChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playbackChanged)
    Q_PROPERTY(double zoomPxPerSec READ zoomPxPerSec WRITE setZoomPxPerSec NOTIFY viewChanged)

    Q_PROPERTY(QString selectedClipId READ selectedClipId NOTIFY selectionChanged)
    Q_PROPERTY(QStringList selectedClipIds READ selectedClipIds NOTIFY selectionChanged)
    Q_PROPERTY(int selectedClipCount READ selectedClipCount NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedTextId READ selectedTextId NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedClip READ selectedClip NOTIFY timelineChanged)
    Q_PROPERTY(QVariantMap selectedText READ selectedText NOTIFY timelineChanged)

    Q_PROPERTY(QVariantList assetsList READ assetsList NOTIFY assetsChanged)
    Q_PROPERTY(QVariantList clipsList READ clipsList NOTIFY timelineChanged)
    Q_PROPERTY(QVariantList textsList READ textsList NOTIFY timelineChanged)
    Q_PROPERTY(QVariantList transitionPoints READ transitionPoints NOTIFY timelineChanged)
    Q_PROPERTY(QVariantList activeTexts READ activeTexts NOTIFY activeTextsChanged)
    Q_PROPERTY(QVariantList lanesList READ lanesList NOTIFY timelineChanged)
    Q_PROPERTY(double snapGuideSec READ snapGuideSec NOTIFY snapGuideChanged)
    Q_PROPERTY(QVariantMap capabilities READ capabilities NOTIFY capabilitiesChanged)
    Q_PROPERTY(int importing READ importing NOTIFY importStateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyStateChanged)

    Q_PROPERTY(bool canUndo READ canUndo NOTIFY timelineChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY timelineChanged)

    Q_PROPERTY(QVariantMap baseLane READ baseLane NOTIFY previewStateChanged)
    Q_PROPERTY(QVariantMap overlayLane READ overlayLane NOTIFY previewStateChanged)
    Q_PROPERTY(QVariantMap audioLane READ audioLane NOTIFY previewStateChanged)
    Q_PROPERTY(QString clockLane READ clockLane NOTIFY previewStateChanged)
    Q_PROPERTY(QVariantMap selectedClipTransform READ selectedClipTransform NOTIFY previewStateChanged)
    Q_PROPERTY(int projectWidth READ projectWidth NOTIFY projectChanged)
    Q_PROPERTY(int projectHeight READ projectHeight NOTIFY projectChanged)

    Q_PROPERTY(bool exporting READ exporting NOTIFY exportStateChanged)
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportStateChanged)
    Q_PROPERTY(bool recordingVoiceover READ recordingVoiceover NOTIFY voiceoverStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    const Project &project() const { return m_project; }
    QString projectTitle() const { return m_project.title; }
    QString projectPath() const { return m_projectPath; }
    bool dirty() const { return m_dirty; }
    double totalDuration() const { return std::max(10.0, m_project.totalDurationSec()); }
    double playheadSec() const { return m_playheadSec; }
    bool playing() const { return m_playing; }
    double zoomPxPerSec() const { return m_zoomPxPerSec; }

    QString selectedClipId() const { return m_selectedClipId; }
    QStringList selectedClipIds() const { return m_selectedClipIds; }
    int selectedClipCount() const { return m_selectedClipIds.size(); }
    QString selectedTextId() const { return m_selectedTextId; }
    QVariantMap selectedClip() const;
    QVariantMap selectedText() const;

    QVariantList assetsList() const;
    QVariantList clipsList() const;
    QVariantList textsList() const;
    // Junctions where two adjacent clips on one lane touch, so the timeline can offer a transition.
    QVariantList transitionPoints() const;
    QVariantList activeTexts() const;
    QVariantList lanesList() const;
    double snapGuideSec() const { return m_snapGuideSec; }
    QVariantMap capabilities() const;
    int importing() const { return m_importing; }
    bool busy() const { return m_busyJobs > 0; }

    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }

    QVariantMap baseLane() const { return laneMap(Lane::Base); }
    QVariantMap overlayLane() const { return laneMap(Lane::Overlay); }
    QVariantMap audioLane() const { return laneMap(Lane::Audio); }
    QString clockLane() const;
    QVariantMap selectedClipTransform() const;
    int projectWidth() const { return m_project.width; }
    int projectHeight() const { return m_project.height; }

    bool exporting() const { return m_exportProcess.state() != QProcess::NotRunning; }
    double exportProgress() const { return m_exportProgress; }
    bool recordingVoiceover() const { return m_voiceoverProcess.state() != QProcess::NotRunning; }
    QString statusMessage() const { return m_statusMessage; }

    void setPlayheadSec(double sec);
    void setPlaying(bool play);
    void setZoomPxPerSec(double px);

    Q_INVOKABLE void newProject();
    Q_INVOKABLE bool openProject(const QString &pathOrUrl);
    Q_INVOKABLE bool saveProject(const QString &pathOrUrl = {});
    Q_INVOKABLE void refreshCapabilities();
    Q_INVOKABLE void setProjectTitle(const QString &title);
    Q_INVOKABLE void setProjectResolution(int width, int height);

    Q_INVOKABLE void importMedia(const QVariantList &pathsOrUrls);
    Q_INVOKABLE void importSingleMedia(const QString &pathOrUrl);
    Q_INVOKABLE void importMediaAt(const QVariantList &pathsOrUrls, int track, double atSec);
    Q_INVOKABLE void setLaneState(int track, const QString &key, bool value);
    // Snapped position for the drag in progress, and whether a guide line should be shown.
    Q_INVOKABLE double previewSnap(const QString &clipId, double sec, int track);
    Q_INVOKABLE void clearSnapGuide();
    Q_INVOKABLE void applyLookPreset(const QString &clipId, const QString &preset);
    Q_INVOKABLE void removeAsset(const QString &assetId);
    // Appends a clip for the asset; returns the new clip id, empty when the asset is unknown.
    Q_INVOKABLE QString addAssetToTimeline(const QString &assetId, int track = 0, double atSec = -1.0);

    Q_INVOKABLE void selectClip(const QString &clipId);
    Q_INVOKABLE void selectClipAdditive(const QString &clipId);
    Q_INVOKABLE void selectClipRange(const QString &clipId);
    Q_INVOKABLE void selectAllClips();
    Q_INVOKABLE void clearClipSelection();
    Q_INVOKABLE void selectText(const QString &textId);

    Q_INVOKABLE bool moveSelectedClips(double deltaSec, int deltaTrack);

    Q_INVOKABLE void moveClip(const QString &clipId, double newStartSec, int newTrack);
    // Sets the transition that carries the first clip into the second (they must touch on one lane).
    Q_INVOKABLE void setTransitionBetween(const QString &firstId, const QString &secondId,
                                          const QString &kind, double durationSec = 0.5);
    Q_INVOKABLE void trimClip(const QString &clipId, double newStartSec, double newDurationSec, double newSourceInSec);
    Q_INVOKABLE void splitAtPlayhead();
    Q_INVOKABLE void deleteSelected(bool ripple = false);
    Q_INVOKABLE void updateClipProperty(const QString &clipId, const QString &key, const QVariant &value);
    Q_INVOKABLE void addKeyframeAtPlayhead(const QString &clipId);
    Q_INVOKABLE void updateTransformAtPlayhead(const QString &clipId, double x, double y,
                                                 double scale, double opacity, double rotation);
    Q_INVOKABLE void removeKeyframe(const QString &clipId, double timeSec);

    Q_INVOKABLE void addTextAtPlayhead(const QString &text = "New Title", bool isCaption = false);
    Q_INVOKABLE void updateTextProperty(const QString &textId, const QString &key, const QVariant &value);
    Q_INVOKABLE void moveText(const QString &textId, double newStartSec);
    Q_INVOKABLE void importSrtFile(const QString &pathOrUrl);
    Q_INVOKABLE void exportSrtFile(const QString &pathOrUrl);

    Q_INVOKABLE void generateAutoCaptions(const QString &clipId, const QString &language = "en");
    Q_INVOKABLE void toggleVoiceoverRecording();
    Q_INVOKABLE void generateLocalTts(const QString &text);
    Q_INVOKABLE void applyAiBackgroundRemoval(const QString &clipId);

    Q_INVOKABLE QVariantMap estimateExport(const QString &preset, const QString &codec,
                                           const QString &resolutionPreset) const;
    Q_INVOKABLE void startExport(const QString &outputPathOrUrl, const QString &preset,
                                 const QString &codec, const QString &resolutionPreset);
    Q_INVOKABLE void cancelExport();

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void syncPlayheadFromMedia(const QString &lane, qint64 mediaPosMs);
    Q_INVOKABLE void laneReachedEnd(const QString &lane);
    Q_INVOKABLE QString formatTimecode(double sec) const;

signals:
    void projectChanged();
    void assetsChanged();
    void timelineChanged();
    void selectionChanged();
    void playheadChanged();
    void playbackChanged();
    void viewChanged();
    void previewChanged();
    void previewStateChanged();
    void seekRequested();
    void activeTextsChanged();
    void snapGuideChanged();
    void exportStateChanged();
    void exportFinished(bool ok, const QString &message);
    void voiceoverStateChanged();
    void statusChanged();
    void errorOccurred(const QString &message);
    void capabilitiesChanged();
    void importStateChanged();
    void busyStateChanged();

private:
    enum class Lane { Base = 0, Overlay = 1, Audio = 2 };
    static QString laneName(Lane lane);
    static Lane laneFromName(const QString &name);

    void pushUndo(const QString &mergeKey = {});
    void setStatus(const QString &msg);
    void fail(const QString &msg);
    QString normalizeLocalPath(const QString &pathOrUrl) const;
    // Probes `path` off the GUI thread, adds the asset, then calls `onReady` on the GUI thread with
    // the new asset id (empty when the probe failed; the reason is already reported).
    void importAssetAsync(const QString &pathOrUrl, const std::function<void(const QString &)> &onReady);
    // Fills one asset's thumbnail, filmstrip and waveform off the GUI thread and merges them back.
    void startPreviewJob(const MediaAsset &asset);
    // One dropped batch: the files still to place and where the next one starts.
    struct ImportChain;
    void processImportChain(const std::shared_ptr<ImportChain> &chain);
    // Snaps a timeline time to a neighbour's edge, the playhead or zero, within kSnapSec.
    double snapTime(double sec, int track, const QString &ignoreClipId = {}) const;
    const TimelineClip *clipAtPlayhead(Lane lane) const;
    QVariantMap laneMap(Lane lane) const;
    void advancePlayhead(double sec);
    void refreshLanes();
    void refreshActiveTexts(bool force);
    QString nextId(const QString &prefix);
    void pruneClipSelection();

    Project m_project;
    QString m_projectPath;
    bool m_dirty = false;
    double m_playheadSec = 0.0;
    double m_snapGuideSec = -1.0;
    bool m_playing = false;
    double m_zoomPxPerSec = 48.0;
    QString m_selectedClipId;
    QStringList m_selectedClipIds;
    QString m_selectionAnchorId;
    QString m_selectedTextId;
    QString m_statusMessage = "Ready — drop clips or press Ctrl+O";

    QList<Project> m_undoStack;
    QList<Project> m_redoStack;
    int m_idCounter = 1;
    Capabilities m_caps;
    int m_importing = 0;
    int m_busyJobs = 0;
    // Bumped whenever the project is replaced, so a job started for the old one can drop its result.
    int m_jobGeneration = 0;

    QTimer m_playbackTimer;
    QElapsedTimer m_playbackClock;
    QString m_undoMergeKey;
    QElapsedTimer m_undoMergeClock;
    QString m_laneClipIds[3];
    QStringList m_activeTextIds;

    QProcess m_exportProcess;
    double m_exportProgress = 0.0;
    QString m_exportTempAss;
    QString m_exportOutputPath;
    QString m_exportStderrTail; // last 2 KB of ffmpeg stderr, kept because the ready-read handler drains it
    bool m_exportCancelled = false;

    QProcess m_voiceoverProcess;
    QString m_voiceoverPath;
    double m_voiceoverStartSec = 0.0;
};
