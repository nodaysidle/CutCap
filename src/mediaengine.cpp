#include "mediaengine.h"

#include <QColor>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>

bool Capabilities::supportsVideoCodec(const QString &codec) const {
    if (codec == "h265")
        return hasH265;
    if (codec == "av1")
        return hasAv1;
    return hasH264;
}

QString Capabilities::encoderNameForCodec(const QString &codec) const {
    if (codec == "h265")
        return QStringLiteral("libx265");
    if (codec == "av1")
        return QStringLiteral("libsvtav1");
    return QStringLiteral("libx264");
}

QJsonObject Capabilities::toJson() const {
    return QJsonObject{
        {"ffmpeg", ffmpeg},
        {"ffprobe", ffprobe},
        {"pwRecord", pwRecord},
        {"whisper", whisperCmd},
        {"tts", ttsCmd},
        {"rembg", rembgCmd},
        {"encoders", QJsonObject{
            {"h264", hasH264},
            {"h265", hasH265},
            {"av1", hasAv1},
            {"aac", hasAac},
        }},
    };
}

Capabilities MediaEngine::detectCapabilities() {
    Capabilities c;
    c.ffmpeg = !QStandardPaths::findExecutable("ffmpeg").isEmpty();
    c.ffprobe = !QStandardPaths::findExecutable("ffprobe").isEmpty();
    c.pwRecord = !QStandardPaths::findExecutable("pw-record").isEmpty() ||
                 !QStandardPaths::findExecutable("parec").isEmpty();

    if (c.ffmpeg) {
        static QString cachedEncoders;
        static QString cachedFfmpegPath;
        const QString ffmpegPath = QStandardPaths::findExecutable("ffmpeg");
        if (cachedFfmpegPath != ffmpegPath) {
            QProcess proc;
            proc.start(ffmpegPath, {"-hide_banner", "-v", "error", "-encoders"});
            if (proc.waitForFinished(4000) && proc.exitCode() == 0)
                cachedEncoders = QString::fromUtf8(proc.readAllStandardOutput());
            else
                cachedEncoders.clear();
            cachedFfmpegPath = ffmpegPath;
        }
        c.hasH264 = cachedEncoders.contains(QRegularExpression(R"(\blibx264\b)"));
        c.hasH265 = cachedEncoders.contains(QRegularExpression(R"(\blibx265\b)"));
        c.hasAv1 = cachedEncoders.contains(QRegularExpression(R"(\blibsvtav1\b)"));
        c.hasAac = cachedEncoders.contains(QRegularExpression(R"(\baac\b)"));
    }

    for (const char *cmd : {"whisper-cli", "faster-whisper", "whisper"}) {
        if (!QStandardPaths::findExecutable(cmd).isEmpty()) {
            c.whisperCmd = cmd;
            break;
        }
    }
    for (const char *cmd : {"piper", "piper-tts", "espeak-ng", "espeak"}) {
        if (!QStandardPaths::findExecutable(cmd).isEmpty()) {
            c.ttsCmd = cmd;
            break;
        }
    }
    if (!QStandardPaths::findExecutable("rembg").isEmpty())
        c.rembgCmd = "rembg";

    return c;
}

QString MediaEngine::cacheDir() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
    return dir;
}

// Recordings and generated media a project depends on; unlike the cache this is never swept.
QString MediaEngine::mediaDir() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/media";
    QDir().mkpath(dir);
    return dir;
}

int MediaEngine::pruneCache(const QString &dir, int maxAgeDays, const QStringList &keepPaths) {
    if (dir.isEmpty() || maxAgeDays <= 0)
        return 0;
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-maxAgeDays);
    int removed = 0;
    const QFileInfoList entries = QDir(dir).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        // Only previews and temporary subtitle files can be rebuilt; anything else in here may be
        // a recording a saved project still points at.
        const QString name = entry.fileName();
        if (!name.startsWith("thumb_") && !name.startsWith("strip_") && !name.startsWith("subs_"))
            continue;
        if (keepPaths.contains(entry.absoluteFilePath()) || entry.lastModified() >= cutoff)
            continue;
        if (QFile::remove(entry.absoluteFilePath()))
            ++removed;
    }
    return removed;
}

static QString hashForPath(const QString &path) {
    QFileInfo fi(path);
    const QString key = QString("%1:%2:%3").arg(fi.absoluteFilePath()).arg(fi.size()).arg(fi.lastModified().toMSecsSinceEpoch());
    return QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex());
}

MediaAsset MediaEngine::probeMedia(const QString &path, QString *error) {
    MediaAsset asset;
    QFileInfo fi(path);
    if (!fi.exists()) {
        if (error)
            *error = QString("File not found: %1").arg(path);
        return asset;
    }

    asset.id = hashForPath(path).left(12);
    asset.path = fi.absoluteFilePath();
    asset.name = fi.fileName();

    const QString ext = fi.suffix().toLower();
    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "bmp") {
        asset.kind = "image";
        asset.durationSec = 5.0;
        asset.hasAudio = false;
    }

    QProcess proc;
    proc.start("ffprobe", {"-v", "quiet", "-print_format", "json", "-show_format", "-show_streams", asset.path});
    if (!proc.waitForFinished(10000) || proc.exitCode() != 0) {
        if (error)
            *error = QString("ffprobe failed for %1").arg(path);
        return asset;
    }

    const auto doc = QJsonDocument::fromJson(proc.readAllStandardOutput()).object();
    const auto fmt = doc.value("format").toObject();
    const double fmtDur = fmt.value("duration").toString().toDouble();
    if (fmtDur > 0.0 && asset.kind != "image")
        asset.durationSec = fmtDur;

    bool hasVideo = false;
    bool hasAudio = false;
    for (const auto &v : doc.value("streams").toArray()) {
        const auto s = v.toObject();
        const QString codecType = s.value("codec_type").toString();
        if (codecType == "video") {
            // An MP3/FLAC/M4A carries its cover art as a video stream with attached_pic set.
            // Counting it as a picture track turns an audio file into a "video" clip on V1.
            const bool attachedPicture = s.value("disposition").toObject().value("attached_pic").toInt() == 1;
            if (attachedPicture)
                continue;
            if (!hasVideo) {
                hasVideo = true;
                asset.width = s.value("width").toInt(1920);
                asset.height = s.value("height").toInt(1080);
                const QString rateStr = s.value("r_frame_rate").toString("30/1");
                const auto parts = rateStr.split('/');
                if (parts.size() == 2 && parts[1].toDouble() > 0)
                    asset.fps = std::clamp(parts[0].toDouble() / parts[1].toDouble(), 1.0, 240.0);
            }
        } else if (codecType == "audio") {
            hasAudio = true;
        }
    }

    if (asset.kind != "image") {
        asset.kind = hasVideo ? "video" : "audio";
        asset.hasAudio = hasAudio;
    }
    return asset;
}

QString MediaEngine::ensureThumbnail(const MediaAsset &asset) {
    if (asset.kind == "audio" || asset.path.isEmpty())
        return {};
    const QString out = cacheDir() + "/thumb_" + hashForPath(asset.path).left(16) + ".jpg";
    if (QFileInfo::exists(out))
        return out;

    QProcess proc;
    // Stills have a single frame; seeking into them yields nothing.
    const bool seek = asset.kind == "video" && asset.durationSec > 1.0;
    QStringList args{"-y", "-v", "error"};
    if (seek)
        args << "-ss" << "0.5";
    args << "-i" << asset.path << "-frames:v" << "1" << "-vf" << "scale=320:-2" << "-q:v" << "5" << out;
    proc.start("ffmpeg", args);
    proc.waitForFinished(8000);
    return QFileInfo::exists(out) ? out : QString();
}

// Frames for the timeline strip: one row of kFilmstripFrames tiles, sampled evenly across the source.
QString MediaEngine::ensureFilmstrip(const MediaAsset &asset) {
    if (asset.kind == "audio" || asset.path.isEmpty())
        return {};
    const QString out = cacheDir() + "/strip_" + hashForPath(asset.path).left(16) + ".jpg";
    if (QFileInfo::exists(out))
        return out;
    const int frames = asset.kind == "image" ? 1 : kFilmstripFrames;
    QStringList vf;
    if (asset.kind != "image")
        vf << QString("fps=%1/%2").arg(frames).arg(std::max(0.1, asset.durationSec), 0, 'f', 3);
    vf << QString("scale=%1:%2:force_original_aspect_ratio=increase,crop=%1:%2").arg(kFilmstripTileWidth).arg(kFilmstripTileHeight)
       << QString("tile=%1x1").arg(frames);
    QProcess proc;
    proc.start("ffmpeg", {"-y", "-v", "error", "-i", asset.path, "-vf", vf.join(','), "-frames:v", "1", "-q:v", "4", out});
    proc.waitForFinished(20000);
    return QFileInfo::exists(out) ? out : QString();
}

void MediaEngine::preparePreviews(MediaAsset &asset) {
    if (asset.thumbnailPath.isEmpty() || !QFileInfo::exists(asset.thumbnailPath))
        asset.thumbnailPath = ensureThumbnail(asset);
    if (asset.filmstripPath.isEmpty() || !QFileInfo::exists(asset.filmstripPath))
        asset.filmstripPath = ensureFilmstrip(asset);
    if (asset.hasAudio && asset.waveformPeaks.isEmpty())
        asset.waveformPeaks = extractWaveform(asset.path, 80);
}

QVector<double> MediaEngine::extractWaveform(const QString &path, int buckets) {
    QVector<double> peaks(buckets, 0.0);
    if (path.isEmpty() || buckets <= 0)
        return peaks;

    QProcess proc;
    proc.start("ffmpeg", {"-v", "quiet", "-i", path, "-ac", "1", "-ar", "4000", "-f", "s16le", "-"});
    if (!proc.waitForFinished(12000))
        return peaks;

    const QByteArray pcm = proc.readAllStandardOutput();
    const int totalSamples = pcm.size() / 2;
    if (totalSamples <= 0)
        return peaks;

    const auto *samples = reinterpret_cast<const int16_t *>(pcm.constData());
    const int step = std::max(1, totalSamples / buckets);
    double maxPeak = 1e-6;
    for (int b = 0; b < buckets; ++b) {
        const int start = b * step;
        const int end = std::min(totalSamples, start + step);
        double sum = 0.0;
        for (int i = start; i < end; ++i)
            sum = std::max(sum, std::abs(samples[i]) / 32768.0);
        peaks[b] = sum;
        maxPeak = std::max(maxPeak, sum);
    }
    for (double &p : peaks)
        p = std::clamp(p / maxPeak, 0.05, 1.0);
    return peaks;
}

static QString toAssColor(const QString &hex, double opacity = 1.0) {
    QColor c(hex);
    if (!c.isValid())
        c = QColor("#eceff4");
    const int alpha = std::clamp(int(std::round((1.0 - opacity) * 255.0)), 0, 255);
    return QString("&H%1%2%3%4")
        .arg(alpha, 2, 16, QChar('0'))
        .arg(c.blue(), 2, 16, QChar('0'))
        .arg(c.green(), 2, 16, QChar('0'))
        .arg(c.red(), 2, 16, QChar('0'))
        .toUpper();
}

static QString formatAssTime(double sec) {
    sec = std::max(0.0, sec);
    const int h = int(sec) / 3600;
    const int m = (int(sec) % 3600) / 60;
    const int s = int(sec) % 60;
    const int cs = int(std::round((sec - std::floor(sec)) * 100.0)) % 100;
    return QString("%1:%2:%3.%4")
        .arg(h)
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(cs, 2, 10, QChar('0'));
}

QString MediaEngine::generateAssSubtitles(const Project &project, const QString &assPath, int width, int height) {
    if (project.texts.isEmpty())
        return {};

    QString out;
    out += "[Script Info]\n";
    out += "ScriptType: v4.00+\n";
    out += QString("PlayResX: %1\nPlayResY: %2\n").arg(width).arg(height);
    out += "ScaledBorderAndShadow: yes\n\n";
    out += "[V4+ Styles]\n";
    out += "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, "
           "Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
           "Alignment, MarginL, MarginR, MarginV, Encoding\n";
    out += "Style: Default,Inter,44,&H00ECF0F4,&H000000FF,&H40090A0C,&H80090A0C,-1,0,0,0,100,100,0,0,3,8,0,5,24,24,24,1\n\n";
    out += "[Events]\n";
    out += "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";

    for (const auto &t : project.texts) {
        if (t.text.trimmed().isEmpty())
            continue;
        const int px = int(std::round(t.x * width));
        const int py = int(std::round(t.y * height));
        const QString primary = toAssColor(t.color, 1.0);
        const QString back = toAssColor(t.bgColor, t.bgOpacity);
        QString escaped = t.text;
        escaped.replace("\\", "\\\\");
        escaped.replace("\n", "\\N");
        escaped.replace("{", "\\{").replace("}", "\\}");

        const QString overrideTag = QString("{\\an5\\pos(%1,%2)\\fs%3\\b%4\\1c%5\\3c%6\\4c%6}")
                                        .arg(px)
                                        .arg(py)
                                        .arg(t.fontSize)
                                        .arg(t.bold ? 1 : 0)
                                        .arg(primary)
                                        .arg(back);

        out += QString("Dialogue: 0,%1,%2,Default,,0,0,0,,%3%4\n")
                   .arg(formatAssTime(t.startSec), formatAssTime(t.endSec()), overrideTag, escaped);
    }

    QFile f(assPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(out.toUtf8());
        return assPath;
    }
    return {};
}

static QString buildAtempoChain(double speed) {
    QStringList filters;
    double s = std::clamp(speed, 0.25, 4.0);
    while (s > 2.0) {
        filters << "atempo=2.0";
        s /= 2.0;
    }
    while (s < 0.5) {
        filters << "atempo=0.5";
        s /= 0.5;
    }
    filters << QString("atempo=%1").arg(s, 0, 'f', 4);
    return filters.join(',');
}

static bool isXfadeTransition(const QString &t) {
    return t == "fade" || t == "dissolve" || t == "wipeleft" || t == "wiperight" || t == "slideleft" || t == "slideright";
}

// FFmpeg expression for one keyframed property at time `t` (clip-local seconds), using the same
// smoothstep interpolation as TimelineClip::evaluateAt so export matches the preview.
static QString keyframeExpr(QList<Keyframe> kfs, double Keyframe::*field, const QString &t) {
    if (kfs.isEmpty())
        return QString::number(Keyframe{}.*field, 'f', 6);
    std::sort(kfs.begin(), kfs.end(), [](const Keyframe &a, const Keyframe &b) { return a.timeSec < b.timeSec; });
    QString expr = QString::number(kfs.last().*field, 'f', 6);
    for (int i = kfs.size() - 2; i >= 0; --i) {
        const double a = kfs[i].*field;
        const double b = kfs[i + 1].*field;
        const double span = kfs[i + 1].timeSec - kfs[i].timeSec;
        QString segment = QString::number(b, 'f', 6);
        if (span > 1e-6) {
            const QString u = QString("clip((%1-%2)/%3,0,1)").arg(t).arg(kfs[i].timeSec, 0, 'f', 6).arg(span, 0, 'f', 6);
            segment = QString("(%1+(%2)*%3*%3*(3-2*%3))").arg(a, 0, 'f', 6).arg(b - a, 0, 'f', 6).arg(u);
        }
        expr = QString("if(lte(%1,%2),%3,%4)").arg(t).arg(kfs[i + 1].timeSec, 0, 'f', 6).arg(segment, expr);
    }
    return QString("if(lte(%1,%2),%3,%4)").arg(t).arg(kfs.first().timeSec, 0, 'f', 6).arg(kfs.first().*field, 0, 'f', 6).arg(expr);
}

static bool keyframesAnimate(const QList<Keyframe> &kfs, double Keyframe::*field) {
    for (const auto &k : kfs) {
        if (std::abs(k.*field - kfs.first().*field) > 1e-6)
            return true;
    }
    return false;
}

static bool hasSpatialTransform(const QList<Keyframe> &kfs) {
    const Keyframe identity;
    for (const auto &k : kfs) {
        if (std::abs(k.x - identity.x) > 1e-4 || std::abs(k.y - identity.y) > 1e-4 ||
            std::abs(k.scale - identity.scale) > 1e-4 || std::abs(k.rotation - identity.rotation) > 1e-4)
            return true;
    }
    return false;
}

// Maps a canvas-sized frame the way the preview transforms its lane: scale and rotate around the
// centre, then move the centre to (x·W, y·H). A transparent margin keeps perspective's edge
// replication from smearing the clip's border across the canvas.
static QStringList transformFilters(const QList<Keyframe> &kfs, int w, int h, double fps) {
    constexpr int margin = 4;
    const QString t = QString("(in/%1)").arg(fps, 0, 'f', 4);
    const QString x = keyframeExpr(kfs, &Keyframe::x, t);
    const QString y = keyframeExpr(kfs, &Keyframe::y, t);
    const QString s = keyframeExpr(kfs, &Keyframe::scale, t);
    const QString a = QString("((%1)*PI/180)").arg(keyframeExpr(kfs, &Keyframe::rotation, t));
    const double hu = w / 2.0 + margin;
    const double hv = h / 2.0 + margin;
    QStringList corners;
    const int signs[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (int i = 0; i < 4; ++i) {
        const double u = signs[i][0] * hu;
        const double v = signs[i][1] * hv;
        corners << QString("x%1='%2+(%3)*%4+(%5)*((%6)*cos(%7)-(%8)*sin(%7))'")
                       .arg(i).arg(margin).arg(x).arg(w).arg(s).arg(u, 0, 'f', 2).arg(a).arg(v, 0, 'f', 2);
        corners << QString("y%1='%2+(%3)*%4+(%5)*((%6)*sin(%7)+(%8)*cos(%7))'")
                       .arg(i).arg(margin).arg(y).arg(h).arg(s).arg(u, 0, 'f', 2).arg(a).arg(v, 0, 'f', 2);
    }
    return {QString("pad=%1:%2:%3:%3:color=black@0").arg(w + 2 * margin).arg(h + 2 * margin).arg(margin),
            QString("perspective=%1:sense=destination:eval=frame").arg(corners.join(':')),
            QString("crop=%1:%2:%3:%3").arg(w).arg(h).arg(margin)};
}

QStringList MediaEngine::buildExportArgs(const Project &project, const QString &outputPath,
                                         const ExportOptions &opts, const QString &assPath) {
    const int outW = opts.width > 0 ? opts.width : project.width;
    const int outH = opts.height > 0 ? opts.height : project.height;
    const double fps = project.fps > 0.0 ? project.fps : 30.0;
    const double totalDur = std::max(0.5, project.totalDurationSec());

    QStringList args{"-y"};

    // Collect clips sorted by track then timelineStartSec
    QList<TimelineClip> mainVideoClips;
    QList<TimelineClip> overlayClips;
    QList<TimelineClip> audioClips;
    for (const auto &c : project.clips) {
        if (c.kind == "audio" || c.track == 2)
            audioClips.append(c);
        else if (c.track == 1)
            overlayClips.append(c);
        else
            mainVideoClips.append(c);
    }
    auto byStart = [](const TimelineClip &a, const TimelineClip &b) {
        return a.timelineStartSec < b.timelineStartSec;
    };
    std::sort(mainVideoClips.begin(), mainVideoClips.end(), byStart);
    std::sort(overlayClips.begin(), overlayClips.end(), byStart);
    std::sort(audioClips.begin(), audioClips.end(), byStart);

    struct ClipPlan {
        TimelineClip clip;
        int inputIdx = -1;
        bool isImage = false;
        bool isMain = false;
        int chain = -1;            // V1 clips joined by xfade transitions share a chain
        double xfadeInSec = 0.0;   // > 0: crossfade in from the previous chain member
        double padSec = 0.0;       // tail extension the next member crosses over
        double sourceExtSec = 0.0; // part of padSec backed by real source frames/audio (rest is a held frame)
    };

    // V1 transitions keep every clip at its timeline position: the outgoing clip is extended past
    // its end by the transition length (real source if available, else its last frame held), so
    // audio, overlays and captions stay in sync and the total length is unchanged.
    QList<ClipPlan> plans;
    int chainCount = 0;
    for (const auto &c : mainVideoClips) {
        ClipPlan p;
        p.clip = c;
        p.isMain = true;
        p.isImage = (c.kind == "image");
        if (!plans.isEmpty()) {
            ClipPlan &prev = plans.last();
            const double overlap = prev.clip.timelineEndSec() - c.timelineStartSec;
            const double d = std::min({c.transitionDurationSec, c.durationSec, prev.clip.durationSec});
            if (isXfadeTransition(c.transitionIn) && overlap > -0.05 && d > 0.05 &&
                c.timelineStartSec > prev.clip.timelineStartSec + 1e-3) {
                p.xfadeInSec = d;
                p.chain = prev.chain;
                prev.padSec = std::max(0.0, d - overlap);
                const double available =
                    prev.isImage ? prev.padSec
                                 : std::max(0.0, (prev.clip.sourceDurationSec - prev.clip.sourceInSec) / prev.clip.speed -
                                                     prev.clip.durationSec);
                prev.sourceExtSec = std::min(prev.padSec, available);
            }
        }
        if (p.chain < 0)
            p.chain = chainCount++;
        plans.append(p);
    }
    for (const auto &c : overlayClips) {
        ClipPlan p;
        p.clip = c;
        p.isImage = (c.kind == "image");
        plans.append(p);
    }
    for (const auto &c : audioClips) {
        ClipPlan p;
        p.clip = c;
        plans.append(p);
    }

    // Input 0: black base canvas
    args << "-f" << "lavfi" << "-i"
         << QString("color=c=black:s=%1x%2:r=%3:d=%4").arg(outW).arg(outH).arg(fps, 0, 'f', 2).arg(totalDur, 0, 'f', 3);

    int nextInput = 1;
    for (auto &p : plans) {
        const auto &c = p.clip;
        const QString path = (!c.processedPath.isEmpty() && QFileInfo::exists(c.processedPath))
                                 ? c.processedPath
                                 : c.sourcePath;
        const double lengthSec = c.durationSec + p.sourceExtSec;
        if (p.isImage) {
            args << "-loop" << "1" << "-t" << QString::number(lengthSec, 'f', 3) << "-i" << path;
        } else {
            args << "-ss" << QString::number(c.sourceInSec, 'f', 3)
                 << "-t" << QString::number(lengthSec * c.speed, 'f', 3)
                 << "-i" << path;
        }
        p.inputIdx = nextInput++;
    }

    QStringList filterParts;
    QString currentVideo = "[0:v]";
    int stepIdx = 0;

    auto buildVideoFiltersForClip = [&](const ClipPlan &p, double ptsOffsetSec, bool fadeFromBlack) -> QString {
        const auto &c = p.clip;
        QStringList vf;
        vf << QString("setpts=(PTS-STARTPTS)/%1+%2/TB")
                  .arg(c.speed, 0, 'f', 4)
                  .arg(ptsOffsetSec, 0, 'f', 3);
        if (p.padSec > 0.001) {
            vf << QString("tpad=stop_mode=clone:stop_duration=%1").arg(p.padSec, 0, 'f', 3);
            vf << QString("trim=duration=%1").arg(c.durationSec + p.padSec, 0, 'f', 3);
        }

        // Fit inside the canvas exactly like the preview frame. Effects run on the clip's own pixels
        // (as in the preview), then a transparent pad brings it to canvas size for the keyframed transform.
        vf << QString("scale=%1:%2:force_original_aspect_ratio=decrease").arg(outW).arg(outH);
        vf << QString("fps=%1").arg(fps, 0, 'f', 2);
        vf << "format=yuva420p";

        if (std::abs(c.brightness) > 0.001 || std::abs(c.contrast - 1.0) > 0.001 || std::abs(c.saturation - 1.0) > 0.001) {
            vf << QString("eq=brightness=%1:contrast=%2:saturation=%3")
                      .arg(c.brightness, 0, 'f', 3)
                      .arg(c.contrast, 0, 'f', 3)
                      .arg(c.saturation, 0, 'f', 3);
        }
        if (c.blur > 0.5)
            vf << QString("boxblur=%1:1").arg(c.blur, 0, 'f', 1);
        if (c.vignette)
            vf << "vignette";

        if (c.bgRemovalMode == "chromakey") {
            const QString hex = QColor(sanitizedChromaColor(c.chromaColor)).name(QColor::HexRgb).mid(1);
            vf << QString("chromakey=0x%1:%2:%3")
                      .arg(hex)
                      .arg(c.chromaSimilarity, 0, 'f', 3)
                      .arg(c.chromaBlend, 0, 'f', 3);
        }

        if (fadeFromBlack && c.transitionDurationSec > 0.05) {
            vf << QString("fade=t=in:st=%1:d=%2:alpha=1")
                      .arg(ptsOffsetSec, 0, 'f', 3)
                      .arg(std::min(c.transitionDurationSec, c.durationSec * 0.5), 0, 'f', 3);
        }
        vf << QString("pad=%1:%2:(ow-iw)/2:(oh-ih)/2:color=black@0").arg(outW).arg(outH);

        const Keyframe first = c.evaluateAt(0.0);
        if (keyframesAnimate(c.keyframes, &Keyframe::opacity)) {
            vf << QString("geq=lum='lum(X,Y)':cb='cb(X,Y)':cr='cr(X,Y)':a='alpha(X,Y)*(%1)'")
                      .arg(keyframeExpr(c.keyframes, &Keyframe::opacity, QString("(T-%1)").arg(ptsOffsetSec, 0, 'f', 3)));
        } else if (first.opacity < 0.99) {
            vf << QString("colorchannelmixer=aa=%1").arg(first.opacity, 0, 'f', 3);
        }
        if (hasSpatialTransform(c.keyframes))
            vf << transformFilters(c.keyframes, outW, outH, fps);
        return vf.join(',');
    };

    auto overlayOntoCanvas = [&](const QString &streamTag, const QString &posExpr, double startSec, double endSec) {
        const QString nextOut = QString("[vo%1]").arg(stepIdx++);
        filterParts << QString("%1%2overlay=%3:enable='between(t,%4,%5)':eof_action=pass%6")
                           .arg(currentVideo, streamTag, posExpr)
                           .arg(startSec, 0, 'f', 3)
                           .arg(endSec, 0, 'f', 3)
                           .arg(nextOut);
        currentVideo = nextOut;
    };

    // V1: each chain is built at local time 0, crossfaded member by member, then shifted to its start.
    for (int chain = 0; chain < chainCount; ++chain) {
        QString accumulated;
        double chainStart = 0.0;
        double chainEnd = 0.0;
        for (const auto &p : plans) {
            if (!p.isMain || p.chain != chain)
                continue;
            const QString tag = QString("[m%1]").arg(p.inputIdx);
            const bool fadeFromBlack = p.xfadeInSec <= 0.0 && p.clip.transitionIn != "none";
            filterParts << QString("[%1:v]%2%3").arg(p.inputIdx).arg(buildVideoFiltersForClip(p, 0.0, fadeFromBlack), tag);
            if (accumulated.isEmpty()) {
                accumulated = tag;
                chainStart = p.clip.timelineStartSec;
            } else {
                const QString out = QString("[x%1]").arg(p.inputIdx);
                filterParts << QString("%1%2xfade=transition=%3:duration=%4:offset=%5%6")
                                   .arg(accumulated, tag, p.clip.transitionIn)
                                   .arg(p.xfadeInSec, 0, 'f', 3)
                                   .arg(p.clip.timelineStartSec - chainStart, 0, 'f', 3)
                                   .arg(out);
                accumulated = out;
            }
            chainEnd = p.clip.timelineEndSec();
        }
        const QString shifted = QString("[c%1]").arg(chain);
        filterParts << QString("%1setpts=PTS+%2/TB%3").arg(accumulated).arg(chainStart, 0, 'f', 3).arg(shifted);
        overlayOntoCanvas(shifted, "x=0:y=0", chainStart, chainEnd);
    }

    // V2: overlays placed at their keyframed position on top of V1.
    for (const auto &p : plans) {
        if (p.isMain || p.clip.kind == "audio" || p.clip.track == 2)
            continue;
        const QString vTag = QString("[v%1]").arg(p.inputIdx);
        filterParts << QString("[%1:v]%2%3")
                           .arg(p.inputIdx)
                           .arg(buildVideoFiltersForClip(p, p.clip.timelineStartSec, p.clip.transitionIn != "none"), vTag);
        overlayOntoCanvas(vTag, "x=0:y=0", p.clip.timelineStartSec, p.clip.timelineEndSec());
    }

    if (!assPath.isEmpty()) {
        QString escapedAss = assPath;
        escapedAss.replace("\\", "\\\\").replace(":", "\\:").replace("'", "\\'");
        const QString subOut = "[vsub]";
        filterParts << QString("%1subtitles='%2'%3").arg(currentVideo, escapedAss, subOut);
        currentVideo = subOut;
    }

    // Final format for hardware compatibility
    filterParts << QString("%1format=yuv420p[vout]").arg(currentVideo);

    // Audio chain
    QStringList audioInputs;
    int aStep = 0;
    for (const auto &p : plans) {
        if (p.isImage || p.clip.muted || p.clip.volume <= 0.001)
            continue;
        // Lane mute/solo: a silenced track contributes no audio, and solo isolates a track.
        if (!project.laneAudible(p.clip.kind == "audio" ? 2 : p.clip.track))
            continue;
        if (p.clip.kind == "video") {
            const int idx = project.findAssetIndex(p.clip.assetId);
            if (idx >= 0 && !project.assets[idx].hasAudio)
                continue;
        }
        const auto &c = p.clip;
        QStringList af;
        af << buildAtempoChain(c.speed);
        if (std::abs(c.volume - 1.0) > 0.01)
            af << QString("volume=%1").arg(c.volume, 0, 'f', 3);
        if (c.audioVoiceClarity)
            af << "highpass=f=85,equalizer=f=3000:t=q:w=1.5:g=3";
        if (c.audioDenoise)
            af << "afftdn=nf=-25";
        if (c.audioCompress)
            af << "acompressor=threshold=-18dB:ratio=3:attack=5:release=50";
        if (c.audioNormalize)
            af << "loudnorm=I=-14:TP=-1.5:LRA=11";
        // Crossfade with the neighbouring V1 clip: the outgoing tail fades out while the next clip fades in.
        if (p.xfadeInSec > 0.0)
            af << QString("afade=t=in:st=0:d=%1").arg(p.xfadeInSec, 0, 'f', 3);
        if (p.sourceExtSec > 0.01)
            af << QString("afade=t=out:st=%1:d=%2").arg(c.durationSec, 0, 'f', 3).arg(p.sourceExtSec, 0, 'f', 3);
        const int delayMs = std::max(0, int(std::round(c.timelineStartSec * 1000.0)));
        if (delayMs > 0)
            af << QString("adelay=%1|%1").arg(delayMs);

        const QString aTag = QString("[a%1]").arg(aStep++);
        filterParts << QString("[%1:a]%2%3").arg(p.inputIdx).arg(af.join(','), aTag);
        audioInputs << aTag;
    }

    bool hasAudioOut = false;
    if (audioInputs.size() == 1) {
        filterParts << QString("%1anull[aout]").arg(audioInputs.first());
        hasAudioOut = true;
    } else if (audioInputs.size() > 1) {
        filterParts << QString("%1amix=inputs=%2:duration=longest:normalize=0[aout]")
                           .arg(audioInputs.join(""))
                           .arg(audioInputs.size());
        hasAudioOut = true;
    }

    args << "-filter_complex" << filterParts.join(";");
    args << "-map" << "[vout]";
    if (hasAudioOut)
        args << "-map" << "[aout]";

    // Codec & quality preset
    int crf = 18;
    QString xPreset = "medium";
    if (opts.preset == "medium") {
        crf = 22;
        xPreset = "fast";
    } else if (opts.preset == "fast") {
        crf = 25;
        xPreset = "veryfast";
    }

    if (opts.codec == "h265") {
        args << "-c:v" << "libx265" << "-crf" << QString::number(crf + 2) << "-preset" << xPreset;
    } else if (opts.codec == "av1") {
        args << "-c:v" << "libsvtav1" << "-crf" << QString::number(crf + 8) << "-preset" << "8";
    } else {
        args << "-c:v" << "libx264" << "-crf" << QString::number(crf) << "-preset" << xPreset;
    }

    if (hasAudioOut)
        args << "-c:a" << "aac" << "-b:a" << "192k";
    args << "-movflags" << "+faststart" << "-t" << QString::number(totalDur, 'f', 3) << outputPath;

    return args;
}

static double parseSrtTimestamp(const QString &ts) {
    // 00:01:23,456 or 00:01:23.456
    const QRegularExpression re(R"((\d+):(\d+):(\d+)[,\.](\d+))");
    const auto m = re.match(ts.trimmed());
    if (!m.hasMatch())
        return 0.0;
    return m.captured(1).toInt() * 3600.0 + m.captured(2).toInt() * 60.0 + m.captured(3).toInt() +
           m.captured(4).toInt() / 1000.0;
}

QList<TextOverlay> MediaEngine::parseSrt(const QString &srtContent, bool asCaptions) {
    QList<TextOverlay> out;
    QString normalized = srtContent;
    normalized.replace("\r\n", "\n");
    const auto blocks = normalized.split(QRegularExpression("\n{2,}"), Qt::SkipEmptyParts);
    int idx = 1;
    for (const auto &block : blocks) {
        const auto lines = block.split('\n', Qt::SkipEmptyParts);
        int timeLine = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].contains("-->")) {
                timeLine = i;
                break;
            }
        }
        if (timeLine < 0 || timeLine + 1 >= lines.size())
            continue;
        const auto times = lines[timeLine].split("-->");
        if (times.size() != 2)
            continue;
        const double start = parseSrtTimestamp(times[0]);
        const double end = parseSrtTimestamp(times[1]);
        QStringList textLines = lines.mid(timeLine + 1);

        TextOverlay t;
        t.id = QString("cap_%1_%2").arg(idx++).arg(int(start * 100));
        t.text = textLines.join("\n").trimmed();
        t.isCaption = asCaptions;
        t.startSec = start;
        t.durationSec = std::max(0.2, end - start);
        t.x = 0.5;
        t.y = asCaptions ? 0.84 : 0.5;
        t.fontSize = asCaptions ? 38 : 48;
        out.append(t);
    }
    return out;
}

QString MediaEngine::formatSrtTime(double sec) {
    sec = std::max(0.0, sec);
    const int h = int(sec) / 3600;
    const int m = (int(sec) % 3600) / 60;
    const int s = int(sec) % 60;
    const int ms = int(std::round((sec - std::floor(sec)) * 1000.0)) % 1000;
    return QString("%1:%2:%3,%4")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(ms, 3, 10, QChar('0'));
}

QString MediaEngine::exportSrt(const QList<TextOverlay> &texts) {
    QString out;
    int idx = 1;
    for (const auto &t : texts) {
        out += QString("%1\n%2 --> %3\n%4\n\n")
                   .arg(idx++)
                   .arg(formatSrtTime(t.startSec), formatSrtTime(t.endSec()), t.text);
    }
    return out;
}

QList<TextOverlay> MediaEngine::runAutoCaptions(const QString &mediaPath, const QString &language, QString *error) {
    const auto caps = detectCapabilities();
    if (caps.whisperCmd.isEmpty()) {
        if (error) {
            *error = "No local Whisper binary found (checked whisper-cli, faster-whisper, whisper). "
                     "Install one locally (e.g. `yay -S whisper.cpp` or `uv tool install faster-whisper`) "
                     "to enable offline auto-captions, or import an .srt file.";
        }
        return {};
    }

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        if (error)
            *error = "Failed to create temporary directory for audio extraction.";
        return {};
    }

    const QString wavPath = tmp.path() + "/audio.wav";
    QProcess ff;
    ff.start("ffmpeg", {"-y", "-i", mediaPath, "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le", wavPath});
    if (!ff.waitForFinished(60000) || ff.exitCode() != 0) {
        if (error)
            *error = "Failed to extract audio for captions.";
        return {};
    }

    QProcess wp;
    QString srtPath = tmp.path() + "/audio.srt";
    if (caps.whisperCmd == "whisper-cli") {
        wp.start("whisper-cli", {"-f", wavPath, "-l", language, "-osrt", "-of", tmp.path() + "/audio"});
    } else {
        wp.start(caps.whisperCmd, {wavPath, "--language", language, "--output_format", "srt", "--output_dir", tmp.path()});
    }
    if (!wp.waitForFinished(300000) || wp.exitCode() != 0) {
        if (error)
            *error = QString("Local %1 failed: %2").arg(caps.whisperCmd, QString::fromUtf8(wp.readAllStandardError()));
        return {};
    }

    QFile srtFile(srtPath);
    if (!srtFile.open(QIODevice::ReadOnly)) {
        if (error)
            *error = "Whisper finished but no .srt file was produced.";
        return {};
    }
    return parseSrt(QString::fromUtf8(srtFile.readAll()), true);
}

bool MediaEngine::runLocalTts(const QString &text, const QString &outputWavPath, QString *error) {
    const auto caps = detectCapabilities();
    if (caps.ttsCmd.isEmpty()) {
        if (error) {
            *error = "No local TTS engine found (checked piper, piper-tts, espeak-ng, espeak). "
                     "Install `espeak-ng` (`sudo pacman -S espeak-ng`) or `piper-tts` for offline voice generation.";
        }
        return false;
    }

    QProcess proc;
    if (caps.ttsCmd == "espeak-ng" || caps.ttsCmd == "espeak") {
        proc.start(caps.ttsCmd, {"-w", outputWavPath, text});
        if (!proc.waitForFinished(30000) || proc.exitCode() != 0) {
            if (error)
                *error = QString::fromUtf8(proc.readAllStandardError());
            return false;
        }
        return QFileInfo::exists(outputWavPath);
    }

    // Piper reads text from stdin
    proc.start(caps.ttsCmd, {"--output_file", outputWavPath});
    proc.write(text.toUtf8());
    proc.closeWriteChannel();
    if (!proc.waitForFinished(60000) || proc.exitCode() != 0) {
        if (error)
            *error = QString::fromUtf8(proc.readAllStandardError());
        return false;
    }
    return QFileInfo::exists(outputWavPath);
}

bool MediaEngine::runAiBackgroundRemoval(const QString &inputPath, const QString &outputPath, QString *error) {
    const auto caps = detectCapabilities();
    if (caps.rembgCmd.isEmpty()) {
        if (error) {
            *error = "Local `rembg` is not installed. Use built-in Chroma Key for instant background removal, "
                     "or install `rembg` (`uv tool install rembg[cli]`) for local AI segmentation.";
        }
        return false;
    }
    QProcess proc;
    proc.start(caps.rembgCmd, {"i", inputPath, outputPath});
    if (!proc.waitForFinished(120000) || proc.exitCode() != 0) {
        if (error)
            *error = QString::fromUtf8(proc.readAllStandardError());
        return false;
    }
    return QFileInfo::exists(outputPath);
}
