#pragma once

#include "project.h"
#include <QObject>
#include <QProcess>
#include <QStringList>

struct Capabilities {
    bool ffmpeg = false;
    bool ffprobe = false;
    bool pwRecord = false;
    bool hasH264 = false;
    bool hasH265 = false;
    bool hasAv1 = false;
    bool hasAac = false;
    QString whisperCmd; // whisper-cli | whisper | faster-whisper
    QString ttsCmd;     // piper | espeak-ng | espeak
    QString rembgCmd;   // rembg

    bool supportsVideoCodec(const QString &codec) const;
    QString encoderNameForCodec(const QString &codec) const;
    QJsonObject toJson() const;
};

struct ExportOptions {
    QString preset = "high"; // high | medium | fast
    QString codec = "h264";  // h264 | h265 | av1
    int width = 0;           // 0 = use project width
    int height = 0;          // 0 = use project height
};

class MediaEngine {
public:

    static Capabilities detectCapabilities();
    static QString cacheDir();
    static QString mediaDir();
    // Deletes cache files older than maxAgeDays, except the paths in keepPaths. Returns the count.
    static int pruneCache(const QString &dir, int maxAgeDays = 7, const QStringList &keepPaths = {});

    static MediaAsset probeMedia(const QString &path, QString *error = nullptr);
    static QString ensureThumbnail(const MediaAsset &asset);
    static QString ensureFilmstrip(const MediaAsset &asset);
    static QVector<double> extractWaveform(const QString &path, int buckets = 80);
    // Fills any missing thumbnail, filmstrip and waveform (e.g. projects written by the CLI).
    static void preparePreviews(MediaAsset &asset);

    static constexpr int kFilmstripFrames = 24;
    static constexpr int kFilmstripTileWidth = 128;
    static constexpr int kFilmstripTileHeight = 72;

    static QString generateAssSubtitles(const Project &project, const QString &assPath, int width, int height);
    static QStringList buildExportArgs(const Project &project, const QString &outputPath,
                                       const ExportOptions &opts, const QString &assPath = {});

    static QList<TextOverlay> parseSrt(const QString &srtContent, bool asCaptions = true);
    static QString formatSrtTime(double sec);
    static QString exportSrt(const QList<TextOverlay> &texts);

    // Synchronous helpers for CLI / background threads
    // spanStartSec/spanLengthSec limit the transcription to part of the file; cue times are then
    // relative to spanStartSec. A negative length means "to the end".
    static QList<TextOverlay> runAutoCaptions(const QString &mediaPath, const QString &language = "en",
                                              QString *error = nullptr, double spanStartSec = 0.0,
                                              double spanLengthSec = -1.0);
    // Maps cues that are relative to the clip's source-in point onto the timeline.
    static QList<TextOverlay> captionsForClip(const QList<TextOverlay> &cues, const TimelineClip &clip);
    static bool runLocalTts(const QString &text, const QString &outputWavPath, QString *error = nullptr);
    static bool runAiBackgroundRemoval(const QString &inputPath, const QString &outputPath, QString *error = nullptr);
};
