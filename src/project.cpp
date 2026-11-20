#include "project.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>
#include <cmath>

QString sanitizedChromaColor(const QString &raw) {
    static const QRegularExpression re(QStringLiteral("^#?[0-9A-Fa-f]{6}$"));
    const QString trimmed = raw.trimmed();
    if (!re.match(trimmed).hasMatch())
        return QStringLiteral("#00ff00");
    QString normalized = trimmed;
    if (normalized.startsWith(QLatin1Char('#')))
        normalized.remove(0, 1);
    return QColor(QStringLiteral("#") + normalized).name();
}

// Anything that is not an absolute path (a URL, an ffmpeg protocol such as concat:, a relative
// path) is dropped: ffmpeg would open it as a network or protocol input.
QString localMediaPath(const QString &raw) {
    return QDir::isAbsolutePath(raw) ? raw : QString();
}

namespace {

int evenDimension(int v) {
    const int clamped = std::clamp(v, 16, 8192);
    return clamped - (clamped % 2);
}

}

QJsonObject Keyframe::toJson() const {
    return QJsonObject{
        {"timeSec", timeSec},
        {"x", x},
        {"y", y},
        {"scale", scale},
        {"opacity", opacity},
        {"rotation", rotation},
    };
}

Keyframe Keyframe::fromJson(const QJsonObject &obj) {
    Keyframe k;
    k.timeSec = std::max(0.0, obj.value("timeSec").toDouble(0.0));
    k.x = std::clamp(obj.value("x").toDouble(0.5), 0.0, 1.0);
    k.y = std::clamp(obj.value("y").toDouble(0.5), 0.0, 1.0);
    k.scale = std::clamp(obj.value("scale").toDouble(1.0), 0.1, 3.0);
    k.opacity = std::clamp(obj.value("opacity").toDouble(1.0), 0.0, 1.0);
    k.rotation = std::clamp(obj.value("rotation").toDouble(0.0), -180.0, 180.0);
    return k;
}

QJsonObject MediaAsset::toJson() const {
    QJsonArray peaks;
    for (double p : waveformPeaks)
        peaks.append(p);
    return QJsonObject{
        {"id", id},
        {"path", path},
        {"name", name},
        {"kind", kind},
        {"durationSec", durationSec},
        {"width", width},
        {"height", height},
        {"fps", fps},
        {"hasAudio", hasAudio},
        {"thumbnailPath", thumbnailPath},
        {"filmstripPath", filmstripPath},
        {"waveformPeaks", peaks},
    };
}

MediaAsset MediaAsset::fromJson(const QJsonObject &obj) {
    MediaAsset a;
    a.id = obj.value("id").toString();
    a.path = localMediaPath(obj.value("path").toString());
    a.name = obj.value("name").toString();
    a.kind = obj.value("kind").toString("video");
    a.durationSec = std::max(0.1, obj.value("durationSec").toDouble(5.0));
    a.width = evenDimension(obj.value("width").toInt(1920));
    a.height = evenDimension(obj.value("height").toInt(1080));
    a.fps = std::clamp(obj.value("fps").toDouble(30.0), 1.0, 240.0);
    a.hasAudio = obj.value("hasAudio").toBool(true);
    a.thumbnailPath = localMediaPath(obj.value("thumbnailPath").toString());
    a.filmstripPath = localMediaPath(obj.value("filmstripPath").toString());
    const auto arr = obj.value("waveformPeaks").toArray();
    for (const auto &v : arr)
        a.waveformPeaks.append(v.toDouble());
    return a;
}

Keyframe TimelineClip::evaluateAt(double localTimeSec) const {
    if (keyframes.isEmpty())
        return Keyframe{};

    QList<Keyframe> sorted = keyframes;
    std::sort(sorted.begin(), sorted.end(), [](const Keyframe &a, const Keyframe &b) {
        return a.timeSec < b.timeSec;
    });

    if (localTimeSec <= sorted.first().timeSec)
        return sorted.first();
    if (localTimeSec >= sorted.last().timeSec)
        return sorted.last();

    for (int i = 0; i + 1 < sorted.size(); ++i) {
        const auto &k0 = sorted[i];
        const auto &k1 = sorted[i + 1];
        if (localTimeSec >= k0.timeSec && localTimeSec <= k1.timeSec) {
            const double span = k1.timeSec - k0.timeSec;
            const double t = span > 1e-6 ? (localTimeSec - k0.timeSec) / span : 0.0;
            // Smoothstep easing for natural CapCut-style motion
            const double s = t * t * (3.0 - 2.0 * t);
            Keyframe out;
            out.timeSec = localTimeSec;
            out.x = k0.x + (k1.x - k0.x) * s;
            out.y = k0.y + (k1.y - k0.y) * s;
            out.scale = k0.scale + (k1.scale - k0.scale) * s;
            out.opacity = k0.opacity + (k1.opacity - k0.opacity) * s;
            out.rotation = k0.rotation + (k1.rotation - k0.rotation) * s;
            return out;
        }
    }
    return sorted.last();
}

QJsonObject TimelineClip::toJson() const {
    QJsonArray kfs;
    for (const auto &k : keyframes)
        kfs.append(k.toJson());
    return QJsonObject{
        {"id", id},
        {"assetId", assetId},
        {"sourcePath", sourcePath},
        {"name", name},
        {"kind", kind},
        {"track", track},
        {"timelineStartSec", timelineStartSec},
        {"sourceInSec", sourceInSec},
        {"durationSec", durationSec},
        {"sourceDurationSec", sourceDurationSec},
        {"speed", speed},
        {"volume", volume},
        {"muted", muted},
        {"brightness", brightness},
        {"contrast", contrast},
        {"saturation", saturation},
        {"blur", blur},
        {"vignette", vignette},
        {"transitionIn", transitionIn},
        {"transitionDurationSec", transitionDurationSec},
        {"audioDenoise", audioDenoise},
        {"audioVoiceClarity", audioVoiceClarity},
        {"audioNormalize", audioNormalize},
        {"audioCompress", audioCompress},
        {"bgRemovalMode", bgRemovalMode},
        {"chromaColor", chromaColor},
        {"chromaSimilarity", chromaSimilarity},
        {"chromaBlend", chromaBlend},
        {"processedPath", processedPath},
        {"keyframes", kfs},
    };
}

TimelineClip TimelineClip::fromJson(const QJsonObject &obj) {
    TimelineClip c;
    c.id = obj.value("id").toString();
    c.assetId = obj.value("assetId").toString();
    c.sourcePath = localMediaPath(obj.value("sourcePath").toString());
    c.name = obj.value("name").toString();
    c.kind = obj.value("kind").toString("video");
    c.track = std::clamp(obj.value("track").toInt(0), 0, 2);
    c.timelineStartSec = std::max(0.0, obj.value("timelineStartSec").toDouble(0.0));
    c.sourceInSec = std::max(0.0, obj.value("sourceInSec").toDouble(0.0));
    c.durationSec = std::max(0.1, obj.value("durationSec").toDouble(5.0));
    c.sourceDurationSec = std::max(0.1, obj.value("sourceDurationSec").toDouble(c.durationSec));
    c.speed = std::clamp(obj.value("speed").toDouble(1.0), 0.25, 4.0);
    c.volume = std::clamp(obj.value("volume").toDouble(1.0), 0.0, 2.0);
    c.muted = obj.value("muted").toBool(false);
    c.brightness = std::clamp(obj.value("brightness").toDouble(0.0), -0.5, 0.5);
    c.contrast = std::clamp(obj.value("contrast").toDouble(1.0), 0.5, 2.0);
    c.saturation = std::clamp(obj.value("saturation").toDouble(1.0), 0.0, 2.0);
    c.blur = std::clamp(obj.value("blur").toDouble(0.0), 0.0, 20.0);
    c.vignette = obj.value("vignette").toBool(false);
    c.transitionIn = obj.value("transitionIn").toString("none");
    c.transitionDurationSec = std::clamp(obj.value("transitionDurationSec").toDouble(0.5), 0.1, 3.0);
    c.audioDenoise = obj.value("audioDenoise").toBool(false);
    c.audioVoiceClarity = obj.value("audioVoiceClarity").toBool(false);
    c.audioNormalize = obj.value("audioNormalize").toBool(false);
    c.audioCompress = obj.value("audioCompress").toBool(false);
    c.bgRemovalMode = obj.value("bgRemovalMode").toString("none");
    c.chromaColor = sanitizedChromaColor(obj.value("chromaColor").toString("#00ff00"));
    c.chromaSimilarity = std::clamp(obj.value("chromaSimilarity").toDouble(0.18), 0.01, 0.6);
    c.chromaBlend = std::clamp(obj.value("chromaBlend").toDouble(0.08), 0.0, 0.5);
    c.processedPath = localMediaPath(obj.value("processedPath").toString());
    const auto arr = obj.value("keyframes").toArray();
    for (const auto &v : arr)
        c.keyframes.append(Keyframe::fromJson(v.toObject()));
    return c;
}

QJsonObject TextOverlay::toJson() const {
    return QJsonObject{
        {"id", id},
        {"text", text},
        {"isCaption", isCaption},
        {"startSec", startSec},
        {"durationSec", durationSec},
        {"x", x},
        {"y", y},
        {"fontSize", fontSize},
        {"color", color},
        {"bgColor", bgColor},
        {"bgOpacity", bgOpacity},
        {"bold", bold},
    };
}

TextOverlay TextOverlay::fromJson(const QJsonObject &obj) {
    TextOverlay t;
    t.id = obj.value("id").toString();
    t.text = obj.value("text").toString("Title");
    t.isCaption = obj.value("isCaption").toBool(false);
    t.startSec = std::max(0.0, obj.value("startSec").toDouble(0.0));
    t.durationSec = std::max(0.1, obj.value("durationSec").toDouble(3.0));
    t.x = obj.value("x").toDouble(0.5);
    t.y = obj.value("y").toDouble(0.82);
    t.fontSize = obj.value("fontSize").toInt(44);
    t.color = obj.value("color").toString("#eceff4");
    t.bgColor = obj.value("bgColor").toString("#090a0c");
    t.bgOpacity = std::clamp(obj.value("bgOpacity").toDouble(0.65), 0.0, 1.0);
    t.bold = obj.value("bold").toBool(true);
    return t;
}

// Solo wins: if any lane is soloed, only soloed lanes are audible.
bool Project::laneAudible(int track) const {
    if (track < 0 || track > 2)
        return true;
    bool anySolo = false;
    for (const auto &lane : lanes)
        anySolo = anySolo || lane.solo;
    const LaneState &lane = lanes[track];
    if (lane.solo)
        return !lane.muted;
    return !anySolo && !lane.muted;
}

bool Project::laneLocked(int track) const {
    return track >= 0 && track <= 2 && lanes[track].locked;
}

double Project::totalDurationSec() const {
    double maxEnd = 0.0;
    for (const auto &c : clips)
        maxEnd = std::max(maxEnd, c.timelineEndSec());
    for (const auto &t : texts)
        maxEnd = std::max(maxEnd, t.endSec());
    return maxEnd;
}

QJsonObject LaneState::toJson() const {
    return {{"muted", muted}, {"solo", solo}, {"locked", locked}};
}

LaneState LaneState::fromJson(const QJsonObject &obj) {
    LaneState l;
    l.muted = obj.value("muted").toBool();
    l.solo = obj.value("solo").toBool();
    l.locked = obj.value("locked").toBool();
    return l;
}

QJsonObject Project::toJson() const {
    QJsonArray assetArr, clipArr, textArr;
    for (const auto &a : assets)
        assetArr.append(a.toJson());
    for (const auto &c : clips)
        clipArr.append(c.toJson());
    for (const auto &t : texts)
        textArr.append(t.toJson());
    return QJsonObject{
        {"version", 1},
        {"title", title},
        {"width", width},
        {"height", height},
        {"fps", fps},
        {"lanes", QJsonArray{LaneState(lanes[0]).toJson(), LaneState(lanes[1]).toJson(), LaneState(lanes[2]).toJson()}},
        {"assets", assetArr},
        {"clips", clipArr},
        {"texts", textArr},
    };
}

Project Project::fromJson(const QJsonObject &obj) {
    Project p;
    p.title = obj.value("title").toString("Untitled Project");
    p.width = evenDimension(obj.value("width").toInt(1920));
    p.height = evenDimension(obj.value("height").toInt(1080));
    p.fps = std::clamp(obj.value("fps").toDouble(30.0), 1.0, 240.0);
    const auto laneArr = obj.value("lanes").toArray();
    for (int i = 0; i < 3 && i < laneArr.size(); ++i)
        p.lanes[i] = LaneState::fromJson(laneArr[i].toObject());
    for (const auto &v : obj.value("assets").toArray())
        p.assets.append(MediaAsset::fromJson(v.toObject()));
    for (const auto &v : obj.value("clips").toArray())
        p.clips.append(TimelineClip::fromJson(v.toObject()));
    for (const auto &v : obj.value("texts").toArray())
        p.texts.append(TextOverlay::fromJson(v.toObject()));
    return p;
}

bool Project::saveToFile(const QString &path, QString *error) const {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

Project Project::loadFromFile(const QString &path, bool *ok, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok)
            *ok = false;
        if (error)
            *error = file.errorString();
        return {};
    }
    QJsonParseError parseErr;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (ok)
            *ok = false;
        if (error)
            *error = parseErr.errorString();
        return {};
    }
    if (ok)
        *ok = true;
    return fromJson(doc.object());
}

int Project::findClipIndex(const QString &id) const {
    for (int i = 0; i < clips.size(); ++i) {
        if (clips[i].id == id)
            return i;
    }
    return -1;
}

int Project::findTextIndex(const QString &id) const {
    for (int i = 0; i < texts.size(); ++i) {
        if (texts[i].id == id)
            return i;
    }
    return -1;
}

int Project::findAssetIndex(const QString &id) const {
    for (int i = 0; i < assets.size(); ++i) {
        if (assets[i].id == id)
            return i;
    }
    return -1;
}

double Project::nextTrackEndSec(int track) const {
    double end = 0.0;
    for (const auto &c : clips) {
        if (c.track == track)
            end = std::max(end, c.timelineEndSec());
    }
    return end;
}
