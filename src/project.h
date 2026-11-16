#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector>

struct Keyframe {
    double timeSec = 0.0;
    double x = 0.5;
    double y = 0.5;
    double scale = 1.0;
    double opacity = 1.0;
    double rotation = 0.0;

    QJsonObject toJson() const;
    static Keyframe fromJson(const QJsonObject &obj);
};

struct MediaAsset {
    QString id;
    QString path;
    QString name;
    QString kind = "video"; // video | audio | image
    double durationSec = 5.0;
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
    bool hasAudio = true;
    QString thumbnailPath;
    QString filmstripPath; // MediaEngine::kFilmstripFrames frames side by side, evenly spread over the source
    QVector<double> waveformPeaks;

    QJsonObject toJson() const;
    static MediaAsset fromJson(const QJsonObject &obj);
};

struct TimelineClip {
    QString id;
    QString assetId;
    QString sourcePath;
    QString name;
    QString kind = "video";
    int track = 0; // 0 = Main Video, 1 = Overlay/PiP, 2 = Audio/VO
    double timelineStartSec = 0.0;
    double sourceInSec = 0.0;
    double durationSec = 5.0;
    double sourceDurationSec = 5.0;
    double speed = 1.0;
    double volume = 1.0;
    bool muted = false;

    // Visual effects
    double brightness = 0.0; // -0.5 .. 0.5
    double contrast = 1.0;   // 0.5 .. 2.0
    double saturation = 1.0; // 0.0 .. 2.0
    double blur = 0.0;       // 0.0 .. 20.0
    bool vignette = false;

    // Transitions (applied at clip start)
    QString transitionIn = "none"; // none | fade | dissolve | wipeleft | wiperight | slideleft | slideright
    double transitionDurationSec = 0.5;

    // Audio cleanup
    bool audioDenoise = false;
    bool audioVoiceClarity = false;
    bool audioNormalize = false;
    bool audioCompress = false;

    // Background removal
    QString bgRemovalMode = "none"; // none | chromakey | ai_rembg
    QString chromaColor = "#00ff00";
    double chromaSimilarity = 0.18;
    double chromaBlend = 0.08;
    QString processedPath;

    QList<Keyframe> keyframes;

    double timelineEndSec() const { return timelineStartSec + durationSec; }
    Keyframe evaluateAt(double localTimeSec) const;

    QJsonObject toJson() const;
    static TimelineClip fromJson(const QJsonObject &obj);
};

struct TextOverlay {
    QString id;
    QString text = "Title";
    bool isCaption = false;
    double startSec = 0.0;
    double durationSec = 3.0;
    double x = 0.5;
    double y = 0.82;
    int fontSize = 44;
    QString color = "#eceff4";
    QString bgColor = "#090a0c";
    double bgOpacity = 0.65;
    bool bold = true;

    double endSec() const { return startSec + durationSec; }

    QJsonObject toJson() const;
    static TextOverlay fromJson(const QJsonObject &obj);
};

// Lane-level state: muting silences a whole track without touching every clip, solo overrides,
// locking protects a track from edits.
struct LaneState {
    bool muted = false;
    bool solo = false;
    bool locked = false;

    QJsonObject toJson() const;
    static LaneState fromJson(const QJsonObject &obj);
};

struct Project {
    QString title = "Untitled Project";
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
    LaneState lanes[3]; // index = track: 0 V1, 1 V2, 2 A1
    QList<MediaAsset> assets;
    QList<TimelineClip> clips;
    QList<TextOverlay> texts;

    double totalDurationSec() const;
    bool laneAudible(int track) const;
    bool laneLocked(int track) const;
    QJsonObject toJson() const;
    static Project fromJson(const QJsonObject &obj);
    bool saveToFile(const QString &path, QString *error = nullptr) const;
    static Project loadFromFile(const QString &path, bool *ok = nullptr, QString *error = nullptr);

    int findClipIndex(const QString &id) const;
    int findTextIndex(const QString &id) const;
    int findAssetIndex(const QString &id) const;
    double nextTrackEndSec(int track) const;
};
