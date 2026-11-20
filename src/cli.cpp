#include "cli.h"
#include "mediaengine.h"
#include "project.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <cstdio>

namespace {
void printJson(const QJsonObject &obj) {
    std::fprintf(stdout, "%s\n", QJsonDocument(obj).toJson(QJsonDocument::Compact).constData());
}

int failCli(const QString &msg, bool json) {
    if (json) {
        printJson({{"ok", false}, {"error", msg}});
    } else {
        std::fprintf(stderr, "Error: %s\n", qUtf8Printable(msg));
    }
    return 1;
}

QString argValue(const QStringList &args, const QString &flag, const QString &fallback = {}) {
    const int idx = args.indexOf(flag);
    if (idx >= 0 && idx + 1 < args.size())
        return args[idx + 1];
    return fallback;
}
} // namespace

bool Cli::shouldRunHeadless(const QStringList &args) {
    if (args.size() < 2)
        return false;
    const QString cmd = args[1];
    if (cmd == "open")
        return false;
    static const QStringList headlessCmds{
        "capabilities", "probe", "new", "add-clip", "split", "add-text", "captions", "export", "--help", "-h", "help"
    };
    return headlessCmds.contains(cmd);
}

QStringList Cli::guiFiles(const QStringList &args) {
    QStringList files;
    if (args.size() < 2)
        return files;
    if (args[1] == "open")
        files = args.mid(2);
    else if (!args[1].startsWith('-'))
        files = args.mid(1);
    for (int i = files.size() - 1; i >= 0; --i) {
        if (files[i].startsWith('-'))
            files.removeAt(i);
    }
    return files;
}

int Cli::run(const QStringList &args) {
    const QString cmd = args.value(1);
    const bool json = args.contains("--json");

    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        std::fprintf(stdout,
            "CutCap — Local-first video editor CLI\n\n"
            "Usage:\n"
            "  cutcap [open <file>]                                Launch GUI\n"
            "  cutcap capabilities [--json]                        Report local tools (ffmpeg, whisper, tts, rembg)\n"
            "  cutcap probe <media> [--json]                       Probe media metadata\n"
            "  cutcap new <project.cutcap> [--title T] [--json]    Create empty project\n"
            "  cutcap add-clip <project.cutcap> <media> [--track 0] [--start -1] [--json]\n"
            "  cutcap split <project.cutcap> <clip-id> <at-sec> [--json]\n"
            "  cutcap add-text <project.cutcap> --text \"...\" [--start 0] [--duration 3] [--caption] [--json]\n"
            "  cutcap captions <project.cutcap> <media> [--lang en] [--json]\n"
            "  cutcap export <project.cutcap> <out.mp4> [--preset high|medium|fast] [--codec h264|h265|av1] [--json]\n");
        return 0;
    }

    if (cmd == "capabilities") {
        const auto caps = MediaEngine::detectCapabilities();
        if (json)
            printJson({{"ok", true}, {"capabilities", caps.toJson()}});
        else
            std::fprintf(stdout, "ffmpeg=%d ffprobe=%d pwRecord=%d whisper=%s tts=%s rembg=%s\n",
                         caps.ffmpeg, caps.ffprobe, caps.pwRecord,
                         qUtf8Printable(caps.whisperCmd), qUtf8Printable(caps.ttsCmd), qUtf8Printable(caps.rembgCmd));
        return 0;
    }

    if (cmd == "probe") {
        if (args.size() < 3)
            return failCli("Usage: cutcap probe <media>", json);
        QString err;
        const MediaAsset asset = MediaEngine::probeMedia(args[2], &err);
        if (!err.isEmpty())
            return failCli(err, json);
        if (json)
            printJson({{"ok", true}, {"asset", asset.toJson()}});
        else
            std::fprintf(stdout, "%s (%s) %dx%d %.2fs %.2ffps\n",
                         qUtf8Printable(asset.name), qUtf8Printable(asset.kind),
                         asset.width, asset.height, asset.durationSec, asset.fps);
        return 0;
    }

    if (cmd == "new") {
        if (args.size() < 3)
            return failCli("Usage: cutcap new <project.cutcap>", json);
        Project p;
        p.title = argValue(args, "--title", QFileInfo(args[2]).completeBaseName());
        p.width = argValue(args, "--width", "1920").toInt();
        p.height = argValue(args, "--height", "1080").toInt();
        QString err;
        if (!p.saveToFile(args[2], &err))
            return failCli(err, json);
        if (json)
            printJson({{"ok", true}, {"path", args[2]}, {"project", p.toJson()}});
        return 0;
    }

    if (cmd == "add-clip") {
        if (args.size() < 4)
            return failCli("Usage: cutcap add-clip <project.cutcap> <media>", json);
        bool ok = false;
        QString err;
        Project p = Project::loadFromFile(args[2], &ok, &err);
        if (!ok)
            return failCli(err, json);
        MediaAsset asset = MediaEngine::probeMedia(args[3], &err);
        if (!err.isEmpty())
            return failCli(err, json);
        MediaEngine::preparePreviews(asset);
        p.assets.append(asset);

        TimelineClip c;
        c.id = QString("clip_%1").arg(QDateTime::currentMSecsSinceEpoch() % 1000000);
        c.assetId = asset.id;
        c.sourcePath = asset.path;
        c.name = asset.name;
        c.kind = asset.kind;
        c.track = argValue(args, "--track", asset.kind == "audio" ? "2" : "0").toInt();
        if (c.kind == "image" && c.track == 2)
            return failCli("An image cannot go on the audio track (2); use --track 0 or 1.", json);
        const double startArg = argValue(args, "--start", "-1").toDouble();
        c.timelineStartSec = startArg >= 0.0 ? startArg : p.nextTrackEndSec(c.track);
        c.durationSec = asset.durationSec;
        c.sourceDurationSec = asset.durationSec;
        p.clips.append(c);

        if (!p.saveToFile(args[2], &err))
            return failCli(err, json);
        if (json)
            printJson({{"ok", true}, {"clipId", c.id}, {"clip", c.toJson()}});
        return 0;
    }

    if (cmd == "split") {
        if (args.size() < 5)
            return failCli("Usage: cutcap split <project.cutcap> <clip-id> <at-sec>", json);
        bool ok = false;
        QString err;
        Project p = Project::loadFromFile(args[2], &ok, &err);
        if (!ok)
            return failCli(err, json);
        const int idx = p.findClipIndex(args[3]);
        if (idx < 0)
            return failCli("Clip not found: " + args[3], json);
        const double atSec = args[4].toDouble();
        TimelineClip left = p.clips[idx];
        if (atSec <= left.timelineStartSec + 0.05 || atSec >= left.timelineEndSec() - 0.05)
            return failCli("Split time must be inside clip bounds", json);
        TimelineClip right = left;
        const double offset = atSec - left.timelineStartSec;
        left.durationSec = offset;
        right.id = QString("clip_%1").arg(QDateTime::currentMSecsSinceEpoch() % 1000000);
        right.timelineStartSec = atSec;
        right.sourceInSec = left.sourceInSec + offset * left.speed;
        right.durationSec = std::max(0.1, right.durationSec - offset);
        p.clips[idx] = left;
        p.clips.insert(idx + 1, right);
        if (!p.saveToFile(args[2], &err))
            return failCli(err, json);
        if (json)
            printJson({{"ok", true}, {"leftId", left.id}, {"rightId", right.id}});
        return 0;
    }

    if (cmd == "add-text") {
        if (args.size() < 3)
            return failCli("Usage: cutcap add-text <project.cutcap> --text \"...\"", json);
        bool ok = false;
        QString err;
        Project p = Project::loadFromFile(args[2], &ok, &err);
        if (!ok)
            return failCli(err, json);
        TextOverlay t;
        t.id = QString("txt_%1").arg(QDateTime::currentMSecsSinceEpoch() % 1000000);
        t.text = argValue(args, "--text", "Title");
        t.startSec = argValue(args, "--start", "0").toDouble();
        t.durationSec = argValue(args, "--duration", "3").toDouble();
        t.isCaption = args.contains("--caption");
        p.texts.append(t);
        if (!p.saveToFile(args[2], &err))
            return failCli(err, json);
        if (json)
            printJson({{"ok", true}, {"textId", t.id}});
        return 0;
    }

    if (cmd == "captions") {
        if (args.size() < 4)
            return failCli("Usage: cutcap captions <project.cutcap> <media>", json);
        bool ok = false;
        QString err;
        Project p = Project::loadFromFile(args[2], &ok, &err);
        if (!ok)
            return failCli(err, json);
        const auto caps = MediaEngine::runAutoCaptions(args[3], argValue(args, "--lang", "en"), &err);
        if (!err.isEmpty())
            return failCli(err, json);
        p.texts.append(caps);
        if (!p.saveToFile(args[2], &err))
            return failCli(err, json);
        if (json)
            printJson({{"ok", true}, {"added", caps.size()}});
        return 0;
    }

    if (cmd == "export") {
        if (args.size() < 4)
            return failCli("Usage: cutcap export <project.cutcap> <output.mp4>", json);
        bool ok = false;
        QString err;
        Project p = Project::loadFromFile(args[2], &ok, &err);
        if (!ok)
            return failCli(err, json);
        ExportOptions opts;
        opts.preset = argValue(args, "--preset", "high");
        opts.codec = argValue(args, "--codec", "h264");

        QTemporaryDir tmp;
        QString assPath;
        if (!p.texts.isEmpty() && tmp.isValid())
            assPath = MediaEngine::generateAssSubtitles(p, tmp.path() + "/subs.ass", p.width, p.height);

        const QStringList ffArgs = MediaEngine::buildExportArgs(p, args[3], opts, assPath);
        QProcess proc;
        proc.start("ffmpeg", ffArgs);
        if (!proc.waitForFinished(-1) || proc.exitCode() != 0)
            return failCli(QString::fromUtf8(proc.readAllStandardError()).right(500), json);

        if (json)
            printJson({{"ok", true}, {"output", args[3]}});
        else
            std::fprintf(stdout, "Exported %s\n", qUtf8Printable(args[3]));
        return 0;
    }

    return failCli("Unknown command: " + cmd + " (try --help)", json);
}
