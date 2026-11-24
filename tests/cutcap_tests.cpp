#include <QtTest/QtTest>
#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>
#include <QFile>
#include <QFontInfo>
#include <QRegularExpression>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtQml/qqml.h>
#include <memory>
#include <QMimeData>
#include <QUrl>

#include "backend.h"
#include "clipframeitem.h"
#include "cli.h"
#include "design.h"
#include "mediaengine.h"
#include "project.h"
#include "theme.h"

static QString makeSampleVideo(const QString &dir, const QString &name, const QString &color, double seconds) {
    const QString path = dir + "/" + name;
    QProcess ff;
    ff.start("ffmpeg", {"-y", "-v", "error", "-f", "lavfi", "-i",
                        QString("color=c=%1:s=320x240:r=30:d=%2").arg(color).arg(seconds),
                        "-f", "lavfi", "-i", QString("sine=frequency=440:sample_rate=44100:duration=%1").arg(seconds),
                        "-shortest", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", path});
    if (!ff.waitForFinished(20000) || ff.exitCode() != 0)
        return {};
    return path;
}

static double probeDurationSec(const QString &path) {
    QProcess p;
    p.start("ffprobe", {"-v", "error", "-show_entries", "format=duration", "-of", "default=nw=1:nk=1", path});
    p.waitForFinished(10000);
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed().toDouble();
}

static QQuickItem *findItem(QQuickItem *root, const QString &objectName) {
    if (!root)
        return nullptr;
    if (root->objectName() == objectName)
        return root;
    for (QQuickItem *child : root->childItems()) {
        if (auto *found = findItem(child, objectName))
            return found;
    }
    return nullptr;
}

static QQuickItem *findClipBody(QQuickItem *root, const QString &clipId) {
    if (!root)
        return nullptr;
    for (QQuickItem *child : root->childItems()) {
        if (child->objectName() == "clipBody" && child->property("clipId").toString() == clipId)
            return child;
        if (auto *found = findClipBody(child, clipId))
            return found;
    }
    return nullptr;
}

static void collectUnreadableButtons(QQuickItem *item, QStringList *problems, int *inspected = nullptr) {
    if (!item || !item->isVisible())
        return;
    if (item->width() <= 0 || item->height() <= 0) {
        // A button the layout squeezed to nothing is the worst kind of unreadable.
        if (item->property("labelItem").value<QQuickItem *>() && !item->property("text").toString().isEmpty())
            problems->append(QString("ActionButton '%1' has no area (%2x%3)")
                                 .arg(item->property("text").toString()).arg(item->width()).arg(item->height()));
        return;
    }
    const QString text = item->property("text").toString();
    if (!text.isEmpty()) {
        if (inspected)
            ++*inspected;
        if (item->inherits("QQuickAbstractButton")) {
            auto *content = item->property("contentItem").value<QQuickItem *>();
            if (!content || content->height() + 0.5 < content->implicitHeight() || content->height() <= 0)
                problems->append(QString("Button '%1' content %2px < %3px")
                                     .arg(text)
                                     .arg(content ? content->height() : 0)
                                     .arg(content ? content->implicitHeight() : 0));
        } else if (auto *label = item->property("labelItem").value<QQuickItem *>()) {
            if (item->height() + 0.5 < label->implicitHeight() || item->width() + 0.5 < label->implicitWidth())
                problems->append(QString("ActionButton '%1' %2x%3 < label %4x%5")
                                     .arg(text)
                                     .arg(item->width())
                                     .arg(item->height())
                                     .arg(label->implicitWidth())
                                     .arg(label->implicitHeight()));
        }
    }
    for (QQuickItem *child : item->childItems())
        collectUnreadableButtons(child, problems, inspected);
}

static QStringList *g_qmlErrors = nullptr;
static QtMessageHandler g_previousHandler = nullptr;

static void captureQmlErrors(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    if (g_qmlErrors && message.contains(".qml:"))
        g_qmlErrors->append(message);
    if (g_previousHandler)
        g_previousHandler(type, context, message);
}

static double contrastRatio(const QString &fg, const QString &bg) {
    auto lum = [](const QColor &c) {
        auto lin = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
    };
    const double a = lum(QColor(fg));
    const double b = lum(QColor(bg));
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

// Worst per-channel delta and mean delta (0-255) between two same-size images, comparing the raw
// stored channels. The CPU reference is premultiplied and the grab window composites the item over
// a black clear color, so its RGB channels hold the same premultiplied values. The alpha channel is
// skipped on purpose: the offscreen grab window has no alpha buffer, so its alpha is always 255.
static void imageDelta(const QImage &actual, const QImage &expected, int *maxDelta, double *meanDelta) {
    Q_ASSERT(actual.size() == expected.size() && !actual.isNull());
    *maxDelta = 0;
    *meanDelta = 0.0;
    for (int y = 0; y < actual.height(); ++y) {
        for (int x = 0; x < actual.width(); ++x) {
            const QRgb a = actual.pixel(x, y);
            const QRgb e = expected.pixel(x, y);
            const int dr = std::abs(qRed(a) - qRed(e));
            const int dg = std::abs(qGreen(a) - qGreen(e));
            const int db = std::abs(qBlue(a) - qBlue(e));
            *maxDelta = std::max({*maxDelta, dr, dg, db});
            *meanDelta += double(dr + dg + db);
        }
    }
    *meanDelta /= double(actual.width()) * actual.height() * 3;
}

// Sample media shared by the UI tests: each clip is encoded once per process and reused, so
// splitting the UI smoke coverage into several tests does not re-pay the ffmpeg cost (the media
// cache under XDG_CACHE_HOME then also makes the repeat imports cheap).
static QString uiSampleVideo(const QString &name, const QString &color, double seconds) {
    static QTemporaryDir sharedDir;
    static QHash<QString, QString> cache;
    const auto it = cache.constFind(name);
    if (it != cache.constEnd())
        return it.value();
    const QString path = makeSampleVideo(sharedDir.path(), name, color, seconds);
    cache.insert(name, path);
    return path;
}

static QString uiSampleTone() {
    static QTemporaryDir sharedDir;
    static QString path;
    if (path.isEmpty()) {
        const QString wav = sharedDir.path() + "/tone.wav";
        QProcess tone;
        tone.start("ffmpeg", {"-y", "-v", "error", "-f", "lavfi", "-i", "sine=frequency=300:duration=2", wav});
        if (tone.waitForFinished(10000) && tone.exitCode() == 0)
            path = wav;
    }
    return path;
}

// One UI test's private app instance: a fresh QML engine wired to fresh Theme/Design/Backend
// singletons, so every UI test fails on its own without state leaking in from another test.
// Members are ordered so destruction runs engine-first, before the singletons it references.
struct UiHarness {
    std::unique_ptr<Theme> theme;
    std::unique_ptr<Design> design;
    std::unique_ptr<Backend> backend;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow *window = nullptr;
    QStringList qmlErrors;

    // Registers the singletons but does not build the window yet, so a test can set up backend
    // state (imports, clip moves) first, exactly like the app does before Main.qml reads it.
    static UiHarness create(const QString &workDir) {
        UiHarness h;
        QQuickStyle::setStyle("Material");
        h.theme = std::make_unique<Theme>(workDir);
        h.design = std::make_unique<Design>(*h.theme);
        h.backend = std::make_unique<Backend>();
        // Register the module's types before injecting instances: a singleton instance seals the
        // module version, and the module's own registration would then be dropped.
        extern void qml_register_types_CutCap();
        qml_register_types_CutCap();
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Theme", h.theme.get());
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Design", h.design.get());
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Backend", h.backend.get());
        return h;
    }

    void loadEngine() {
        g_qmlErrors = &qmlErrors;
        g_previousHandler = qInstallMessageHandler(captureQmlErrors);
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->load(QUrl("qrc:/qml/Main.qml"));
        qInstallMessageHandler(g_previousHandler);
        g_qmlErrors = nullptr;
        if (!engine->rootObjects().isEmpty())
            window = qobject_cast<QQuickWindow *>(engine->rootObjects().first());
    }

    static UiHarness loadMain(const QString &workDir) {
        UiHarness h = create(workDir);
        h.loadEngine();
        return h;
    }
};

// Writes an executable shell shim and returns its path.
static QString makeTool(const QString &dir, const QString &name, const QByteArray &body) {
    QDir().mkpath(dir);
    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return {};
    f.write(body);
    f.close();
    if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner))
        return {};
    return path;
}

// Restores PATH when the test scope ends, even on an early QVERIFY return.
struct PathRestore {
    QByteArray value;
    ~PathRestore() { qputenv("PATH", value); }
};

class CutCapTests : public QObject {
    Q_OBJECT

private slots:
    // Thumbnails, filmstrips and recordings land in per-run directories, never in the developer's
    // own ~/.cache or ~/.local/share.
    void initTestCase() {
        QVERIFY(m_xdgRoot.isValid());
        QVERIFY(QDir().mkpath(m_xdgRoot.path() + "/cache"));
        QVERIFY(QDir().mkpath(m_xdgRoot.path() + "/data"));
        qputenv("XDG_CACHE_HOME", (m_xdgRoot.path() + "/cache").toUtf8());
        qputenv("XDG_DATA_HOME", (m_xdgRoot.path() + "/data").toUtf8());
        QVERIFY(MediaEngine::cacheDir().startsWith(m_xdgRoot.path()));
    }

    void themeReadsOmarchyColorsToml() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QDir().mkpath(dir.path() + "/theme");
        QFile f(dir.path() + "/theme/colors.toml");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("mode = \"dark\"\naccent = \"#7393a7\"\nbackground = \"#14161a\"\n");
        f.close();

        Theme t(dir.path());
        QCOMPARE(t.accent(), QString("#7393a7"));
        QCOMPARE(t.background(), QString("#14161a"));
        QCOMPARE(t.darkMode(), true);
    }

    // Muted text must clear WCAG AA on every panel surface, for light and dark Omarchy themes.
    // The design system only exists if QML cannot bypass it: type sizes and radii must come from
    // Design tokens, and the tokens themselves must form sane, ascending scales.
    void designTokensAreUsedAndConsistent() {
        const QStringList files{"ActionButton", "Main", "MediaBinPanel", "PreviewPanel", "PreviewLane", "TimelinePanel",
                                "InspectorPanel", "ExportDialog", "ShortcutsDialog"};
        const QRegularExpression literalType(R"(font\.pixelSize:\s*[0-9])");
        const QRegularExpression literalBtnType(R"(fontPixelSize:\s*[0-9])");
        const QRegularExpression literalRadius(R"(\bradius:\s*[1-9])");
        const QRegularExpression rawRgba(R"(\bQt\.rgba\()");
        const QRegularExpression rawButton(R"(\bButton\s*\{)");
        QStringList offenders;
        for (const QString &name : files) {
            QFile f(":/qml/" + name + ".qml");
            QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(name));
            const QString text = QString::fromUtf8(f.readAll());
            const auto lines = text.split('\n');
            for (int i = 0; i < lines.size(); ++i) {
                if (literalType.match(lines[i]).hasMatch() || literalBtnType.match(lines[i]).hasMatch() ||
                    literalRadius.match(lines[i]).hasMatch() || rawRgba.match(lines[i]).hasMatch() ||
                    rawButton.match(lines[i]).hasMatch()) {
                    offenders << QString("%1.qml:%2 %3").arg(name).arg(i + 1).arg(lines[i].trimmed());
                }
            }
        }
        QVERIFY2(offenders.isEmpty(), qPrintable(offenders.mid(0, 8).join("\n")));

        QTemporaryDir dir;
        Theme theme(dir.path());
        Design d(theme);
        QVERIFY(d.s1() < d.s2() && d.s2() < d.s3() && d.s3() < d.s4() && d.s4() < d.s5() && d.s5() < d.s6());
        // Type floor: nothing under 12 px (labels are read, not just glanced at), and no tier inverts.
        QVERIFY(d.tMicro() >= 12 && d.tCaption() >= 12);
        QVERIFY(d.tMicro() <= d.tCaption() && d.tCaption() <= d.tBody() && d.tBody() <= d.tBodyLg() &&
                d.tBodyLg() < d.tTitle());
        QVERIFY(d.hCompact() < d.hSmall() && d.hSmall() < d.hStd() && d.hStd() < d.hLarge());
        QVERIFY2(d.hCompact() >= 24, "pointer targets must stay at least 24 px high");
        QVERIFY(!d.fontUi().isEmpty() && !d.fontMono().isEmpty());
        QVERIFY(d.rXs() < d.rSm() && d.rSm() < d.rMd() && d.rMd() < d.rLg());
        QVERIFY(d.hCompact() >= d.tBodyLg() + 8);   // a 24px control can hold 13px text with padding
        QVERIFY(contrastRatio(d.textPrimary(), d.surfacePanel()) >= 7.0);
        QVERIFY(contrastRatio(d.textSecondary(), d.surfaceRaised()) >= 4.5);
        QVERIFY(d.alpha("#112233", 0.5).endsWith("80"));
    }

    // A family without the weights the UI asks for makes Qt fall back to a slanted face (Nimbus
    // Sans has no 500/600 and resolved to Italic and Bold Italic).
    void uiFontWeightsAreUpright() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        Design d(theme);
        for (int weight : {d.wRegular(), d.wMedium(), d.wSemibold()}) {
            QFont font(d.fontUi());
            font.setWeight(QFont::Weight(weight));
            const QFontInfo info(font);
            QVERIFY2(info.family().startsWith(d.fontUi()),
                     qPrintable(QString("UI font %1 is not installed; got %2").arg(d.fontUi(), info.family())));
            const QString style = info.styleName();
            QVERIFY2(!style.contains("Italic", Qt::CaseInsensitive) && !style.contains("Oblique", Qt::CaseInsensitive),
                     qPrintable(QString("weight %1 of %2 resolves to the slanted face '%3'")
                                    .arg(weight).arg(d.fontUi(), style)));
        }
    }

    void secondaryTextMeetsContrastOnAllSurfaces() {
        const QList<QList<QString>> themes = {
            {"dark", "#090a0c", "#0e1013", "#14161a", "#1d2026", "#282d36", "#6f7785", "#d8dde6", "#eceff4"},
            {"light", "#ffffff", "#f7f7f8", "#eeeef0", "#e4e4e8", "#d8d8dd", "#8a8a92", "#2a2a30", "#14141a"},
        };
        for (const auto &palette : themes) {
            QTemporaryDir dir;
            QDir().mkpath(dir.path() + "/theme");
            QFile f(dir.path() + "/theme/colors.toml");
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QString("mode = \"%1\"\ndarker_background = \"%2\"\ndark_background = \"%3\"\nbackground = \"%4\"\n"
                            "lighter_background = \"%5\"\nselection = \"%6\"\ndark_foreground = \"%7\"\nforeground = \"%8\"\n"
                            "bright_foreground = \"%9\"\n")
                        .arg(palette[0], palette[1], palette[2], palette[3], palette[4], palette[5], palette[6], palette[7],
                             palette[8])
                        .toUtf8());
            f.close();

            Theme t(dir.path());
            for (const QString &surface : {palette[2], palette[3], palette[4], palette[5]}) {
                const double ratio = contrastRatio(t.secondaryForeground(), surface);
                QVERIFY2(ratio >= 4.5, qPrintable(QString("%1 theme: %2 on %3 = %4:1")
                                                      .arg(palette[0], t.secondaryForeground(), surface)
                                                      .arg(ratio, 0, 'f', 2)));
            }
            QVERIFY(contrastRatio(t.brightForeground(), palette[3]) >= 7.0);
            QVERIFY(contrastRatio(t.accentForeground(), t.accent()) >= 4.5);
        }
    }

    // Assets written by the CLI or by an older project must still get thumbnails, strips and waveforms.
    void preparePreviewsFillsMissingPreviews() {
        QTemporaryDir dir;
        const QString png = dir.path() + "/still.png";
        const QString vid = makeSampleVideo(dir.path(), "clip.mp4", "purple", 2.0);
        QVERIFY(!vid.isEmpty());
        QProcess still;
        still.start("ffmpeg", {"-y", "-v", "error", "-f", "lavfi", "-i", "smptehdbars=s=640x360", "-frames:v", "1", png});
        QVERIFY(still.waitForFinished(10000) && still.exitCode() == 0);

        for (const QString &path : {png, vid}) {
            QString err;
            MediaAsset asset = MediaEngine::probeMedia(path, &err);
            QVERIFY2(err.isEmpty(), qPrintable(err));
            asset.thumbnailPath.clear();
            asset.filmstripPath.clear();
            asset.waveformPeaks.clear();
            MediaEngine::preparePreviews(asset);
            QVERIFY2(!asset.thumbnailPath.isEmpty() && QFileInfo::exists(asset.thumbnailPath), qPrintable(path + " thumbnail"));
            QVERIFY2(!asset.filmstripPath.isEmpty() && QFileInfo::exists(asset.filmstripPath), qPrintable(path + " filmstrip"));
            if (asset.kind == "image")
                QVERIFY(asset.waveformPeaks.isEmpty());
            else
                QCOMPARE(asset.waveformPeaks.size(), 80);
        }
    }

    void keyframeInterpolationSmoothstep() {
        TimelineClip c;
        c.durationSec = 4.0;
        c.keyframes.append({0.0, 0.2, 0.5, 1.0, 1.0, 0.0});
        c.keyframes.append({2.0, 0.8, 0.5, 2.0, 0.5, 90.0});

        const Keyframe mid = c.evaluateAt(1.0);
        QVERIFY(std::abs(mid.x - 0.5) < 1e-4);
        QVERIFY(std::abs(mid.scale - 1.5) < 1e-4);
        QVERIFY(std::abs(mid.opacity - 0.75) < 1e-4);
        QVERIFY(std::abs(mid.rotation - 45.0) < 1e-4);
    }

    void projectRoundTripAndSrtParsing() {
        Project p;
        p.title = "Test Edit";
        TimelineClip c;
        c.id = "clip_1";
        c.name = "intro.mp4";
        c.sourcePath = "/tmp/intro.mp4";
        c.durationSec = 6.5;
        c.audioDenoise = true;
        c.bgRemovalMode = "chromakey";
        p.clips.append(c);

        const QString srt = "1\n00:00:01,000 --> 00:00:03,500\nHello Omarchy\n\n";
        p.texts = MediaEngine::parseSrt(srt, true);
        QCOMPARE(p.texts.size(), 1);
        QCOMPARE(p.texts[0].text, QString("Hello Omarchy"));
        QVERIFY(std::abs(p.texts[0].startSec - 1.0) < 1e-3);
        QVERIFY(std::abs(p.texts[0].durationSec - 2.5) < 1e-3);

        QTemporaryDir dir;
        const QString projPath = dir.path() + "/test.cutcap";
        QVERIFY(p.saveToFile(projPath));
        bool ok = false;
        const Project loaded = Project::loadFromFile(projPath, &ok);
        QVERIFY(ok);
        QCOMPARE(loaded.title, QString("Test Edit"));
        QCOMPARE(loaded.clips.size(), 1);
        QCOMPARE(loaded.clips[0].audioDenoise, true);
        QCOMPARE(loaded.texts.size(), 1);
    }

    // Every declared field is set away from its default and inside its clamp range, so a field that
    // is written but not read back (or the reverse) changes the JSON and fails the comparison.
    void projectFileKeepsEveryField() {
        Project p;
        p.title = "Every Field";
        p.width = 1280;
        p.height = 720;
        p.fps = 25.0;
        p.lanes[1].muted = true;
        p.lanes[2].locked = true;

        MediaAsset a;
        a.id = "asset_1";
        a.path = "/tmp/every.png";
        a.name = "every.png";
        a.kind = "image";
        a.durationSec = 7.5;
        a.width = 1280;
        a.height = 720;
        a.fps = 25.0;
        a.hasAudio = false;
        a.thumbnailPath = "/tmp/every-thumb.jpg";
        a.filmstripPath = "/tmp/every-strip.jpg";
        a.waveformPeaks = {0.1, 0.5};
        p.assets.append(a);

        TimelineClip c;
        c.id = "clip_1";
        c.assetId = "asset_1";
        c.sourcePath = "/tmp/every.png";
        c.name = "every clip";
        c.kind = "audio";
        c.track = 1;
        c.timelineStartSec = 2.0;
        c.sourceInSec = 0.5;
        c.durationSec = 4.0;
        c.sourceDurationSec = 9.0;
        c.speed = 2.0;
        c.volume = 1.5;
        c.muted = true;
        c.brightness = 0.2;
        c.contrast = 1.4;
        c.saturation = 0.6;
        c.blur = 3.0;
        c.vignette = true;
        c.transitionIn = "fade";
        c.transitionDurationSec = 1.0;
        c.audioDenoise = true;
        c.audioVoiceClarity = true;
        c.audioNormalize = true;
        c.audioCompress = true;
        c.bgRemovalMode = "chromakey";
        c.chromaColor = "#112233";
        c.chromaSimilarity = 0.3;
        c.chromaBlend = 0.2;
        c.processedPath = "/tmp/every-processed.mp4";
        Keyframe k;
        k.timeSec = 0.5;
        k.x = 0.3;
        k.y = 0.7;
        k.scale = 1.5;
        k.opacity = 0.8;
        k.rotation = 45.0;
        c.keyframes.append(k);
        p.clips.append(c);

        TextOverlay t;
        t.id = "text_1";
        t.text = "Every field";
        t.isCaption = true;
        t.startSec = 1.5;
        t.durationSec = 2.5;
        t.x = 0.2;
        t.y = 0.1;
        t.fontSize = 60;
        t.color = "#ff8800";
        t.bgColor = "#102030";
        t.bgOpacity = 0.4;
        t.bold = false;
        p.texts.append(t);

        // If a count changes a field was added or removed: set it above, then update the number.
        QCOMPARE(a.toJson().keys().size(), 12);
        QCOMPARE(c.toJson().keys().size(), 30);
        QCOMPARE(t.toJson().keys().size(), 12);

        QTemporaryDir dir;
        const QString path = dir.path() + "/full.cutcap";
        QVERIFY(p.saveToFile(path));
        bool ok = false;
        const Project loaded = Project::loadFromFile(path, &ok);
        QVERIFY(ok);
        QCOMPARE(loaded.toJson(), p.toJson());
    }

    // Save through the app, reopen in a fresh Backend, and check the edit came back intact.
    void backendSaveAndOpenRoundTrip() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "round.mp4", "0x225577", 3.0);
        QVERIFY(!vid.isEmpty());
        const QString path = dir.path() + "/round.cutcap";
        QStringList clipIds;
        {
            Backend b;
            b.importSingleMedia(vid);
            waitForImports(b);
            b.setPlayheadSec(1.0);
            b.splitAtPlayhead();
            b.addTextAtPlayhead("Round trip");
            b.setProjectTitle("Saved Edit");
            QCOMPARE(b.clipsList().size(), 2);
            QCOMPARE(b.textsList().size(), 1);
            QVERIFY(b.dirty());
            QVERIFY(b.saveProject(path));
            QVERIFY(!b.dirty());
            for (const QVariant &c : b.clipsList())
                clipIds << c.toMap().value("id").toString();
        }
        Backend b;
        QVERIFY(b.openProject(path));
        QVERIFY(!b.dirty());
        QCOMPARE(b.projectTitle(), QString("Saved Edit"));
        QCOMPARE(b.clipsList().size(), 2);
        QStringList reopened;
        for (const QVariant &c : b.clipsList())
            reopened << c.toMap().value("id").toString();
        QCOMPARE(reopened, clipIds);
        QCOMPARE(b.textsList().size(), 1);
        QCOMPARE(b.textsList().first().toMap().value("text").toString(), QString("Round trip"));
        waitForImports(b);
        QCOMPARE(b.assetsList().size(), 1);
        QVERIFY2(!b.assetsList().first().toMap().value("thumbnailPath").toString().isEmpty(),
                 "a reopened project must get its previews back");
        QVERIFY(!b.canUndo());
    }

    void exportArgsContainFiltersAndSubtitles() {
        Project p;
        TimelineClip c;
        c.id = "clip_1";
        c.sourcePath = "/tmp/demo.mp4";
        c.kind = "video";
        c.durationSec = 4.0;
        c.brightness = 0.1;
        c.audioDenoise = true;
        c.audioNormalize = true;
        p.clips.append(c);

        ExportOptions opts;
        opts.preset = "high";
        opts.codec = "h264";
        const QStringList args = MediaEngine::buildExportArgs(p, "/tmp/out.mp4", opts, "/tmp/subs.ass");
        const QString joined = args.join(" ");
        QVERIFY(joined.contains("eq=brightness=0.100"));
        QVERIFY(joined.contains("afftdn=nf=-25"));
        QVERIFY(joined.contains("loudnorm="));
        QVERIFY(joined.contains("subtitles="));
        QVERIFY(joined.contains("libx264"));
    }

    // A still on the audio lane has no sound to give; asking ffmpeg for its audio fails the export.
    void exportSkipsImageOnAudioTrack() {
        Project p;
        TimelineClip video;
        video.id = "v";
        video.kind = "video";
        video.sourcePath = "/tmp/base.mp4";
        video.track = 0;
        video.durationSec = 2.0;
        video.sourceDurationSec = 2.0;
        p.clips.append(video);
        TimelineClip still;
        still.id = "i";
        still.kind = "image";
        still.sourcePath = "/tmp/picture.png";
        still.track = 2;
        still.durationSec = 2.0;
        still.sourceDurationSec = 2.0;
        p.clips.append(still);

        const QStringList args = MediaEngine::buildExportArgs(p, "/tmp/out.mp4", ExportOptions{}, {});
        QVERIFY2(args.contains("/tmp/base.mp4"), "positive control: the video clip is an input");
        QVERIFY2(!args.contains("/tmp/picture.png"), qPrintable(args.join(' ')));
    }

    void backendSplitAndUndoRedo() {
        QTemporaryDir dir;
        const QString vid = dir.path() + "/sample.mp4";
        QProcess ff;
        ff.start("ffmpeg", {"-y", "-f", "lavfi", "-i", "color=c=blue:s=320x240:d=3",
                            "-f", "lavfi", "-i", "anullsrc=r=44100:cl=mono", "-t", "3",
                            "-c:v", "libx264", "-c:a", "aac", vid});
        QVERIFY(ff.waitForFinished(15000));
        QCOMPARE(ff.exitCode(), 0);

        Backend b;
        b.importSingleMedia(vid);
        waitForImports(b);
        QCOMPARE(b.clipsList().size(), 1);
        b.setPlayheadSec(1.5);
        b.splitAtPlayhead();
        QCOMPARE(b.clipsList().size(), 2);
        QVERIFY(b.canUndo());
        b.undo();
        QCOMPARE(b.clipsList().size(), 1);
        b.redo();
        QCOMPARE(b.clipsList().size(), 2);
    }

    // Previews are derived from the file, not edited by the user: undo must not take them away.
    void testUndoKeepsPreviews() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "undo.mp4", "0x445566", 2.0);
        QVERIFY(!vid.isEmpty());

        Backend b;
        b.importSingleMedia(vid);
        waitForImports(b);
        QCOMPARE(b.assetsList().size(), 1);
        QVERIFY2(!b.assetsList().first().toMap().value("thumbnailPath").toString().isEmpty(),
                 "the previews never arrived, so the test cannot observe them being lost");

        auto checkPreviews = [&](const char *when) {
            QCOMPARE(b.assetsList().size(), 1);
            const QVariantMap asset = b.assetsList().first().toMap();
            QVERIFY2(!asset.value("thumbnailPath").toString().isEmpty(), when);
            QVERIFY2(!asset.value("filmstripPath").toString().isEmpty(), when);
            QVERIFY2(asset.value("waveformPeaks").toList().size() == 80, when);
        };

        QVERIFY(b.canUndo());
        b.undo();
        checkPreviews("after undo");
        b.redo();
        checkPreviews("after redo");
        QCOMPARE(b.clipsList().size(), 1);
        QVERIFY2(!b.clipsList().first().toMap().value("filmstripPath").toString().isEmpty(), "clip after redo");
    }

    void exportArgsChainXfadeForAdjacentTransition() {
        Project p;
        TimelineClip a;
        a.id = "a";
        a.sourcePath = "/tmp/a.mp4";
        a.durationSec = 2.0;
        a.sourceDurationSec = 2.0;
        TimelineClip b = a;
        b.id = "b";
        b.sourcePath = "/tmp/b.mp4";
        b.timelineStartSec = 2.0;
        b.transitionIn = "wipeleft";
        b.transitionDurationSec = 0.5;
        p.clips = {a, b};

        const QString joined = MediaEngine::buildExportArgs(p, "/tmp/out.mp4", ExportOptions{}, {}).join(" ");
        QVERIFY2(joined.contains("xfade=transition=wipeleft:duration=0.500:offset=2.000"), qPrintable(joined));
        // Source has no spare frames after the out point, so the outgoing clip holds its last frame.
        QVERIFY(joined.contains("tpad=stop_mode=clone:stop_duration=0.500"));
        QVERIFY(joined.contains("afade=t=in:st=0:d=0.500"));
    }

    void exportRendersCrossfadeAtTimelinePosition() {
        QTemporaryDir dir;
        const QString teal = makeSampleVideo(dir.path(), "teal.mp4", "0x008080", 2.0);
        const QString red = makeSampleVideo(dir.path(), "red.mp4", "0xff0000", 2.0);
        QVERIFY(!teal.isEmpty() && !red.isEmpty());

        Project p;
        p.width = 320;
        p.height = 240;
        double start = 0.0;
        for (const QString &path : {teal, red}) {
            QString err;
            const MediaAsset asset = MediaEngine::probeMedia(path, &err);
            QVERIFY2(err.isEmpty(), qPrintable(err));
            p.assets.append(asset);
            TimelineClip c;
            c.id = asset.id;
            c.assetId = asset.id;
            c.sourcePath = asset.path;
            c.kind = asset.kind;
            c.timelineStartSec = start;
            c.durationSec = asset.durationSec;
            c.sourceDurationSec = asset.durationSec;
            start += asset.durationSec;
            p.clips.append(c);
        }
        p.clips[1].transitionIn = "dissolve";
        p.clips[1].transitionDurationSec = 0.5;

        const QString out = dir.path() + "/out.mp4";
        QProcess ff;
        ff.start("ffmpeg", MediaEngine::buildExportArgs(p, out, ExportOptions{"fast", "h264", 320, 240}, {}));
        QVERIFY(ff.waitForFinished(60000));
        QVERIFY2(ff.exitCode() == 0, qPrintable(QString::fromUtf8(ff.readAllStandardError()).right(800)));
        QVERIFY(std::abs(probeDurationSec(out) - start) < 0.15);

        auto averageColorAt = [&](double sec) {
            QProcess grab;
            grab.start("ffmpeg", {"-v", "error", "-ss", QString::number(sec), "-i", out, "-frames:v", "1",
                                  "-vf", "scale=1:1:flags=area", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"});
            grab.waitForFinished(10000);
            const QByteArray px = grab.readAllStandardOutput();
            return px.size() >= 3 ? QColor(quint8(px[0]), quint8(px[1]), quint8(px[2])) : QColor();
        };
        const QColor before = averageColorAt(1.0);
        const QColor during = averageColorAt(2.25);
        const QColor after = averageColorAt(3.5);
        QVERIFY2(before.red() < 40 && before.green() > 100, qPrintable(before.name()));
        QVERIFY2(after.red() > 200 && after.green() < 40, qPrintable(after.name()));
        QVERIFY2(during.red() > 40 && during.green() > 30, qPrintable("no blend mid-transition: " + during.name()));
    }

    // The preview must show the colours the export produces: run each look through
    // ClipFrameItem::process and through the FFmpeg filters used on export, and compare.
    void previewLookMatchesFfmpegFilters() {
        QTemporaryDir dir;
        const QString src = dir.path() + "/src.png";
        QProcess mk;
        mk.start("ffmpeg", {"-y", "-v", "error", "-f", "lavfi", "-i", "testsrc2=s=320x240:d=1", "-frames:v", "1", src});
        QVERIFY(mk.waitForFinished(10000) && mk.exitCode() == 0);
        const QImage source(src);
        QVERIFY(!source.isNull());

        struct Case {
            const char *name;
            ClipLook look;
            QString filters;
        };
        ClipLook eq;
        eq.brightness = 0.1;
        eq.contrast = 1.4;
        eq.saturation = 0.3;
        ClipLook blur;
        blur.blurRadius = 3;
        ClipLook vignette;
        vignette.vignette = true;
        ClipLook key;
        key.chromaKey = true;
        key.keyColor = QColor("#00ff00");
        key.similarity = 0.18;
        key.blend = 0.08;
        const QList<Case> cases = {
            {"eq", eq, "eq=brightness=0.100:contrast=1.400:saturation=0.300"},
            {"boxblur", blur, "boxblur=3.0:1"},
            {"vignette", vignette, "vignette"},
            {"chromakey", key, "chromakey=0x00ff00:0.180:0.080"},
        };
        for (const Case &c : cases) {
            const QString outPath = dir.path() + "/" + c.name + ".png";
            QProcess ff;
            ff.start("ffmpeg", {"-y", "-v", "error", "-i", src, "-vf", "format=yuva420p," + c.filters + ",format=rgba",
                                "-frames:v", "1", outPath});
            QVERIFY(ff.waitForFinished(10000) && ff.exitCode() == 0);
            const QImage expected = QImage(outPath).convertToFormat(QImage::Format_RGBA8888);
            const QImage actual = ClipFrameItem::process(source, c.look).convertToFormat(QImage::Format_RGBA8888);
            QCOMPARE(actual.size(), expected.size());

            double colourError = 0.0;
            double alphaError = 0.0;
            for (int y = 0; y < actual.height(); ++y) {
                for (int x = 0; x < actual.width(); ++x) {
                    const QColor a = actual.pixelColor(x, y);
                    const QColor e = expected.pixelColor(x, y);
                    // Colour of a fully keyed-out pixel is invisible, so weight it by coverage.
                    const double visible = std::min(a.alpha(), e.alpha()) / 255.0;
                    colourError += visible * (std::abs(a.red() - e.red()) + std::abs(a.green() - e.green()) + std::abs(a.blue() - e.blue())) / 3.0;
                    alphaError += std::abs(a.alpha() - e.alpha());
                }
            }
            const double pixels = actual.width() * actual.height();
            QVERIFY2(colourError / pixels < 6.0 && alphaError / pixels < 6.0,
                     qPrintable(QString("%1: mean colour error %2, alpha error %3 (0-255)")
                                    .arg(c.name).arg(colourError / pixels, 0, 'f', 2).arg(alphaError / pixels, 0, 'f', 2)));
        }
    }

    // The preview applies the look in a GPU shader (src/shaders/look.frag) rather than the CPU
    // pixel loop. The shader must reproduce ClipFrameItem::process() within float rounding:
    // worst per-channel delta at most 2, per effect and all effects combined.
    void gpuLookShaderMatchesCpuProcess() {
        QTemporaryDir dir;
        const int W = 240, H = 135; // small enough that the blur taps stay cheap on llvmpipe
        QImage img(W, H, QImage::Format_ARGB32);
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                const int r = (x * 255) / (W - 1);
                const int g = (y * 255) / (H - 1);
                const int b = ((x + y) * 255) / (W + H - 2);
                img.setPixel(x, y, qRgba(r, g, b, 255));
            }
        }
        // A saturated green patch, so the chroma key has something to bite on.
        for (int y = H / 3; y < 2 * H / 3; ++y)
            for (int x = W / 3; x < 2 * W / 3; ++x)
                img.setPixel(x, y, qRgba(0, 255, 0, 255));
        const QString png = dir.path() + "/frame.png";
        QVERIFY(img.save(png));

        struct Case {
            const char *name;
            ClipLook look;
        };
        ClipLook eq;
        eq.brightness = 0.1;
        eq.contrast = 1.4;
        eq.saturation = 0.3;
        ClipLook blur;
        blur.blurRadius = 8;
        ClipLook vig;
        vig.vignette = true;
        ClipLook key;
        key.chromaKey = true;
        key.keyColor = QColor("#00ff00");
        key.similarity = 0.18;
        key.blend = 0.08;
        ClipLook all;
        all.brightness = 0.05;
        all.contrast = 1.2;
        all.saturation = 1.5;
        all.blurRadius = 3;
        all.vignette = true;
        all.chromaKey = true;
        all.keyColor = QColor("#00ff00");
        all.similarity = 0.18;
        all.blend = 0.08;
        const QList<Case> cases = {
            {"eq", eq}, {"blur r8", blur}, {"vignette", vig}, {"chromakey", key}, {"all combined", all},
        };

        QQuickWindow window;
        window.setColor(Qt::black); // so the grab shows the premultiplied shader output as-is
        window.resize(W, H);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY2(window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software,
                 "the look shader needs an RHI scene graph; run with QT_QUICK_BACKEND=rhi");
        auto *item = new ClipFrameItem(window.contentItem());
        item->setSize(QSizeF(W, H)); // 1:1 with the image, so texel centres line up exactly
        item->setImageSource(png);
        QTRY_VERIFY_WITH_TIMEOUT(item->hasFrame(), 10000);

        for (const Case &c : cases) {
            item->setBrightness(c.look.brightness);
            item->setContrast(c.look.contrast);
            item->setSaturation(c.look.saturation);
            item->setBlur(c.look.blurRadius);
            item->setVignette(c.look.vignette);
            item->setChromaKey(c.look.chromaKey);
            item->setKeyColor(c.look.keyColor);
            item->setSimilarity(c.look.similarity);
            item->setBlend(c.look.blend);
            // grabWindow() syncs and renders the scene, so the shader runs with the new look.
            const QImage shot = window.grabWindow();
            QCOMPARE(shot.size(), QSize(W, H));
            const QImage expected = ClipFrameItem::process(img, c.look);
            const QImage actual = shot;
            int maxDelta = 0;
            double meanDelta = 0.0;
            imageDelta(actual, expected, &maxDelta, &meanDelta);
            QVERIFY2(maxDelta <= 2,
                     qPrintable(QString("%1: GPU shader vs CPU process() max channel delta %2, mean %3")
                                    .arg(c.name).arg(maxDelta).arg(meanDelta, 0, 'f', 3)));
            qDebug("gpuLookShaderMatchesCpuProcess %s: max delta %d, mean %.3f", c.name, maxDelta, meanDelta);
        }

        // Back on the identity look the item must drop the shader again and show the source
        // exactly (the shader path would round-trip through YUV and lose a little precision).
        item->setBrightness(0.0);
        item->setContrast(1.0);
        item->setSaturation(1.0);
        item->setBlur(0.0);
        item->setVignette(false);
        item->setChromaKey(false);
        const QImage idShot = window.grabWindow();
        QCOMPARE(idShot.size(), QSize(W, H));
        int maxDelta = 0;
        double meanDelta = 0.0;
        imageDelta(idShot, img.convertToFormat(QImage::Format_ARGB32_Premultiplied), &maxDelta, &meanDelta);
        QVERIFY2(maxDelta <= 1,
                 qPrintable(QString("identity after look: max channel delta %1, mean %2")
                                .arg(maxDelta).arg(meanDelta, 0, 'f', 3)));
    }

    // The audit measured the CPU loop at ~44-54 ms per 1400x788 frame (saturation / blur r8 /
    // chroma), capping the preview near 20 fps. With the look on the GPU the item's pipeline
    // must not run that loop at all: the assertion is on the pixel counter and on the rendered
    // frame; the timings are recorded for the report.
    void gpuPreviewSkipsCpuPixelLoop() {
        QTemporaryDir dir;
        const int W = 1400, H = 788;
        QImage img(W, H, QImage::Format_ARGB32);
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                const int r = (x * 7 + y * 3) % 256;
                const int g = (x * 3 + y * 11) % 256;
                const int b = ((x ^ y) * 5) % 256;
                img.setPixel(x, y, qRgba(r, g, b, 255));
            }
        }
        const QString png = dir.path() + "/big.png";
        QVERIFY(img.save(png));

        ClipLook sat;
        sat.saturation = 0.4;
        ClipLook blur;
        blur.blurRadius = 8;
        ClipLook key;
        key.chromaKey = true;
        key.keyColor = QColor("#00ff00");
        const QList<QPair<const char *, ClipLook>> looks = {
            {"saturation", sat}, {"blur r8", blur}, {"chromakey", key},
        };

        // What the CPU path costs for the same three looks (the "before" numbers).
        const qint64 pixelsBefore = ClipFrameItem::cpuPixelsProcessed();
        double cpuMs = 0.0;
        for (const auto &entry : looks) {
            QElapsedTimer t;
            t.start();
            const QImage out = ClipFrameItem::process(img, entry.second);
            const double ms = t.elapsed();
            QVERIFY(!out.isNull());
            qDebug("CPU process() %s on %dx%d: %.1f ms", entry.first, W, H, ms);
            if (qstrcmp(entry.first, "saturation") == 0)
                cpuMs = ms;
        }
        QCOMPARE(ClipFrameItem::cpuPixelsProcessed() - pixelsBefore, qint64(3) * W * H);

        QQuickWindow window;
        window.setColor(Qt::black); // so the grab shows the premultiplied shader output as-is
        window.resize(W, H);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software);
        auto *item = new ClipFrameItem(window.contentItem());
        item->setSize(QSizeF(W, H));
        item->setImageSource(png);
        QTRY_VERIFY_WITH_TIMEOUT(item->hasFrame(), 20000);

        // A non-identity look on an existing frame only moves shader uniforms.
        item->setSaturation(0.4);
        QElapsedTimer t;
        t.start();
        QTRY_VERIFY_WITH_TIMEOUT(item->hasFrame(), 20000);
        const qint64 pipelineMs = t.elapsed();
        qDebug("GPU-path pipeline for %dx%d with a look: %lld ms (CPU process() saturation: %.1f ms)",
               W, H, pipelineMs, cpuMs);
        // Not one pixel of this frame went through the CPU loop.
        QCOMPARE(ClipFrameItem::cpuPixelsProcessed() - pixelsBefore, qint64(3) * W * H);

        // And what the shader drew still matches the CPU reference.
        const QImage shot = window.grabWindow();
        QCOMPARE(shot.size(), QSize(W, H));
        const QImage expected = ClipFrameItem::process(img, sat);
        const QImage actual = shot;
        int maxDelta = 0;
        double meanDelta = 0.0;
        imageDelta(actual, expected, &maxDelta, &meanDelta);
        QVERIFY2(maxDelta <= 2,
                 qPrintable(QString("saturation: GPU vs CPU max channel delta %1, mean %2")
                                .arg(maxDelta).arg(meanDelta, 0, 'f', 3)));
    }

    // A preview nobody can see must not burn a core on frames; it catches up when shown again.
    void hiddenPreviewDoesNoWork() {
        QTemporaryDir dir;
        const QString png = dir.path() + "/still.png";
        QImage still(64, 36, QImage::Format_RGB32);
        still.fill(Qt::red);
        QVERIFY(still.save(png));

        QQuickWindow window;
        window.resize(320, 240);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *item = new ClipFrameItem(window.contentItem());
        item->setSize(QSizeF(160, 90));

        item->setVisible(false);
        item->setImageSource(png);
        QTest::qWait(600);
        QVERIFY2(!item->hasFrame(), "a hidden preview processed its source");

        item->setVisible(true);
        QTRY_VERIFY2_WITH_TIMEOUT(item->hasFrame(), "the preview did not catch up when shown", 5000);
    }

    void dropSnapsToNeighbourEdges() {
        QTemporaryDir dir;
        const QString a = makeSampleVideo(dir.path(), "a.mp4", "blue", 2.0);
        const QString b = makeSampleVideo(dir.path(), "b.mp4", "green", 2.0);
        QVERIFY(!a.isEmpty() && !b.isEmpty());
        Backend backend;
        backend.importSingleMedia(a);              // V1 0..2
        waitForImports(backend);
        const QString firstId = backend.assetsList().first().toMap().value("id").toString();
        backend.importSingleMedia(b);              // V1 appended at 2.0
        waitForImports(backend);
        backend.setPlayheadSec(6.0);

        // 0.1s after the end of the existing clip: snap to the edge instead of leaving a sliver gap.
        backend.addAssetToTimeline(firstId, 0, 2.1);
        QCOMPARE(backend.clipsList().last().toMap().value("timelineStartSec").toDouble(), 2.0);
        // Within 0.1s of the playhead: lands exactly on it.
        backend.addAssetToTimeline(firstId, 0, 5.92);
        QCOMPARE(backend.clipsList().last().toMap().value("timelineStartSec").toDouble(), 6.0);
        // Mid-gap: kept where dropped.
        backend.addAssetToTimeline(firstId, 0, 3.4);
        QVERIFY(std::abs(backend.clipsList().last().toMap().value("timelineStartSec").toDouble() - 3.4) < 1e-6);
    }

    void fileDropLandsOnLaneUnderPointer() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "drop.mp4", "orange", 2.0);
        const QString wav = dir.path() + "/tone.wav";
        QProcess tone;
        tone.start("ffmpeg", {"-y", "-v", "error", "-f", "lavfi", "-i", "sine=frequency=300:duration=2", wav});
        QVERIFY(tone.waitForFinished(10000) && tone.exitCode() == 0 && !vid.isEmpty());

        Backend backend;
        // A video dropped on V2 stays on V2; audio dropped on V2 is routed to A1.
        backend.importMediaAt({QUrl::fromLocalFile(vid)}, 1, 4.0);
        waitForImports(backend);
        QCOMPARE(backend.clipsList().size(), 1);
        QCOMPARE(backend.clipsList().first().toMap().value("track").toInt(), 1);
        QCOMPARE(backend.clipsList().first().toMap().value("timelineStartSec").toDouble(), 4.0);

        backend.importMediaAt({QUrl::fromLocalFile(wav)}, 1, 1.0);
        waitForImports(backend);
        QCOMPARE(backend.clipsList().last().toMap().value("track").toInt(), 2);

        // Two files: packed one after another from the drop point, not stacked.
        backend.importMediaAt({QUrl::fromLocalFile(vid), QUrl::fromLocalFile(vid)}, 0, 0.0);
        waitForImports(backend);
        const auto clips = backend.clipsList();
        QCOMPARE(clips.size(), 4);
        QCOMPARE(clips[clips.size() - 2].toMap().value("track").toInt(), 0);
        QCOMPARE(clips[clips.size() - 2].toMap().value("timelineStartSec").toDouble(), 0.0);
        QCOMPARE(clips.last().toMap().value("timelineStartSec").toDouble(), 2.0);
    }

    // Lane mute/solo must reach the export audio chain, not just the preview.
    void laneMuteAndSoloControlAudio() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "lane.mp4", "blue", 2.0);
        QVERIFY(!vid.isEmpty());
        Backend backend;
        backend.importSingleMedia(vid);
        waitForImports(backend);
        const auto audioFilters = [&] {
            const QString joined = MediaEngine::buildExportArgs(backend.project(), "/tmp/o.mp4", ExportOptions{}, {}).join(" ");
            return joined.count("[1:a]");
        };
        QVERIFY2(audioFilters() == 1, "clip audio must be in the chain by default");

        backend.setLaneState(0, "muted", true);
        QVERIFY2(audioFilters() == 0, "a muted V1 lane must contribute no audio");
        backend.setLaneState(0, "muted", false);

        backend.setLaneState(0, "solo", true);
        QVERIFY(audioFilters() == 1);
        backend.setLaneState(0, "solo", false);
        backend.setLaneState(2, "solo", true); // solo A1: V1 audio is excluded
        QVERIFY(audioFilters() == 0);
        backend.setLaneState(2, "solo", false);
        QVERIFY(audioFilters() == 1);
        QVERIFY(backend.lanesList().size() == 3);
    }

    void laneLockBlocksEdits() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "lock.mp4", "green", 2.0);
        QVERIFY(!vid.isEmpty());
        Backend backend;
        backend.importSingleMedia(vid);
        waitForImports(backend);
        const QString clipId = backend.selectedClipId();
        backend.setPlayheadSec(0.5);
        backend.setLaneState(0, "locked", true);
        QVERIFY(backend.selectedClipId().isEmpty()); // lock drops selection from that lane

        backend.selectClip(clipId);
        backend.updateClipProperty(clipId, "volume", 0.1);
        QCOMPARE(backend.selectedClip().value("volume").toDouble(), 1.0);
        backend.applyLookPreset(clipId, "mono");
        QCOMPARE(backend.selectedClip().value("saturation").toDouble(), 1.0);
        backend.addKeyframeAtPlayhead(clipId);
        QCOMPARE(backend.selectedClip().value("keyframes").toList().size(), 0);
        backend.updateTransformAtPlayhead(clipId, 0.2, 0.2, 0.5, 0.8, 15.0);
        QCOMPARE(backend.selectedClip().value("keyframes").toList().size(), 0);
        backend.selectClip(clipId);
        backend.deleteSelected(false);
        QCOMPARE(backend.clipsList().size(), 1);

        backend.setLaneState(0, "locked", false);
        backend.selectClip(clipId);
        backend.updateClipProperty(clipId, "volume", 0.1);
        QVERIFY(std::abs(backend.selectedClip().value("volume").toDouble() - 0.1) < 1e-6);
    }

    void exportCapabilitiesAndEstimate() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "est.mp4", "teal", 4.0);
        QVERIFY(!vid.isEmpty());
        Backend backend;
        QTRY_VERIFY_WITH_TIMEOUT(backend.capabilities().value("encoders").toMap().value("h264").toBool(), 15000);
        const QVariantMap caps = backend.capabilities();
        const QVariantMap encoders = caps.value("encoders").toMap();
        QVERIFY(encoders.contains("h264"));
        QCOMPARE(encoders.value("h264").toBool(), true);

        backend.importSingleMedia(vid);
        waitForImports(backend);
        const QVariantMap projHigh = backend.estimateExport("high", "h264", "project");
        const QVariantMap projFast = backend.estimateExport("fast", "h264", "project");
        const QVariantMap uhdHigh = backend.estimateExport("high", "h264", "4k");
        const QVariantMap vertHigh = backend.estimateExport("high", "h264", "vertical");

        QCOMPARE(projHigh.value("width").toInt(), 1920);
        QCOMPARE(projHigh.value("height").toInt(), 1080);
        QCOMPARE(uhdHigh.value("width").toInt(), 3840);
        QCOMPARE(uhdHigh.value("height").toInt(), 2160);
        QCOMPARE(vertHigh.value("width").toInt(), 1080);
        QCOMPARE(vertHigh.value("height").toInt(), 1920);
        QCOMPARE(projHigh.value("codecSupported").toBool(), true);
        QVERIFY(projHigh.value("estimatedBytes").toLongLong() > projFast.value("estimatedBytes").toLongLong());
        QVERIFY(uhdHigh.value("estimatedBytes").toLongLong() > projHigh.value("estimatedBytes").toLongLong());
        QVERIFY(!projHigh.value("estimatedSizeLabel").toString().isEmpty());
    }

    // A failed export must say why. The ready-read handler drains ffmpeg's stderr for the progress
    // bar, so the tail has to survive for the finished handler.
    void testExportErrorHasReason() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "gone.mp4", "blue", 1.0);
        QVERIFY(!vid.isEmpty());

        Backend b;
        QTRY_VERIFY_WITH_TIMEOUT(b.capabilities().value("encoders").toMap().value("h264").toBool(), 15000);
        b.importSingleMedia(vid);
        waitForImports(b);
        QCOMPARE(b.clipsList().size(), 1);

        // The source disappears between import and export, so ffmpeg fails with a real message.
        QVERIFY(QFile::remove(vid));

        QSignalSpy finished(&b, &Backend::exportFinished);
        b.startExport(dir.path() + "/out.mp4", "fast", "h264", "project");
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 60000);

        const QList<QVariant> result = finished.takeFirst();
        QCOMPARE(result.value(0).toBool(), false);
        const QString message = result.value(1).toString();
        QVERIFY2(message.contains("No such file", Qt::CaseInsensitive), qPrintable("no reason given: " + message));
        QVERIFY2(!message.startsWith("ffmpeg exited with code"), qPrintable("only the exit code: " + message));
        QVERIFY2(b.statusMessage().startsWith("Export failed") &&
                     b.statusMessage().contains("No such file", Qt::CaseInsensitive),
                 qPrintable(b.statusMessage()));
    }

    // Cancelling must remove the half-written file and must not be reported as a failure reason.
    void testCancelRemovesPartial() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "long.mp4", "0x224466", 8.0);
        QVERIFY(!vid.isEmpty());

        Backend b;
        QTRY_VERIFY_WITH_TIMEOUT(b.capabilities().value("encoders").toMap().value("h264").toBool(), 15000);
        b.importSingleMedia(vid);
        waitForImports(b);
        QCOMPARE(b.clipsList().size(), 1);

        const QString out = dir.path() + "/partial.mp4";
        QSignalSpy finished(&b, &Backend::exportFinished);
        b.startExport(out, "high", "h264", "project");
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(out), 20000);
        QVERIFY2(b.exporting(), "the export finished before it could be cancelled");

        b.cancelExport();
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 20000);
        const QList<QVariant> result = finished.takeFirst();
        QCOMPARE(result.value(0).toBool(), false);
        QCOMPARE(result.value(1).toString(), QString("cancelled"));
        QVERIFY2(!QFileInfo::exists(out), "cancel must remove the partial output");
        QCOMPARE(b.statusMessage(), QString("Export cancelled"));
    }

    // An export that runs to the end must say so, write a playable file of the right length, and
    // honour the vertical resolution preset.
    void exportFinishesAndPlays() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "finish.mp4", "0x336699", 2.0);
        QVERIFY(!vid.isEmpty());

        Backend b;
        QTRY_VERIFY_WITH_TIMEOUT(b.capabilities().value("encoders").toMap().value("h264").toBool(), 15000);
        b.importSingleMedia(vid);
        waitForImports(b);
        b.addTextAtPlayhead("Burned in");
        // An export runs as long as the longest item on the timeline. Backend::totalDuration() has a
        // 10 s floor, so the longest end is taken from the clip and text lists instead.
        double expected = 0.0;
        for (const QVariant &c : b.clipsList())
            expected = std::max(expected, c.toMap().value("timelineStartSec").toDouble() + c.toMap().value("durationSec").toDouble());
        for (const QVariant &t : b.textsList())
            expected = std::max(expected, t.toMap().value("startSec").toDouble() + t.toMap().value("durationSec").toDouble());
        QVERIFY2(expected >= 2.0, "the timeline is shorter than its clip");

        for (const QString &preset : {QString("project"), QString("vertical")}) {
            const QString out = dir.path() + "/finish-" + preset + ".mp4";
            QSignalSpy finished(&b, &Backend::exportFinished);
            b.startExport(out, "fast", "h264", preset);
            QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 120000);

            const QList<QVariant> result = finished.takeFirst();
            QCOMPARE(result.value(0).toBool(), true);
            QVERIFY2(result.value(1).toString().startsWith("Saved to "), qPrintable(result.value(1).toString()));
            QVERIFY2(QFileInfo::exists(out), qPrintable(out));
            QVERIFY2(QFileInfo(out).size() > 1000, qPrintable(preset + " output is too small"));
            QVERIFY2(std::abs(probeDurationSec(out) - expected) < 0.3, qPrintable(preset + " duration is off"));
            QVERIFY(!b.exporting());
            QCOMPARE(b.exportProgress(), 1.0);

            if (preset == "vertical") {
                QProcess p;
                p.start("ffprobe", {"-v", "error", "-select_streams", "v:0", "-show_entries",
                                    "stream=width,height", "-of", "csv=p=0", out});
                QVERIFY(p.waitForFinished(10000));
                QCOMPARE(QString::fromUtf8(p.readAllStandardOutput()).trimmed(), QString("1080,1920"));
            }
        }
    }

    // An export that dies with the app must not leave a truncated file that looks like a video.
    void testQuitDuringExportRemovesPartial() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "long.mp4", "0x224466", 8.0);
        QVERIFY(!vid.isEmpty());
        const QString out = dir.path() + "/partial.mp4";
        {
            Backend b;
            QTRY_VERIFY_WITH_TIMEOUT(b.capabilities().value("encoders").toMap().value("h264").toBool(), 15000);
            b.importSingleMedia(vid);
            waitForImports(b);
            b.startExport(out, "high", "h264", "project");
            QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(out), 20000);
            QVERIFY2(b.exporting(), "the export finished before the app was closed");
        }
        QVERIFY2(!QFileInfo::exists(out), "quitting mid-export left a partial output file");
    }

    void testCloseAsksWhileExporting() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vid = makeSampleVideo(dir.path(), "src.mp4", "0x224466", 8.0);
        QVERIFY(!vid.isEmpty());

        QQuickStyle::setStyle("Material");
        Theme theme(dir.path());
        Design design(theme);
        Backend backend;
        // Register the module's types before injecting instances: a singleton instance seals the
        // module version, and the module's own registration would then be dropped.
        extern void qml_register_types_CutCap();
        qml_register_types_CutCap();
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Theme", &theme);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Design", &design);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Backend", &backend);
        QStringList qmlErrors;
        g_qmlErrors = &qmlErrors;
        g_previousHandler = qInstallMessageHandler(captureQmlErrors);
        QQmlApplicationEngine engine;
        engine.load(QUrl("qrc:/qml/Main.qml"));
        qInstallMessageHandler(g_previousHandler);
        g_qmlErrors = nullptr;
        QVERIFY2(engine.rootObjects().size() == 1, qPrintable(qmlErrors.join("\n")));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QTRY_VERIFY_WITH_TIMEOUT(backend.capabilities().value("encoders").toMap().value("h264").toBool(), 15000);
        backend.importSingleMedia(vid);
        waitForImports(backend);
        // Saved, so the unsaved-changes guard is out of the way and only the running export counts.
        QVERIFY(backend.saveProject(dir.path() + "/busy.cutcap"));
        QVERIFY(!backend.dirty());

        const QString out = dir.path() + "/busy.mp4";
        backend.startExport(out, "high", "h264", "project");
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(out), 20000);
        QVERIFY2(backend.exporting(), "the export finished before the window was closed");

        window->close();
        QTest::qWait(300);
        QVERIFY2(window->isVisible(), "closing during an export must ask first");
        auto *dialog = window->findChild<QObject *>("busyCloseDialog");
        QVERIFY2(dialog, "the still-working dialog does not exist");
        QVERIFY2(dialog->property("opened").toBool(), "the still-working dialog did not open");

        // Keep working: the dialog goes away, the window and the export stay.
        auto *keepButton = window->findChild<QObject *>("busyKeepButton");
        QVERIFY2(keepButton, "the Keep working button does not exist");
        QVERIFY(QMetaObject::invokeMethod(keepButton, "clicked"));
        QTest::qWait(200);
        QVERIFY(window->isVisible());
        QVERIFY2(backend.exporting(), "Keep working must not stop the export");

        // Quit anyway: the window closes.
        window->close();
        QTest::qWait(300);
        QVERIFY(window->isVisible());
        auto *quitButton = window->findChild<QObject *>("busyQuitButton");
        QVERIFY2(quitButton, "the Quit anyway button does not exist");
        QVERIFY(QMetaObject::invokeMethod(quitButton, "clicked"));
        QTest::qWait(300);
        QVERIFY2(!window->isVisible(), "Quit anyway must close the window");
        backend.cancelExport();
    }

    void previewSnapExposesGuide() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "snap.mp4", "teal", 2.0);
        QVERIFY(!vid.isEmpty());
        Backend backend;
        backend.importSingleMedia(vid);            // V1 0..2
        waitForImports(backend);
        const QString assetId = backend.assetsList().first().toMap().value("id").toString();
        backend.addAssetToTimeline(assetId, 0, 4.0); // edge at 4..6, playhead 0
        backend.setPlayheadSec(0.0);

        QVERIFY(backend.snapGuideSec() < 0);
        QCOMPARE(backend.previewSnap("", 3.93, 0), 4.0);   // snaps to the clip start
        QCOMPARE(backend.snapGuideSec(), 4.0);
        QCOMPARE(backend.previewSnap("", 3.0, 0), 3.0);    // no target nearby
        QVERIFY(backend.snapGuideSec() < 0);
        backend.previewSnap("", 5.97, 0);
        QCOMPARE(backend.snapGuideSec(), 6.0);             // snaps to the clip end
        backend.clearSnapGuide();
        QVERIFY(backend.snapGuideSec() < 0);
    }

    void lookPresetAppliesAndUndoes() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "preset.mp4", "orange", 2.0);
        QVERIFY(!vid.isEmpty());
        Backend backend;
        backend.importSingleMedia(vid);
        waitForImports(backend);
        const QString clipId = backend.selectedClipId();
        backend.applyLookPreset(clipId, "mono");
        QCOMPARE(backend.selectedClip().value("saturation").toDouble(), 0.0);
        QVERIFY(backend.selectedClip().value("contrast").toDouble() > 1.0);
        backend.applyLookPreset(clipId, "punch");
        QVERIFY(backend.selectedClip().value("saturation").toDouble() > 1.0);
        backend.undo();
        QCOMPARE(backend.selectedClip().value("saturation").toDouble(), 0.0);
        backend.applyLookPreset(clipId, "neutral");
        QCOMPARE(backend.selectedClip().value("saturation").toDouble(), 1.0);
        QCOMPARE(backend.selectedClip().value("brightness").toDouble(), 0.0);
    }

    void sliderDragIsOneUndoStep() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "undo.mp4", "blue", 2.0);
        QVERIFY(!vid.isEmpty());
        Backend b;
        b.importSingleMedia(vid);
        waitForImports(b);
        const QString id = b.selectedClipId();
        for (int i = 1; i <= 20; ++i)
            b.updateClipProperty(id, "volume", 1.0 - i * 0.04);
        b.updateClipProperty(id, "brightness", 0.2);

        b.undo();
        QCOMPARE(b.selectedClip().value("brightness").toDouble(), 0.0);
        QVERIFY(std::abs(b.selectedClip().value("volume").toDouble() - 0.2) < 1e-6);
        b.undo();
        QCOMPARE(b.selectedClip().value("volume").toDouble(), 1.0);
        QCOMPARE(b.clipsList().size(), 1); // the import itself is still one step further back
    }

    void exportAnimatesOverlayKeyframes() {
        QTemporaryDir dir;
        const QString base = makeSampleVideo(dir.path(), "base.mp4", "0x008080", 3.0);
        const QString bar = dir.path() + "/bar.mp4";
        QProcess mk;
        mk.start("ffmpeg", {"-y", "-v", "error", "-f", "lavfi", "-i", "color=c=0xff0000:s=320x80:r=30:d=3",
                            "-c:v", "libx264", "-pix_fmt", "yuv420p", bar});
        QVERIFY(mk.waitForFinished(20000) && mk.exitCode() == 0 && !base.isEmpty());

        Project p;
        p.width = 320;
        p.height = 240;
        for (const QString &path : {base, bar}) {
            QString err;
            const MediaAsset asset = MediaEngine::probeMedia(path, &err);
            QVERIFY2(err.isEmpty(), qPrintable(err));
            p.assets.append(asset);
            TimelineClip c;
            c.id = asset.id;
            c.assetId = asset.id;
            c.sourcePath = asset.path;
            c.kind = asset.kind;
            c.durationSec = c.sourceDurationSec = asset.durationSec;
            c.track = p.clips.size(); // base on V1, bar on V2
            p.clips.append(c);
        }
        // Bar slides from left to right and turns upright: 160x40 at x=25% -> 40x160 at x=75%.
        p.clips[1].keyframes = {{0.0, 0.25, 0.5, 0.5, 1.0, 0.0}, {2.0, 0.75, 0.5, 0.5, 1.0, 90.0}};

        const QString out = dir.path() + "/anim.mp4";
        QProcess ff;
        ff.start("ffmpeg", MediaEngine::buildExportArgs(p, out, ExportOptions{"fast", "h264", 320, 240}, {}));
        QVERIFY(ff.waitForFinished(60000));
        QVERIFY2(ff.exitCode() == 0, qPrintable(QString::fromUtf8(ff.readAllStandardError()).right(800)));

        auto redBox = [&](double sec) {
            QProcess grab;
            grab.start("ffmpeg", {"-v", "error", "-ss", QString::number(sec), "-i", out, "-frames:v", "1",
                                  "-f", "rawvideo", "-pix_fmt", "rgb24", "-"});
            grab.waitForFinished(10000);
            const QByteArray px = grab.readAllStandardOutput();
            QRect box;
            for (int y = 0; y < 240 && px.size() >= 320 * 240 * 3; ++y) {
                for (int x = 0; x < 320; ++x) {
                    const auto *c = reinterpret_cast<const uchar *>(px.constData()) + (y * 320 + x) * 3;
                    if (c[0] > 170 && c[1] < 90 && c[2] < 90)
                        box |= QRect(x, y, 1, 1);
                }
            }
            return box;
        };
        const QRect start = redBox(0.0);
        const QRect mid = redBox(1.0);
        const QRect end = redBox(2.5);
        QVERIFY2(std::abs(start.center().x() - 80) <= 4 && start.width() > 3 * start.height(),
                 qPrintable(QString("start box %1,%2 %3x%4").arg(start.x()).arg(start.y()).arg(start.width()).arg(start.height())));
        QVERIFY2(std::abs(mid.center().x() - 160) <= 6, qPrintable(QString("mid centre x %1").arg(mid.center().x())));
        QVERIFY2(std::abs(end.center().x() - 240) <= 4 && end.height() > 3 * end.width(),
                 qPrintable(QString("end box %1,%2 %3x%4").arg(end.x()).arg(end.y()).arg(end.width()).arg(end.height())));
    }

    // The window builds: Main.qml loads with no QML errors, exposes, and renders a frame.
    void uiSmokeWindowBuilds() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        UiHarness ui = UiHarness::loadMain(dir.path());
        QVERIFY2(ui.engine->rootObjects().size() == 1,
                 qPrintable(ui.qmlErrors.isEmpty() ? QString("Main.qml did not load") : ui.qmlErrors.join("\n")));
        QVERIFY(ui.window);
        QVERIFY(QTest::qWaitForWindowExposed(ui.window));
        QVERIFY2(ui.qmlErrors.isEmpty(), qPrintable(ui.qmlErrors.mid(0, 5).join("\n")));

        const QImage shot = ui.window->grabWindow();
        QVERIFY(!shot.isNull());
        QVERIFY(shot.save(dir.path() + "/cutcap_verify.png"));
    }

    // Every labelled control fits its label at the default window size.
    void uiSmokeButtonsFitLabels() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vid = uiSampleVideo("ui_base.mp4", "0x008080", 3.0);
        QVERIFY(!vid.isEmpty());
        UiHarness ui = UiHarness::loadMain(dir.path());
        QVERIFY2(ui.engine->rootObjects().size() == 1,
                 qPrintable(ui.qmlErrors.isEmpty() ? QString("Main.qml did not load") : ui.qmlErrors.join("\n")));
        QVERIFY(ui.window);
        QVERIFY(QTest::qWaitForWindowExposed(ui.window));
        ui.backend->importSingleMedia(vid);
        waitForImports(*ui.backend);
        QTRY_VERIFY2_WITH_TIMEOUT(findItem(ui.window->contentItem(), "assetCard"),
                                  "the media bin must show the imported card", 10000);

        QStringList problems;
        int inspected = 0;
        collectUnreadableButtons(ui.window->contentItem(), &problems, &inspected);
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join("\n")));
        QVERIFY2(inspected >= 12, qPrintable(QString("only %1 labelled controls were inspected").arg(inspected)));
    }

    // A clip moved to V2 with a corner transform renders as picture-in-picture: V2 composited
    // over V1 at 75%/75% at 40% scale, and a live effect on V1 leaves the PiP untouched.
    void uiSmokePipTransform() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString baseVid = uiSampleVideo("ui_base.mp4", "0x008080", 3.0);
        const QString pipVid = uiSampleVideo("ui_pip.mp4", "0xff0000", 3.0);
        QVERIFY(!baseVid.isEmpty() && !pipVid.isEmpty());

        UiHarness ui = UiHarness::create(dir.path());
        Backend &backend = *ui.backend;
        backend.importSingleMedia(baseVid);
        waitForImports(backend);
        backend.importSingleMedia(pipVid);
        waitForImports(backend);
        // Second import auto-appends to V1; move it onto V2 at 0s as a small bottom-right PiP.
        const QString pipClipId = backend.selectedClipId();
        backend.moveClip(pipClipId, 0.0, 1);
        backend.setPlayheadSec(1.0);
        backend.updateTransformAtPlayhead(pipClipId, 0.75, 0.75, 0.4, 1.0, 0.0);

        ui.loadEngine();
        QVERIFY2(ui.engine->rootObjects().size() == 1,
                 qPrintable(ui.qmlErrors.isEmpty() ? QString("Main.qml did not load") : ui.qmlErrors.join("\n")));
        QVERIFY(ui.window);
        QVERIFY(QTest::qWaitForWindowExposed(ui.window));
        QQuickWindow *window = ui.window;

        // Both lanes render together while paused, decoded without pressing Play, V2 composited on V1.
        QVERIFY2(window->rendererInterface()->graphicsApi() != QSGRendererInterface::Software,
                 "the preview draws video through the scene graph; run with QT_QUICK_BACKEND=rhi");
        auto *canvas = findItem(window->contentItem(), "previewCanvas");
        QVERIFY(canvas);
        for (const char *name : {"baseLane", "overlayLane"}) {
            auto *laneItem = findItem(canvas, name);
            QVERIFY(laneItem);
            // The players settle into Paused asynchronously after the window is exposed.
            QTRY_COMPARE_WITH_TIMEOUT(laneItem->property("player").value<QObject *>()->property("playbackState").toInt(),
                                      2 /* Paused */, 15000);
        }
        const qreal dpr = window->devicePixelRatio();
        auto colorAt = [&](const QImage &img, qreal fx, qreal fy) {
            const QPointF p = canvas->mapToScene(QPointF(canvas->width() * fx, canvas->height() * fy)) * dpr;
            return img.pixelColor(p.toPoint());
        };
        // Wait until both lanes have decoded a frame instead of sleeping a fixed time.
        QTRY_VERIFY2_WITH_TIMEOUT(
            [&] {
                const QImage img = window->grabWindow();
                if (img.isNull())
                    return false;
                const QColor b = colorAt(img, 0.3, 0.3);
                const QColor p = colorAt(img, 0.75, 0.75);
                return b.red() < 40 && b.green() > 100 && b.blue() > 100 && p.red() > 200 && p.green() < 50 && p.blue() < 50;
            }(),
            "V1/V2 previews did not render", 15000);
        const QImage shot = window->grabWindow();
        QVERIFY(!shot.isNull());
        const QColor base = colorAt(shot, 0.3, 0.3);
        const QColor pip = colorAt(shot, 0.75, 0.75);
        const QColor pipEdgeOutside = colorAt(shot, 0.75, 0.53);
        QVERIFY2(base.red() < 40 && base.green() > 100 && base.blue() > 100, qPrintable("V1 not drawn: " + base.name()));
        QVERIFY2(pip.red() > 200 && pip.green() < 50 && pip.blue() < 50, qPrintable("V2 not drawn: " + pip.name()));
        QVERIFY2(pipEdgeOutside.green() > 100, qPrintable("V2 larger than its 40% scale: " + pipEdgeOutside.name()));

        // Live effects: desaturating V1 must turn the teal preview grey without touching the PiP.
        const QString baseClipId = backend.clipsList().first().toMap().value("id").toString();
        backend.updateClipProperty(baseClipId, "saturation", 0.0);
        QTRY_VERIFY2_WITH_TIMEOUT(
            [&] {
                const QImage img = window->grabWindow();
                const QColor grey = colorAt(img, 0.3, 0.3);
                return std::abs(grey.red() - grey.green()) < 20 && std::abs(grey.green() - grey.blue()) < 20 && grey.red() > 40;
            }(),
            "saturation=0 not previewed", 5000);
        const QImage graded = window->grabWindow();
        const QColor pipAfter = colorAt(graded, 0.75, 0.75);
        QVERIFY2(pipAfter.red() > 200 && pipAfter.green() < 50, qPrintable("PiP changed by V1 effect: " + pipAfter.name()));
    }

    // Media lands on the timeline under the pointer: a bin-card drag while scrolled, external
    // file drops routed by geometry, and the landed clips render filmstrip/waveform visuals.
    void uiSmokeDragAndDrop() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString baseVid = uiSampleVideo("ui_base.mp4", "0x008080", 3.0);
        const QString wav = uiSampleTone();
        const QString desktopFile = uiSampleVideo("ui_desktop_drop.mp4", "0x3366cc", 2.0);
        QVERIFY(!baseVid.isEmpty() && !wav.isEmpty() && !desktopFile.isEmpty());

        UiHarness ui = UiHarness::loadMain(dir.path());
        QVERIFY2(ui.engine->rootObjects().size() == 1,
                 qPrintable(ui.qmlErrors.isEmpty() ? QString("Main.qml did not load") : ui.qmlErrors.join("\n")));
        QVERIFY(ui.window);
        QVERIFY(QTest::qWaitForWindowExposed(ui.window));
        Backend &backend = *ui.backend;
        QQuickWindow *window = ui.window;
        backend.importSingleMedia(baseVid);
        waitForImports(backend);

        // Drag a media card onto V1 while the timeline is scrolled: the clip must land under the pointer.
        auto *flick = findItem(window->contentItem(), "timelineFlick");
        auto *dropV1 = findItem(window->contentItem(), "dropV1");
        QVERIFY(flick && dropV1);
        QTRY_VERIFY2_WITH_TIMEOUT(findItem(window->contentItem(), "assetCard"),
                                  "the media bin must show the imported card", 10000);
        auto *card = findItem(window->contentItem(), "assetCard");
        QVERIFY(card);
        backend.setZoomPxPerSec(60.0);
        QTRY_VERIFY2_WITH_TIMEOUT(flick->property("contentWidth").toDouble() > 1000.0,
                                  "the zoomed timeline must overflow the viewport", 5000);
        flick->setProperty("contentX", 400.0);
        QTRY_COMPARE(flick->property("contentX").toDouble(), 400.0);

        const QPoint from = card->mapToScene(QPointF(30, 20)).toPoint();
        const QPointF dropLocal(dropV1->mapFromScene(QPointF(flick->mapToScene(QPointF(250, 0)).x(), 0)).x(),
                                dropV1->height() / 2);
        const QPoint to = dropV1->mapToScene(dropLocal).toPoint();
        const int clipsBefore = backend.clipsList().size();
        QTest::mousePress(window, Qt::LeftButton, {}, from);
        for (int i = 1; i <= 20; ++i) {
            QTest::mouseMove(window, from + (to - from) * i / 20);
            QTest::qWait(10);
        }
        QTest::mouseRelease(window, Qt::LeftButton, {}, to);
        QTRY_COMPARE(backend.clipsList().size(), clipsBefore + 1);

        const QVariantMap dropped = backend.clipsList().last().toMap();
        QCOMPARE(dropped.value("track").toInt(), 0);
        const double expectedSec = dropLocal.x() / 60.0;
        QVERIFY2(expectedSec > 400.0 / 60.0, "drop point should be in the scrolled-off region");
        QVERIFY2(std::abs(dropped.value("timelineStartSec").toDouble() - expectedSec) < 0.15,
                 qPrintable(QString("dropped at %1s, expected %2s")
                                .arg(dropped.value("timelineStartSec").toDouble())
                                .arg(expectedSec)));

        // Clip visuals are real: video clips tile filmstrip frames, audio clips draw the waveform.
        auto firstVisible = [&](const QString &name, bool needsChildren = true) -> QQuickItem * {
            std::function<QQuickItem *(QQuickItem *)> walk = [&](QQuickItem *it) -> QQuickItem * {
                // more than the Repeater itself: at least one tile
                if (it->objectName() == name && it->isVisible() &&
                    (!needsChildren || it->childItems().size() > 1))
                    return it;
                for (QQuickItem *child : it->childItems())
                    if (auto *found = walk(child))
                        return found;
                return nullptr;
            };
            return walk(window->contentItem());
        };
        QTRY_VERIFY2_WITH_TIMEOUT(firstVisible("clipStrip"), "video clips must show filmstrip frames", 10000);
        auto *strip = firstVisible("clipStrip");
        QVERIFY(strip);
        QVERIFY(strip->childItems().first()->property("source").toUrl().isLocalFile());
        // The tile size comes from the clip map, so the strip cannot silently crop a 1x1 pixel.
        auto *tile = strip->childItems().first();
        QVERIFY2(tile->property("tileWidth").toInt() > 1 && tile->property("tileHeight").toInt() > 1,
                 "the filmstrip tile size must reach QML from the clip map");
        QCOMPARE(int(tile->property("sourceClipRect").toRectF().width()), tile->property("tileWidth").toInt());

        // An audio file dropped on the bin lands on A1 and draws its waveform.
        backend.importSingleMedia(wav);
        waitForImports(backend);
        // The waveform is drawn for the visible viewport, and the new clip starts at 0 s while the
        // timeline is scrolled to 400 px, so scroll it into view before looking for it.
        const qreal scrolledX = flick->property("contentX").toDouble();
        flick->setProperty("contentX", 0.0);
        QTRY_COMPARE(flick->property("contentX").toDouble(), 0.0);
        QTRY_VERIFY2_WITH_TIMEOUT(firstVisible("clipWave", false), "audio clips must show a waveform", 10000);
        auto *wave = firstVisible("clipWave", false);
        QVERIFY(wave);
        QVERIFY2(wave->width() > 1.0 && wave->height() > 1.0,
                 qPrintable(QString("waveform is %1x%2").arg(wave->width()).arg(wave->height())));
        // The waveform canvas paints asynchronously, so wait until its pixels are on screen:
        // a canvas that paints nothing would still exist and be sized.
        QTRY_VERIFY2_WITH_TIMEOUT(
            [&] {
                auto *w = firstVisible("clipWave", false);
                if (!w || w->width() <= 1.0 || w->height() <= 1.0)
                    return false;
                const QImage img = window->grabWindow();
                if (img.isNull())
                    return false;
                const qreal d = img.devicePixelRatio();
                const QPointF origin = w->mapToScene(QPointF(0, 0));
                QSet<QRgb> colours;
                for (qreal fx = 0.1; fx < 0.9; fx += 0.02) {
                    const QPoint p = ((origin + QPointF(w->width() * fx, w->height() * 0.5)) * d).toPoint();
                    if (img.rect().contains(p))
                        colours.insert(img.pixel(p));
                }
                return colours.size() > 1;
            }(),
            "the waveform canvas painted nothing", 10000);
        const QImage waveShot = window->grabWindow();
        QVERIFY(!waveShot.isNull());
        const qreal waveDpr = waveShot.devicePixelRatio();
        const QPointF waveOrigin = wave->mapToScene(QPointF(0, 0));
        // Sample the middle band only: the clip name chip overlays the top of the canvas.
        QSet<QRgb> waveColours;
        for (qreal fx = 0.1; fx < 0.9; fx += 0.02) {
            const QPoint p =
                ((waveOrigin + QPointF(wave->width() * fx, wave->height() * 0.5)) * waveDpr).toPoint();
            if (waveShot.rect().contains(p))
                waveColours.insert(waveShot.pixel(p));
        }
        QVERIFY2(waveColours.size() > 1, "the waveform canvas painted nothing");
        flick->setProperty("contentX", scrolledX);
        QTRY_COMPARE(flick->property("contentX").toDouble(), scrolledX);
        // Previews arrive off the GUI thread and reset the timeline and bin models, so geometry
        // captured earlier belongs to delegates that no longer exist.
        dropV1 = findItem(window->contentItem(), "dropV1");
        card = findItem(window->contentItem(), "assetCard");
        QVERIFY(dropV1 && card);

        // File drops from outside are routed by geometry: a point over V1 maps to track 0 and a time,
        // a point outside the lanes falls back to plain import.
        auto *panel = findItem(window->contentItem(), "timelinePanel");
        QVERIFY(panel);
        // A point that is actually visible in the scrolled viewport: the timeline is scrolled to 400 px,
        // so viewport x 250 is content x 650.
        const qreal laneY = dropV1->mapToItem(flick, QPointF(0, dropV1->height() / 2)).y();
        const QPointF onV1 = flick->mapToScene(QPointF(250, laneY));
        const double atV1 = (flick->property("contentX").toDouble() + 250.0) / 60.0;
        QVariant target;
        QVERIFY(QMetaObject::invokeMethod(panel, "dropTargetFor", Q_RETURN_ARG(QVariant, target),
                                          Q_ARG(QVariant, onV1.x()), Q_ARG(QVariant, onV1.y())));
        QVERIFY2(target.canConvert<QVariantMap>(), "a point over V1 must resolve to a lane");
        QCOMPARE(target.toMap().value("track").toInt(), 0);
        QVERIFY(target.toMap().value("atSec").toDouble() >= 0.0);
        // Over the track-label column (outside the Flickable) must not claim a lane, even scrolled.
        QVariant overLabels;
        const QPointF labelsPoint = flick->mapToScene(QPointF(-40, laneY));
        QVERIFY(QMetaObject::invokeMethod(panel, "dropTargetFor", Q_RETURN_ARG(QVariant, overLabels),
                                          Q_ARG(QVariant, labelsPoint.x()), Q_ARG(QVariant, labelsPoint.y())));
        QVERIFY2(!overLabels.isValid() || overLabels.isNull() || !overLabels.toMap().value("track").isValid(),
                 "a drop over the track labels must not be placed on a lane");
        const QPointF offLanes(card->mapToScene(QPointF(10, 10)));
        QVariant outside;
        QVERIFY(QMetaObject::invokeMethod(panel, "dropTargetFor", Q_RETURN_ARG(QVariant, outside),
                                          Q_ARG(QVariant, offLanes.x()), Q_ARG(QVariant, offLanes.y())));
        QVERIFY2(!outside.isValid() || outside.isNull() || !outside.toMap().value("track").isValid(),
                 "a point outside the lanes must not claim a lane");

        // End to end: a real file drag over V1 goes through Main.qml's DropArea into the lane.
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(desktopFile)});
        const int beforeFileDrop = backend.clipsList().size();
        const QPoint dropPoint = onV1.toPoint();
        QDragEnterEvent enter(dropPoint, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QDragMoveEvent move(dropPoint, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QDropEvent drop(dropPoint, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &enter);
        QCoreApplication::sendEvent(window, &move);
        QCoreApplication::sendEvent(window, &drop);
        waitForImports(backend);
        QTRY_COMPARE(backend.clipsList().size(), beforeFileDrop + 1);
        const QVariantMap placed = backend.clipsList().last().toMap();
        QCOMPARE(placed.value("track").toInt(), 0);
        QVERIFY2(std::abs(placed.value("timelineStartSec").toDouble() - atV1) < 0.2,
                 qPrintable(QString("dropped at %1s, expected ~%2s")
                                .arg(placed.value("timelineStartSec").toDouble())
                                .arg(atV1)));
    }

    // Timeline editing through the real delegates: Ctrl+click multi-select, a group-preserving
    // click, a group drag that keeps spacing, lane header toggles, and the junction transition chip.
    void uiSmokeTimelineEditing() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString baseVid = uiSampleVideo("ui_base.mp4", "0x008080", 3.0);
        const QString pipVid = uiSampleVideo("ui_pip.mp4", "0xff0000", 3.0);
        QVERIFY(!baseVid.isEmpty() && !pipVid.isEmpty());

        UiHarness ui = UiHarness::create(dir.path());
        Backend &backend = *ui.backend;
        backend.importSingleMedia(baseVid);
        waitForImports(backend);
        const QString baseClipId = backend.selectedClipId();
        // A second clip from the same asset, butted to the first on V1, for the junction chip.
        const QString assetId = backend.assetsList().last().toMap().value("id").toString();
        backend.addAssetToTimeline(assetId, 0, 3.0);
        const QString droppedId = [&] {
            for (const QVariant &v : backend.clipsList()) {
                const QVariantMap m = v.toMap();
                if (m.value("track").toInt() == 0 && std::abs(m.value("timelineStartSec").toDouble() - 3.0) < 1e-6)
                    return m.value("id").toString();
            }
            return QString();
        }();
        QVERIFY(!droppedId.isEmpty());
        backend.importSingleMedia(pipVid);
        waitForImports(backend);
        // The overlay clip for the selection tests: on V2 at 0 s, starting together with V1.
        const QString pipClipId = backend.selectedClipId();
        backend.moveClip(pipClipId, 0.0, 1);

        ui.loadEngine();
        QVERIFY2(ui.engine->rootObjects().size() == 1,
                 qPrintable(ui.qmlErrors.isEmpty() ? QString("Main.qml did not load") : ui.qmlErrors.join("\n")));
        QVERIFY(ui.window);
        QVERIFY(QTest::qWaitForWindowExposed(ui.window));
        QQuickWindow *window = ui.window;

        // Multi-select through the real delegate: Ctrl+click adds, the outline follows.
        auto *flick = findItem(window->contentItem(), "timelineFlick");
        QVERIFY(flick);
        flick->setProperty("contentX", 0.0);
        QTRY_COMPARE(flick->property("contentX").toDouble(), 0.0);
        backend.selectClip(baseClipId);
        QCOMPARE(backend.selectedClipCount(), 1);
        QTRY_VERIFY2_WITH_TIMEOUT(
            [&] { return findClipBody(window->contentItem(), baseClipId) && findClipBody(window->contentItem(), pipClipId); }(),
            "both clip delegates must exist in the timeline", 10000);
        auto *baseBody = findClipBody(window->contentItem(), baseClipId);
        auto *pipBody = findClipBody(window->contentItem(), pipClipId);
        QVERIFY(baseBody && pipBody);
        QCOMPARE(baseBody->property("selected").toBool(), true);
        QCOMPARE(pipBody->property("selected").toBool(), false);

        const QPoint pipCenter = pipBody->mapToScene(QPointF(pipBody->width() / 2, pipBody->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::ControlModifier, pipCenter);
        QTRY_COMPARE(backend.selectedClipCount(), 2);
        QVERIFY(backend.selectedClipIds().contains(baseClipId));
        QVERIFY(backend.selectedClipIds().contains(pipClipId));
        QCOMPARE(backend.selectedClipId(), pipClipId);
        baseBody = findClipBody(window->contentItem(), baseClipId);
        pipBody = findClipBody(window->contentItem(), pipClipId);
        QVERIFY(baseBody && pipBody);
        QCOMPARE(baseBody->property("selected").toBool(), true);
        QCOMPARE(pipBody->property("selected").toBool(), true);

        // A plain click on one of the selected clips keeps the group instead of collapsing it.
        const QPoint baseCenter = baseBody->mapToScene(QPointF(baseBody->width() / 2, baseBody->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, baseCenter);
        QTRY_COMPARE(backend.selectedClipCount(), 2);

        // Drag one clip of the selection: the whole group must move by the same amount and keep
        // its spacing, which is the part no backend-only test can see.
        backend.setPlayheadSec(0.0);
        const QStringList idsBefore = backend.selectedClipIds();
        const double baseBefore = startOf(backend, baseClipId);
        const double pipBefore = startOf(backend, pipClipId);
        QVERIFY2(std::abs(baseBefore - pipBefore) < 1e-6, "the two clips start together");

        baseBody = findClipBody(window->contentItem(), baseClipId);
        QVERIFY(baseBody);
        const QPoint dragFrom = baseBody->mapToScene(QPointF(baseBody->width() / 2, baseBody->height() / 2)).toPoint();
        const QPoint dragTo = dragFrom + QPoint(qRound(2.0 * backend.zoomPxPerSec()), 0);
        QMouseEvent press(QEvent::MouseButtonPress, dragFrom, window->mapToGlobal(dragFrom),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &press);
        for (int step = 1; step <= 4; ++step) {
            const QPoint p = dragFrom + (dragTo - dragFrom) * step / 4;
            QMouseEvent move(QEvent::MouseMove, p, window->mapToGlobal(p), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &move);
        }
        QMouseEvent release(QEvent::MouseButtonRelease, dragTo, window->mapToGlobal(dragTo),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &release);
        QTRY_VERIFY2_WITH_TIMEOUT(startOf(backend, baseClipId) > baseBefore + 0.5,
                                  qPrintable(QString("the dragged clip did not move (start %1s)").arg(startOf(backend, baseClipId))), 5000);

        const double baseStartAfter = startOf(backend, baseClipId);
        const double pipStartAfter = startOf(backend, pipClipId);
        // The exact landing point depends on QML's drag threshold, so assert the property the
        // feature owns: both clips move, by the same amount, keeping their relative offset.
        QVERIFY2(pipStartAfter > pipBefore + 0.5,
                 qPrintable(QString("the other selected clip did not follow (start %1s)").arg(pipStartAfter)));
        QVERIFY2(std::abs((baseStartAfter - baseBefore) - (pipStartAfter - pipBefore)) < 1e-6,
                 qPrintable(QString("the group lost its spacing: %1s vs %2s")
                                .arg(baseStartAfter - baseBefore).arg(pipStartAfter - pipBefore)));
        QVERIFY2(std::abs(baseStartAfter - pipStartAfter) < 1e-6,
                 "both clips started together and must stay together");
        QVERIFY2(baseStartAfter < 4.0, "the group moved further than the drag");
        QCOMPARE(backend.selectedClipIds(), idsBefore);

        // Track-header controls act on the lane they are labelled with, not on a neighbouring one.
        auto *v2Mute = findItem(window->contentItem(), "laneToggle_V2Overlay_M");
        QVERIFY2(v2Mute, "the V2 row must expose a mute toggle");
        QVERIFY2(v2Mute->width() >= 24 && v2Mute->height() >= 24,
                 qPrintable(QString("lane toggle is only %1x%2").arg(v2Mute->width()).arg(v2Mute->height())));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          v2Mute->mapToScene(QPointF(v2Mute->width() / 2, v2Mute->height() / 2)).toPoint());
        QTRY_COMPARE(backend.lanesList().at(1).toMap().value("muted").toBool(), true);
        QCOMPARE(backend.lanesList().at(0).toMap().value("muted").toBool(), false);

        auto *t1Mute = findItem(window->contentItem(), "laneToggle_T1Text_M");
        QVERIFY2(t1Mute, "the text row is rendered");
        QVERIFY2(!t1Mute->isVisible(), "the text lane has no audio or lock controls to offer");
        backend.setLaneState(1, "muted", false);

        // The lane name and the M/S/L strip are two columns: the name must be shown in full and
        // must never run underneath the controls.
        for (const char *lane : {"V2Overlay", "V1Video", "A1Audio"}) {
            auto *label = findItem(window->contentItem(), QString("laneLabel_") + lane);
            auto *mute = findItem(window->contentItem(), QString("laneToggle_") + lane + "_M");
            QVERIFY2(label && mute, qPrintable(QString("%1 header parts missing").arg(lane)));
            const QList<QQuickItem *> parts = label->childItems();
            QVERIFY2(!parts.isEmpty(), qPrintable(QString("%1 header has no label text").arg(lane)));
            QVERIFY2(parts.first()->width() + 0.5 >= parts.first()->implicitWidth(),
                     qPrintable(QString("%1 lane name is squeezed to %2 of %3 px")
                                    .arg(lane).arg(parts.first()->width()).arg(parts.first()->implicitWidth())));
            const qreal labelRight = label->mapToScene(QPointF(label->width(), 0)).x();
            const qreal toggleLeft = mute->mapToScene(QPointF(0, 0)).x();
            QVERIFY2(labelRight <= toggleLeft,
                     qPrintable(QString("%1 header overlaps: label ends at %2, toggles start at %3")
                                    .arg(lane).arg(labelRight).arg(toggleLeft)));
        }

        // Junction control: the chip sits exactly where two clips touch, and only there.
        backend.moveClip(droppedId, endOf(backend, baseClipId), 0);
        QTRY_VERIFY2_WITH_TIMEOUT(findItem(window->contentItem(), "transitionChip_" + droppedId),
                                  "a transition chip must appear where two clips touch", 10000);
        auto *chip = findItem(window->contentItem(), "transitionChip_" + droppedId);
        QVERIFY2(chip, "a transition chip must appear where two clips touch");
        QVERIFY2(chip->width() >= 24 && chip->height() >= 24,
                 qPrintable(QString("transition chip is only %1x%2").arg(chip->width()).arg(chip->height())));
        auto *droppedBody = findClipBody(window->contentItem(), droppedId);
        QVERIFY(droppedBody);
        const qreal junctionX = droppedBody->mapToScene(QPointF(0, 0)).x();
        const qreal chipCenterX = chip->mapToScene(QPointF(chip->width() / 2, 0)).x();
        QVERIFY2(std::abs(chipCenterX - junctionX) < 2.0,
                 qPrintable(QString("chip at %1, junction at %2").arg(chipCenterX).arg(junctionX)));

        // The chip must actually carry the transition: open its menu and pick Dissolve.
        auto *chipMenu = chip->findChild<QObject *>("transitionMenu_" + droppedId);
        QVERIFY2(chipMenu, "the junction chip must own a transition menu");
        auto *dissolveOption = chipMenu->findChild<QObject *>("transitionOption_dissolve");
        QVERIFY2(dissolveOption, "the menu must offer Dissolve");
        QVERIFY(QMetaObject::invokeMethod(dissolveOption, "triggered"));
        auto transitionInOf = [&] {
            for (const QVariant &v : backend.clipsList())
                if (v.toMap().value("id").toString() == droppedId)
                    return v.toMap().value("transitionIn").toString();
            return QString();
        };
        QTRY_COMPARE_WITH_TIMEOUT(transitionInOf(), QString("dissolve"), 5000);

        backend.moveClip(droppedId, endOf(backend, baseClipId) + 3.0, 0);
        QTRY_VERIFY2_WITH_TIMEOUT(!findItem(window->contentItem(), "transitionChip_" + droppedId),
                                  "a gap must not offer a transition", 10000);
    }

    // Layout audits at several window sizes: top bar, transport, bin metadata, inspector
    // stacking and collapse, card hover swap, scroll affordances, and both dialogs.
    void uiSmokeLayoutAtSizes() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vid = uiSampleVideo("ui_base.mp4", "0x008080", 3.0);
        QVERIFY(!vid.isEmpty());

        UiHarness ui = UiHarness::loadMain(dir.path());
        QVERIFY2(ui.engine->rootObjects().size() == 1,
                 qPrintable(ui.qmlErrors.isEmpty() ? QString("Main.qml did not load") : ui.qmlErrors.join("\n")));
        QVERIFY(ui.window);
        QVERIFY(QTest::qWaitForWindowExposed(ui.window));
        Backend &backend = *ui.backend;
        QQuickWindow *window = ui.window;
        backend.importSingleMedia(vid);
        waitForImports(backend);
        backend.setZoomPxPerSec(60.0);
        const QString baseClipId = backend.clipsList().first().toMap().value("id").toString();
        backend.selectClip(baseClipId);
        QTRY_VERIFY2_WITH_TIMEOUT(findItem(window->contentItem(), "assetCard"),
                                  "the media bin must show the imported card", 10000);

        // The clip inspector stacks its collapsible sections without overlap and fits inside the scroll viewport.
        auto *inspectorColumn = findItem(window->contentItem(), "clipInspectorColumn");
        auto *inspectorScroll = findItem(window->contentItem(), "clipInspectorScroll");
        QTRY_VERIFY2_WITH_TIMEOUT(inspectorColumn && inspectorScroll,
                                  "the clip inspector must stack its sections in one column", 10000);
        QVERIFY(inspectorColumn && inspectorScroll);
        QTRY_VERIFY2_WITH_TIMEOUT(inspectorColumn->childItems().size() >= 6,
                                  qPrintable(QString("only %1 inspector sections").arg(inspectorColumn->childItems().size())), 10000);
        QList<QQuickItem *> sections = inspectorColumn->childItems();
        QVERIFY2(sections.size() >= 6, qPrintable(QString("only %1 inspector sections").arg(sections.size())));
        std::sort(sections.begin(), sections.end(), [](QQuickItem *a, QQuickItem *b) { return a->y() < b->y(); });
        for (int i = 1; i < sections.size(); ++i) {
            QVERIFY2(sections[i]->y() + 0.5 >= sections[i - 1]->y() + sections[i - 1]->height(),
                     qPrintable(QString("inspector sections %1 and %2 overlap at y=%3")
                                    .arg(i - 1).arg(i).arg(sections[i]->y())));
        }
        const QRectF scrollRect(inspectorScroll->mapToScene(QPointF(0, 0)),
                                QSizeF(inspectorScroll->width(), inspectorScroll->height()));
        for (QQuickItem *sec : sections) {
            const qreal secRight = sec->mapToScene(QPointF(sec->width(), 0)).x();
            QVERIFY2(secRight <= scrollRect.right() + 1.0,
                     qPrintable(QString("inspector section %1 clips right edge: %2 > %3")
                                    .arg(sec->objectName()).arg(secRight).arg(scrollRect.right())));
        }

        // Collapsing a section moves subsequent sections upward cleanly, and Reset restores Neutral look.
        auto *sectionLook = findItem(inspectorColumn, "sectionLook");
        auto *sectionTransform = findItem(inspectorColumn, "sectionTransform");
        auto *resetLooksBtn = findItem(inspectorColumn, "resetLooksButton");
        QVERIFY2(sectionLook && sectionTransform && resetLooksBtn, "look section, transform section and reset button must exist");
        QVERIFY2(resetLooksBtn->height() >= 24,
                 qPrintable(QString("reset action is only %1 px high").arg(resetLooksBtn->height())));
        backend.selectClip(baseClipId);
        backend.updateClipProperty(baseClipId, "saturation", 0.0);
        QTRY_VERIFY_WITH_TIMEOUT(backend.selectedClip().value("saturation").toDouble() < 0.1, 5000);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          resetLooksBtn->mapToScene(QPointF(resetLooksBtn->width() / 2, resetLooksBtn->height() / 2)).toPoint());
        QTRY_VERIFY2_WITH_TIMEOUT(std::abs(backend.selectedClip().value("saturation").toDouble() - 1.0) < 1e-6,
                                  "Reset must restore the Neutral look", 5000);
        const qreal transformYExpanded = sectionTransform->y();
        sectionLook->setProperty("expanded", false);
        QTRY_VERIFY2_WITH_TIMEOUT(sectionTransform->y() < transformYExpanded - 40.0,
                                  "collapsing Color & Effects must reclaim vertical space", 5000);
        QVERIFY2(sectionTransform->y() + 0.5 >= sectionLook->y() + sectionLook->height(), "collapsed section overlaps Transform");
        sectionLook->setProperty("expanded", true);
        QTest::qWait(30);

        // Media card: metadata and the action strip share one slot, so nothing floats over the
        // name on hover. The overlap only appears in the hover state, so force it.
        auto *binCard = findItem(window->contentItem(), "assetCard");
        auto *assetName = findItem(binCard, "assetName");
        auto *assetMeta = findItem(binCard, "assetMeta");
        auto *assetActions = findItem(binCard, "assetActions");
        QVERIFY2(binCard && assetName && assetMeta && assetActions, "the media card parts must exist");
        QVERIFY2(assetMeta->isVisible() && !assetActions->isVisible(),
                 "an idle card shows its metadata, not the actions");
        binCard->setProperty("hovered", true);
        QTRY_VERIFY2_WITH_TIMEOUT(!assetMeta->isVisible() && assetActions->isVisible(),
                                  "hovering swaps the metadata for the action strip", 5000);
        QVERIFY2(!(assetMeta->isVisible() && assetActions->isVisible()),
                 "the metadata and the actions must never be on screen together");
        const QRectF nameRect(assetName->mapToScene(QPointF(0, 0)),
                              QSizeF(assetName->width(), assetName->height()));
        const QRectF actionRect(assetActions->mapToScene(QPointF(0, 0)),
                                QSizeF(assetActions->width(), assetActions->height()));
        QVERIFY2(!nameRect.intersects(actionRect),
                 qPrintable(QString("the asset name overlaps the actions at %1,%2")
                                .arg(nameRect.x()).arg(nameRect.y())));
        const QRectF cardRect(binCard->mapToScene(QPointF(0, 0)),
                              QSizeF(binCard->width(), binCard->height()));
        QVERIFY2(cardRect.contains(actionRect), "the action strip must stay inside the card");
        QVERIFY2(assetName->width() + 0.5 >= assetName->implicitWidth(),
                 "the asset name must render without being squeezed");
        binCard->setProperty("hovered", false);

        // Layout audit helper: verifies top bar, preview transport, media bin metadata, timeline
        // scroll affordance, and inspector scroll bar at the current window size.
        auto auditViewportLayout = [&](const QString &label) {
            auto *topBar = findItem(window->contentItem(), "topBar");
            auto *titleField = findItem(topBar, "titleField");
            auto *topBarStatus = findItem(topBar, "topBarStatus");
            auto *shortcutsBtn = findItem(topBar, "shortcutsButton");
            auto *exportBtn = findItem(topBar, "exportButton");
            QVERIFY2(topBar && titleField && topBarStatus && shortcutsBtn && exportBtn, qPrintable(label + ": top bar items missing"));
            const QRectF topRect(topBar->mapToScene(QPointF(0, 0)), QSizeF(topBar->width(), topBar->height()));
            auto sceneRectOf = [](QQuickItem *it) {
                return QRectF(it->mapToScene(QPointF(0, 0)), QSizeF(it->width(), it->height()));
            };
            QVERIFY2(titleField->width() >= 120.0, qPrintable(QString("%1: titleField width %2 < 120").arg(label).arg(titleField->width())));
            for (QQuickItem *ctrl : {titleField, topBarStatus, shortcutsBtn, exportBtn}) {
                const QRectF r = sceneRectOf(ctrl);
                QVERIFY2(topRect.adjusted(-1, -1, 1, 1).contains(r),
                         qPrintable(QString("%1: top bar control %2 overflows (%3..%4 vs %5..%6)")
                                        .arg(label, ctrl->objectName()).arg(r.left()).arg(r.right()).arg(topRect.left()).arg(topRect.right())));
            }
            QVERIFY2(!sceneRectOf(titleField).intersects(sceneRectOf(topBarStatus)),
                     qPrintable(label + ": titleField overlaps topBarStatus"));
            QVERIFY2(!sceneRectOf(topBarStatus).intersects(sceneRectOf(shortcutsBtn)),
                     qPrintable(label + ": topBarStatus overlaps shortcutsButton"));
            QVERIFY2(!sceneRectOf(shortcutsBtn).intersects(sceneRectOf(exportBtn)),
                     qPrintable(label + ": shortcutsButton overlaps exportButton"));

            auto *transport = findItem(window->contentItem(), "previewTransport");
            auto *aspectCombo = findItem(transport, "aspectCombo");
            auto *playBtn = findItem(transport, "transportPlay");
            auto *playheadText = findItem(transport, "transportPlayhead");
            QVERIFY2(transport && aspectCombo && playBtn && playheadText, qPrintable(label + ": transport parts missing"));
            const QRectF trRect = sceneRectOf(transport);
            for (QQuickItem *ctrl : {playheadText, playBtn, aspectCombo}) {
                const QRectF r = sceneRectOf(ctrl);
                QVERIFY2(trRect.adjusted(-1, -1, 1, 1).contains(r),
                         qPrintable(QString("%1: transport control %2 clipped (%3..%4 vs %5..%6)")
                                        .arg(label, ctrl->objectName()).arg(r.left()).arg(r.right()).arg(trRect.left()).arg(trRect.right())));
            }
            QVERIFY2(!sceneRectOf(playBtn).intersects(sceneRectOf(aspectCombo)),
                     qPrintable(label + ": transportPlay overlaps aspectCombo"));

            auto *meta = findItem(window->contentItem(), "assetMeta");
            QVERIFY2(meta, qPrintable(label + ": assetMeta missing"));
            const QString metaText = meta->property("text").toString();
            QVERIFY2(!meta->property("truncated").toBool(),
                     qPrintable(QString("%1: assetMeta truncated ('%2')").arg(label, metaText)));
            QVERIFY2(metaText.contains("Video") && metaText.contains("0:03"),
                     qPrintable(QString("%1: assetMeta lost kind/duration ('%2')").arg(label, metaText)));

            QStringList btnProblems;
            int btnCount = 0;
            collectUnreadableButtons(window->contentItem(), &btnProblems, &btnCount);
            QVERIFY2(btnProblems.isEmpty(), qPrintable(label + ": " + btnProblems.join("\n")));
            QVERIFY(btnCount >= 12);
        };

        // Desktop-window audit (1280x800).
        window->resize(1280, 800);
        QTRY_COMPARE(window->size(), QSize(1280, 800));
        QTest::qWait(60); // one frame for the anchored layout to settle at the new size
        auditViewportLayout("1280x800");
        const QImage desktop = window->grabWindow();
        QVERIFY(!desktop.isNull());
        QVERIFY(desktop.save(dir.path() + "/cutcap_desktop.png"));

        // Compact-window audits (900x600, 880x580, 800x500).
        window->resize(900, 600);
        QTRY_COMPARE(window->size(), QSize(900, 600));
        QTest::qWait(60);
        auditViewportLayout("900x600");

        window->resize(880, 580);
        QTRY_COMPARE(window->size(), QSize(880, 580));
        QTest::qWait(60);
        auditViewportLayout("880x580");

        window->resize(800, 500);
        QTRY_COMPARE(window->size(), QSize(800, 500));
        QTest::qWait(60);
        auditViewportLayout("800x500");

        auto *timelineHScroll = findItem(window->contentItem(), "timelineHScroll");
        QVERIFY2(timelineHScroll, "timeline must have a horizontal scrollbar");
        QTRY_VERIFY2_WITH_TIMEOUT(timelineHScroll->property("size").toDouble() < 1.0 && timelineHScroll->isVisible(),
                                  "horizontal scrollbar must be visible when timeline clips extend past the viewport", 5000);

        auto *clipScrollBar = findItem(window->contentItem(), "clipInspectorScrollBar");
        QVERIFY2(clipScrollBar, "clip inspector must expose a vertical scrollbar");
        QTRY_VERIFY2_WITH_TIMEOUT(clipScrollBar->property("size").toDouble() < 1.0 && clipScrollBar->isVisible(),
                                  "inspector scrollbar must be visible when sections overflow a 600px window", 5000);

        const QImage compact = window->grabWindow();
        QVERIFY(!compact.isNull());
        QVERIFY(compact.save(dir.path() + "/cutcap_compact.png"));

        // Dialog audits: open ExportDialog and ShortcutsDialog, render, verify button labels and
        // containment inside the window.
        auto auditDialog = [&](QObject *dlg, const char *objName, const QString &shotPath) {
            QVERIFY2(dlg, qPrintable(QString("missing dialog %1").arg(objName)));
            QVERIFY(QMetaObject::invokeMethod(dlg, "open"));
            auto *dlgContent = dlg->property("contentItem").value<QQuickItem *>();
            auto *dlgBg = dlg->property("background").value<QQuickItem *>();
            QTRY_VERIFY2_WITH_TIMEOUT(dlgContent && dlgBg && dlgBg->isVisible(),
                                      qPrintable(QString("dialog %1 did not open").arg(objName)), 5000);
            const QRectF winRect(0, 0, window->width(), window->height());
            // The open animation moves the dialog background, so wait until it has settled.
            QTRY_VERIFY2_WITH_TIMEOUT(
                [&] {
                    const QRectF bg(dlgBg->mapToScene(QPointF(0, 0)), QSizeF(dlgBg->width(), dlgBg->height()));
                    return winRect.contains(bg.adjusted(1, 1, -1, -1));
                }(),
                qPrintable(QString("dialog %1 overflows the %2x%3 window")
                               .arg(objName).arg(window->width()).arg(window->height())), 5000);
            const QRectF bgRect(dlgBg->mapToScene(QPointF(0, 0)), QSizeF(dlgBg->width(), dlgBg->height()));
            QVERIFY2(winRect.contains(bgRect.adjusted(1, 1, -1, -1)),
                     qPrintable(QString("dialog %1 (%2x%3 at %4,%5) overflows %6x%7 window")
                                    .arg(objName).arg(bgRect.width()).arg(bgRect.height())
                                    .arg(bgRect.x()).arg(bgRect.y()).arg(window->width()).arg(window->height())));
            QStringList dlgBtnProblems;
            int dlgBtnCount = 0;
            collectUnreadableButtons(dlgContent, &dlgBtnProblems, &dlgBtnCount);
            QVERIFY2(dlgBtnProblems.isEmpty(), qPrintable(dlgBtnProblems.join("\n")));
            QVERIFY2(dlgBtnCount >= 1, qPrintable(QString("dialog %1 had no buttons inspected").arg(objName)));
            const QImage dlgImg = window->grabWindow();
            QVERIFY(!dlgImg.isNull() && dlgImg.save(shotPath));
        };

        QObject *exportDlg = window->findChild<QObject *>("exportDialog");
        auditDialog(exportDlg, "exportDialog", dir.path() + "/cutcap_export_dialog.png");
        auto *exportEst = exportDlg->findChild<QQuickItem *>("exportEstimateText");
        auto *exportResCombo = exportDlg->findChild<QObject *>("exportResCombo");
        auto *exportStartBtn = exportDlg->findChild<QQuickItem *>("exportStartButton");
        QVERIFY(exportEst && exportResCombo && exportStartBtn);
        QVERIFY(exportStartBtn->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(exportEst->property("text").toString().contains("1920×1080"), 5000);
        exportResCombo->setProperty("currentIndex", 1);
        QTRY_VERIFY2_WITH_TIMEOUT(exportEst->property("text").toString().contains("3840×2160"),
                                  qPrintable("resolution combo change did not update estimate: " + exportEst->property("text").toString()), 5000);
        QVERIFY(QMetaObject::invokeMethod(exportDlg, "close"));
        QTRY_VERIFY2_WITH_TIMEOUT(!exportDlg->property("opened").toBool(), "the export dialog did not close", 5000);

        QObject *shortcutsDlg = window->findChild<QObject *>("shortcutsDialog");
        auditDialog(shortcutsDlg, "shortcutsDialog", dir.path() + "/cutcap_shortcuts_dialog.png");
        QVERIFY(QMetaObject::invokeMethod(shortcutsDlg, "close"));
        QTRY_VERIFY2_WITH_TIMEOUT(!shortcutsDlg->property("opened").toBool(), "the shortcuts dialog did not close", 5000);
    }

    // ---- multi-select -------------------------------------------------------

    // Imports probe and prepare media off the GUI thread; drain the queue before asserting on it.
    static void waitForImports(Backend &b, int timeoutMs = 60000) {
        QTRY_VERIFY_WITH_TIMEOUT(b.importing() == 0, timeoutMs);
    }

    // Builds `count` two-second clips on one track at `firstStart + i * step`, in the order given,
    // and returns their ids. The asset is imported once so the ids are clips, not assets.
    static QStringList buildClips(Backend &b, const QString &videoPath, int track, int count,
                                  double firstStart, double step = 2.0) {
        b.importMediaAt({videoPath}, track, firstStart);
        waitForImports(b);
        const QString assetId = b.assetsList().last().toMap().value("id").toString();
        for (int i = 1; i < count; ++i)
            b.addAssetToTimeline(assetId, track, firstStart + i * step);
        QStringList ids;
        for (const QVariant &v : b.clipsList())
            ids.append(v.toMap().value("id").toString());
        Q_ASSERT(ids.size() == count);
        return ids;
    }

    static double startOf(Backend &b, const QString &clipId) {
        for (const QVariant &v : b.clipsList()) {
            if (v.toMap().value("id").toString() == clipId)
                return v.toMap().value("timelineStartSec").toDouble();
        }
        return -1.0;
    }

    static double endOf(Backend &b, const QString &clipId) {
        for (const QVariant &v : b.clipsList()) {
            if (v.toMap().value("id").toString() == clipId)
                return v.toMap().value("timelineStartSec").toDouble()
                       + v.toMap().value("durationSec").toDouble();
        }
        return -1.0;
    }

    // The desktop entry runs `cutcap open %F`; with no file selected, %F expands to nothing.
    // Treating that bare "open" as a filename put a "File not found: open" banner in the app.
    void launcherArgumentsOpenNoPhantomFile() {
        QCOMPARE(Cli::guiFiles({"cutcap"}), QStringList{});
        QCOMPARE(Cli::guiFiles({"cutcap", "open"}), QStringList{});
        QCOMPARE(Cli::guiFiles({"cutcap", "open", "/tmp/a.mp4"}), (QStringList{"/tmp/a.mp4"}));
        QCOMPARE(Cli::guiFiles({"cutcap", "open", "/tmp/a.mp4", "/tmp/b.cutcap"}),
                 (QStringList{"/tmp/a.mp4", "/tmp/b.cutcap"}));
        QCOMPARE(Cli::guiFiles({"cutcap", "/tmp/a.mp4"}), (QStringList{"/tmp/a.mp4"}));
        QCOMPARE(Cli::guiFiles({"cutcap", "open", "--help"}), QStringList{});
    }

    // Every headless command runs against a real project file, and the failure paths leave it alone.
    void cliCommandsWorkEndToEnd() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "cli.mp4", "0x774411", 2.0);
        QVERIFY(!vid.isEmpty());
        const QString proj = dir.path() + "/cli.cutcap";
        const QString out = dir.path() + "/cli.mp4.out.mp4";

        QCOMPARE(Cli::run({"cutcap", "new", proj, "--title", "CLI Edit"}), 0);
        QCOMPARE(Cli::run({"cutcap", "add-clip", proj, vid}), 0);
        bool ok = false;
        Project p = Project::loadFromFile(proj, &ok);
        QVERIFY(ok);
        QCOMPARE(p.title, QString("CLI Edit"));
        QCOMPARE(p.clips.size(), 1);

        QCOMPARE(Cli::run({"cutcap", "split", proj, p.clips.first().id, "1.0"}), 0);
        QCOMPARE(Cli::run({"cutcap", "add-text", proj, "--text", "From the CLI", "--start", "0", "--duration", "1"}), 0);
        p = Project::loadFromFile(proj, &ok);
        QVERIFY(ok);
        QCOMPARE(p.clips.size(), 2);
        QCOMPARE(p.texts.size(), 1);

        QCOMPARE(Cli::run({"cutcap", "export", proj, out, "--preset", "fast"}), 0);
        QVERIFY(QFileInfo::exists(out));
        QVERIFY(std::abs(probeDurationSec(out) - 2.0) < 0.3);

        // Error paths return 1 and change nothing.
        QCOMPARE(Cli::run({"cutcap", "frobnicate"}), 1);
        QCOMPARE(Cli::run({"cutcap", "probe"}), 1);
        QCOMPARE(Cli::run({"cutcap", "add-clip", proj, dir.path() + "/missing.mp4"}), 1);
        const QString png = dir.path() + "/still.png";
        QImage still(64, 36, QImage::Format_RGB32);
        still.fill(Qt::blue);
        QVERIFY(still.save(png));
        QCOMPARE(Cli::run({"cutcap", "add-clip", proj, png, "--track", "2"}), 1);
        p = Project::loadFromFile(proj, &ok);
        QVERIFY(ok);
        QCOMPARE(p.clips.size(), 2);
    }

    void transitionBetweenAdjacentClips() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "xf.mp4", "blue", 2.0);
        QVERIFY(!vid.isEmpty());
        Backend b;
        const QStringList ids = buildClips(b, vid, 0, 2, 0.0);
        QCOMPARE(b.clipsList().size(), 2);

        const QVariantList points = b.transitionPoints();
        QCOMPARE(points.size(), 1);
        QCOMPARE(points.first().toMap().value("firstId").toString(), ids[0]);
        QCOMPARE(points.first().toMap().value("secondId").toString(), ids[1]);
        QCOMPARE(points.first().toMap().value("kind").toString(), QString("none"));

        b.setTransitionBetween(ids[0], ids[1], "dissolve", 0.5);
        QVariantMap second = b.clipsList().at(1).toMap();
        QCOMPARE(second.value("transitionIn").toString(), QString("dissolve"));
        QVERIFY(std::abs(second.value("transitionDurationSec").toDouble() - 0.5) < 1e-6);
        QCOMPARE(b.transitionPoints().first().toMap().value("kind").toString(), QString("dissolve"));

        const QString joined = MediaEngine::buildExportArgs(b.project(), "/tmp/o.mp4", ExportOptions{}, {}).join(" ");
        QVERIFY2(joined.contains("xfade") && joined.contains("dissolve"),
                 "the export must chain the transition at the junction");

        // One undo removes the junction setting, one redo puts it back.
        b.undo();
        QCOMPARE(b.clipsList().at(1).toMap().value("transitionIn").toString(), QString("none"));
        b.redo();
        QCOMPARE(b.clipsList().at(1).toMap().value("transitionIn").toString(), QString("dissolve"));

        // Clips that do not touch are not a junction, so nothing can be set between them.
        b.moveClip(ids[1], 5.0, 0);
        QCOMPARE(b.transitionPoints().size(), 0);
        b.setTransitionBetween(ids[0], ids[1], "fade", 0.5);
        QCOMPARE(b.clipsList().at(1).toMap().value("transitionIn").toString(), QString("dissolve"));

        // A locked lane refuses the change.
        b.moveClip(ids[1], 2.0, 0);
        QCOMPARE(b.transitionPoints().size(), 1);
        b.setLaneState(0, "locked", true);
        b.setTransitionBetween(ids[0], ids[1], "wipeleft", 0.5);
        QCOMPARE(b.clipsList().at(1).toMap().value("transitionIn").toString(), QString("dissolve"));
        b.setLaneState(0, "locked", false);
    }

    void audioWithCoverArtLandsOnA1() {
        QTemporaryDir dir;
        const QString mp3 = dir.path() + "/cover.mp3";
        QProcess ff;
        ff.start("ffmpeg", {"-y", "-v", "error",
                            "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=44100:duration=3",
                            "-f", "lavfi", "-i", "color=c=0x334455:s=256x256:d=1",
                            "-map", "0:a", "-map", "1:v", "-c:a", "libmp3lame",
                            "-c:v", "mjpeg", "-disposition:v", "attached_pic", "-t", "3", mp3});
        QVERIFY(ff.waitForFinished(20000));
        QCOMPARE(ff.exitCode(), 0);

        // Cover art is reported by ffprobe as a video stream; it must not make this a video asset.
        const MediaAsset asset = MediaEngine::probeMedia(mp3);
        QCOMPARE(asset.kind, QString("audio"));
        QVERIFY(asset.hasAudio);

        Backend b;
        b.importSingleMedia(mp3);
        waitForImports(b);
        QCOMPARE(b.clipsList().size(), 1);
        QVariantMap clip = b.clipsList().first().toMap();
        QCOMPARE(clip.value("kind").toString(), QString("audio"));
        QCOMPARE(clip.value("track").toInt(), 2);

        // And it accepts an explicit placement on the audio lane.
        const QString assetId = b.assetsList().first().toMap().value("id").toString();
        QVERIFY(!assetId.isEmpty());
        b.addAssetToTimeline(assetId, 2, 4.0);
        QCOMPARE(b.clipsList().size(), 2);
        clip = b.clipsList().last().toMap();
        QCOMPARE(clip.value("track").toInt(), 2);
        QVERIFY(std::abs(clip.value("timelineStartSec").toDouble() - 4.0) < 0.2);
    }

    void multiSelectAdditiveAndRange() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "multi.mp4", "blue", 2.0);
        QVERIFY(!vid.isEmpty());
        Backend b;
        const QStringList ids = buildClips(b, vid, 0, 4, 0.0);
        QCOMPARE(b.clipsList().size(), 4);
        QCOMPARE(b.selectedClipCount(), 1);
        QCOMPARE(b.selectedClipId(), ids.last());

        b.selectClip(ids[0]);
        b.selectClipAdditive(ids[1]);
        QCOMPARE(b.selectedClipCount(), 2);
        QCOMPARE(b.selectedClipIds().last(), ids[1]);
        QCOMPARE(b.selectedClipId(), ids[1]);
        QVERIFY(b.selectedClipIds().contains(ids[0]) && b.selectedClipIds().contains(ids[1]));

        b.selectClipAdditive(ids[1]);
        QCOMPARE(b.selectedClipCount(), 1);
        QCOMPARE(b.selectedClipId(), ids[0]);

        b.selectClip(ids[0]);
        b.selectClipRange(ids[2]);
        QCOMPARE(b.selectedClipCount(), 3);
        QCOMPARE(b.selectedClipIds(), (QStringList{ids[0], ids[1], ids[2]}));
        QCOMPARE(b.selectedClipId(), ids[2]);

        // A range from a different lane replaces the selection instead of spanning lanes.
        b.addAssetToTimeline(b.assetsList().last().toMap().value("id").toString(), 1, 3.0);
        const QString overlayId = b.clipsList().last().toMap().value("id").toString();
        b.selectClip(ids[2]);
        b.selectClipRange(overlayId);
        QCOMPARE(b.selectedClipCount(), 1);
        QCOMPARE(b.selectedClipId(), overlayId);

        b.selectAllClips();
        QCOMPARE(b.selectedClipCount(), b.clipsList().size());
        QVERIFY(b.selectedClipCount() >= 5);

        b.clearClipSelection();
        QCOMPARE(b.selectedClipCount(), 0);
        QVERIFY(b.selectedClipId().isEmpty());
    }

    void multiSelectDeleteIsOneUndoStep() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "del.mp4", "green", 2.0);
        QVERIFY(!vid.isEmpty());
        Backend b;
        const QStringList ids = buildClips(b, vid, 0, 4, 0.0);
        QCOMPARE(b.clipsList().size(), 4);

        b.selectClip(ids[0]);
        b.selectClipAdditive(ids[1]);
        b.selectClipAdditive(ids[2]);
        QCOMPARE(b.selectedClipCount(), 3);
        b.deleteSelected(false);

        QCOMPARE(b.clipsList().size(), 1);
        QCOMPARE(b.clipsList().first().toMap().value("id").toString(), ids[3]);
        QCOMPARE(b.selectedClipCount(), 0);
        QVERIFY(b.canUndo());

        b.undo();
        QCOMPARE(b.clipsList().size(), 4);
    }

    void multiSelectMovePreservesOffsetsAndClamps() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "move.mp4", "red", 2.0);
        QVERIFY(!vid.isEmpty());
        const QString wav = dir.path() + "/move-tone.wav";
        {
            QProcess ff;
            ff.start("ffmpeg", {"-y", "-v", "error", "-f", "lavfi", "-i",
                                "sine=frequency=440:sample_rate=44100:duration=2", wav});
            QVERIFY(ff.waitForFinished(15000));
            QCOMPARE(ff.exitCode(), 0);
        }

        Backend b;
        const QStringList ids = buildClips(b, vid, 0, 3, 1.0);
        b.importMediaAt({wav}, 2, 10.0);
        waitForImports(b);
        QString audioId;
        for (const QVariant &v : b.clipsList()) {
            if (v.toMap().value("track").toInt() == 2)
                audioId = v.toMap().value("id").toString();
        }
        QVERIFY2(!audioId.isEmpty(), "the audio asset must land on A1");

        b.selectAllClips();
        QCOMPARE(b.selectedClipCount(), 4);
        QVERIFY(b.moveSelectedClips(2.0, 0));
        QVERIFY(std::abs(startOf(b, ids[0]) - 3.0) < 1e-6);
        QVERIFY(std::abs(startOf(b, ids[1]) - 5.0) < 1e-6);
        QVERIFY(std::abs(startOf(b, ids[2]) - 7.0) < 1e-6);
        QVERIFY(std::abs(startOf(b, audioId) - 12.0) < 1e-6);

        // A delta that would push the first clip negative moves the group to zero and no further.
        QVERIFY(b.moveSelectedClips(-100.0, 0));
        QVERIFY(std::abs(startOf(b, ids[0])) < 1e-6);
        QVERIFY(std::abs(startOf(b, ids[1]) - 2.0) < 1e-6);
        QVERIFY(std::abs(startOf(b, ids[2]) - 4.0) < 1e-6);

        b.undo();
        b.undo();
        QVERIFY(std::abs(startOf(b, ids[0]) - 1.0) < 1e-6);
        QVERIFY(std::abs(startOf(b, ids[1]) - 3.0) < 1e-6);
        QVERIFY(std::abs(startOf(b, ids[2]) - 5.0) < 1e-6);

        // A locked clip in the selection refuses the whole move.
        b.setLaneState(0, "locked", true);
        b.selectAllClips();
        QCOMPARE(b.selectedClipCount(), 4);
        QVERIFY(!b.moveSelectedClips(1.0, 0));
        QVERIFY(std::abs(startOf(b, ids[0]) - 1.0) < 1e-6);
        b.setLaneState(0, "locked", false);

        // Audio clips never change track, so a track delta is dropped for the whole group.
        b.selectAllClips();
        QVERIFY(b.moveSelectedClips(0.0, 1));
        for (const QVariant &v : b.clipsList()) {
            const QVariantMap c = v.toMap();
            if (c.value("kind").toString() == "audio")
                QCOMPARE(c.value("track").toInt(), 2);
            else
                QCOMPARE(c.value("track").toInt(), 0);
        }
    }

    void multiSelectRippleDeleteShiftsPerLane() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "rip.mp4", "yellow", 2.0);
        QVERIFY(!vid.isEmpty());
        const QString wav = dir.path() + "/tone.wav";
        {
            QProcess ff;
            ff.start("ffmpeg", {"-y", "-v", "error", "-f", "lavfi", "-i",
                                "sine=frequency=440:sample_rate=44100:duration=2", wav});
            QVERIFY(ff.waitForFinished(15000));
            QCOMPARE(ff.exitCode(), 0);
        }

        Backend b;
        const QStringList ids = buildClips(b, vid, 0, 3, 0.0);
        QCOMPARE(b.clipsList().size(), 3);
        b.importMediaAt({wav}, 2, 10.0);
        waitForImports(b);
        QString audioId;
        for (const QVariant &v : b.clipsList()) {
            if (v.toMap().value("track").toInt() == 2)
                audioId = v.toMap().value("id").toString();
        }
        QVERIFY2(!audioId.isEmpty(), "the audio asset must land on A1");
        QCOMPARE(b.selectedClipCount(), 1);

        b.selectClip(ids[0]);
        b.selectClipAdditive(ids[1]);
        QCOMPARE(b.selectedClipCount(), 2);
        b.deleteSelected(true);

        QCOMPARE(b.clipsList().size(), 2);
        QVERIFY(std::abs(startOf(b, ids[2])) < 1e-6);
        QVERIFY2(std::abs(startOf(b, audioId) - 10.0) < 1e-6,
                 "a ripple on V1 must not move the audio lane");
    }

    void testRejectsInjectedChromaColor() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/evil.cutcap";
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"version":1,"clips":[{"id":"clip_evil","sourcePath":"/tmp/demo.mp4","kind":"video",)"
                R"("durationSec":4.0,"bgRemovalMode":"chromakey",)"
                R"("chromaColor":"00ff00,metadata=mode=print:file=/home/arch/x"}]})");
        f.close();

        bool ok = false;
        const Project p = Project::loadFromFile(path, &ok);
        QVERIFY(ok);
        QCOMPARE(p.clips.size(), 1);
        QCOMPARE(p.clips[0].chromaColor, QString("#00ff00"));
        QCOMPARE(sanitizedChromaColor("00ff00,metadata=mode=print:file=/home/arch/x"), QString("#00ff00"));
        QCOMPARE(sanitizedChromaColor("00FF00"), QString("#00ff00"));
        QCOMPARE(sanitizedChromaColor("#123abc"), QString("#123abc"));

        const QStringList args = MediaEngine::buildExportArgs(p, "/tmp/out.mp4", ExportOptions{}, {});
        const QString joined = args.join(" ");
        QVERIFY2(joined.contains("chromakey=0x00ff00"), qPrintable(joined));
        QVERIFY2(!joined.contains("metadata="), qPrintable(joined));
    }

    void testFromJsonClampsRanges() {
        QJsonObject clip{
            {"id", "c1"},
            {"track", 7},
            {"speed", 0},
            {"durationSec", 0},
            {"sourceDurationSec", -4},
            {"volume", -1},
            {"brightness", 9},
            {"contrast", -2},
            {"saturation", 5},
            {"blur", -3},
            {"chromaSimilarity", 99},
            {"chromaBlend", 4},
            {"keyframes", QJsonArray{QJsonObject{{"timeSec", -2}, {"opacity", 7}}}},
        };
        QJsonObject obj{{"width", 0}, {"height", 9001}, {"fps", 0}, {"clips", QJsonArray{clip}}};

        const Project p = Project::fromJson(obj);
        QCOMPARE(p.width, 16);
        QCOMPARE(p.height, 8192);
        QCOMPARE(p.fps, 1.0);
        QCOMPARE(p.clips.size(), 1);
        const TimelineClip &c = p.clips[0];
        QCOMPARE(c.track, 2);
        QCOMPARE(c.speed, 0.25);
        QCOMPARE(c.durationSec, 0.1);
        QCOMPARE(c.sourceDurationSec, 0.1);
        QCOMPARE(c.volume, 0.0);
        QCOMPARE(c.brightness, 0.5);
        QCOMPARE(c.contrast, 0.5);
        QCOMPARE(c.saturation, 2.0);
        QCOMPARE(c.blur, 0.0);
        QCOMPARE(c.chromaSimilarity, 0.6);
        QCOMPARE(c.chromaBlend, 0.5);
        QCOMPARE(c.keyframes.size(), 1);
        QCOMPARE(c.keyframes[0].timeSec, 0.0);
        QCOMPARE(c.keyframes[0].opacity, 1.0);
    }

    // A project file is untrusted: a media path that is not an absolute local path would reach
    // ffmpeg as a network input the moment the project opens.
    void testProjectPathsMustBeLocalFiles() {
        const QString host = QStringLiteral("127.0.0.1:9");
        const QStringList hostile{QStringLiteral("http://") + host + "/a.mp4",
                                  QStringLiteral("https://") + host + "/a.mp4",
                                  QStringLiteral("tcp://") + host,
                                  QStringLiteral("concat:/a.mp4|/b.mp4"),
                                  QStringLiteral("relative/a.mp4")};
        for (const QString &bad : hostile) {
            const MediaAsset a = MediaAsset::fromJson(QJsonObject{
                {"id", "a1"}, {"path", bad}, {"thumbnailPath", bad}, {"filmstripPath", bad}});
            QVERIFY2(a.path.isEmpty(), qPrintable(bad));
            QVERIFY2(a.thumbnailPath.isEmpty(), qPrintable(bad));
            QVERIFY2(a.filmstripPath.isEmpty(), qPrintable(bad));
            const TimelineClip c = TimelineClip::fromJson(QJsonObject{
                {"id", "c1"}, {"sourcePath", bad}, {"processedPath", bad}});
            QVERIFY2(c.sourcePath.isEmpty(), qPrintable(bad));
            QVERIFY2(c.processedPath.isEmpty(), qPrintable(bad));
        }
        // Positive control: a real absolute path survives unchanged.
        const QString good = QStringLiteral("/tmp/clip one.mp4");
        const MediaAsset a = MediaAsset::fromJson(QJsonObject{
            {"id", "a1"}, {"path", good}, {"thumbnailPath", good}, {"filmstripPath", good}});
        QCOMPARE(a.path, good);
        QCOMPARE(a.thumbnailPath, good);
        QCOMPARE(a.filmstripPath, good);
        const TimelineClip c = TimelineClip::fromJson(QJsonObject{
            {"id", "c1"}, {"sourcePath", good}, {"processedPath", good}});
        QCOMPARE(c.sourcePath, good);
        QCOMPARE(c.processedPath, good);
    }

    // A literal backslash typed in a title must not turn into an ASS control code on export.
    void testAssEscapesBackslash() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Project p;
        TextOverlay t;
        t.text = "C:\\new folder";
        t.startSec = 0.0;
        t.durationSec = 2.0;
        p.texts.append(t);

        const QString ass = MediaEngine::generateAssSubtitles(p, dir.path() + "/subs.ass", 1920, 1080);
        QVERIFY(!ass.isEmpty());
        QFile f(ass);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString content = QString::fromUtf8(f.readAll());
        QString dialogue;
        for (const QString &line : content.split('\n')) {
            if (line.startsWith("Dialogue:"))
                dialogue = line;
        }
        QVERIFY2(!dialogue.isEmpty(), "no dialogue line was written");
        // libass has no escape for a backslash; a word joiner after it stops "\n", "\N" and "\h"
        // from being read as line breaks and hard spaces.
        QVERIFY2(dialogue.contains(QString("C:\\") + QChar(0x2060) + "new folder"), qPrintable(dialogue));
        QVERIFY2(!dialogue.contains("\\\\"), qPrintable(dialogue));
    }

    // Two parses must never hand out the same caption id, or editing one caption edits another.
    void testCaptionIdsAreUnique() {
        const QString srt = "1\n00:00:00,000 --> 00:00:01,000\nHello\n\n2\n00:00:01,000 --> 00:00:02,000\nWorld\n";
        QSet<QString> ids;
        for (int pass = 0; pass < 3; ++pass) {
            const auto items = MediaEngine::parseSrt(srt, true);
            QCOMPARE(items.size(), 2);
            for (const auto &t : items)
                ids.insert(t.id);
        }
        QCOMPARE(ids.size(), 6);
    }

    // Cue times come back relative to the part of the source the clip plays; they are placed on
    // the timeline through the clip's start and speed, and anything past the clip's end is dropped.
    void testCaptionsFollowTrimAndSpeed() {
        TimelineClip clip;
        clip.timelineStartSec = 4.0;
        clip.durationSec = 3.0;   // on the timeline
        clip.sourceInSec = 20.0;
        clip.speed = 2.0;         // plays 6 s of source
        QList<TextOverlay> cues;
        TextOverlay a; a.id = "a"; a.startSec = 1.0; a.durationSec = 2.0; cues.append(a);
        TextOverlay b; b.id = "b"; b.startSec = 5.0; b.durationSec = 4.0; cues.append(b);
        TextOverlay c; c.id = "c"; c.startSec = 6.5; c.durationSec = 1.0; cues.append(c);

        const auto placed = MediaEngine::captionsForClip(cues, clip);
        QCOMPARE(placed.size(), 2);
        QCOMPARE(placed[0].id, QString("a"));
        QVERIFY(std::abs(placed[0].startSec - 4.5) < 1e-6);
        QVERIFY(std::abs(placed[0].durationSec - 1.0) < 1e-6);
        // "b" starts inside the clip and is cut at the clip's end (timeline 7.0).
        QVERIFY(std::abs(placed[1].startSec - 6.5) < 1e-6);
        QVERIFY(std::abs(placed[1].durationSec - 0.5) < 1e-6);
    }

    // Auto-captions transcribe only the part of the source the clip plays, and land where it plays.
    void testAutoCaptionsUseTheClipSpan() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "span.mp4", "teal", 6.0);
        QVERIFY(!vid.isEmpty());

        Backend b;
        b.importSingleMedia(vid);
        waitForImports(b);
        QCOMPARE(b.clipsList().size(), 1);
        const QString clipId = b.clipsList().first().toMap().value("id").toString();

        // Trim: the clip now starts 2 s into its source, sits at timeline 0 and lasts 3 s.
        b.trimClip(clipId, 0.0, 3.0, 2.0);
        b.moveClip(clipId, 4.0, 0);
        const QVariantMap clip = b.clipsList().first().toMap();
        QCOMPARE(clip.value("sourceInSec").toDouble(), 2.0);
        QCOMPARE(clip.value("timelineStartSec").toDouble(), 4.0);

        // A whisper-cli that copies the audio it was given and writes a cue 1 s into that audio.
        const QString bin = dir.path() + "/faketools";
        const QString copied = dir.path() + "/whisper-input.wav";
        QVERIFY(!makeTool(bin, "whisper-cli", "#!/bin/sh\nout=\nin=\n"
                                              "while [ $# -gt 0 ]; do\n"
                                              "  if [ \"$1\" = \"-of\" ]; then out=\"$2\"; fi\n"
                                              "  if [ \"$1\" = \"-f\" ]; then in=\"$2\"; fi\n"
                                              "  shift\n"
                                              "done\n"
                                              "cp \"$in\" '" + copied.toUtf8() + "'\n"
                                              "printf '1\\n00:00:01,000 --> 00:00:02,000\\nSpoken\\n\\n' > \"$out.srt\"\n"
                                              "").isEmpty());
        const QByteArray oldPath = qgetenv("PATH");
        PathRestore restore{oldPath};
        qputenv("PATH", bin.toUtf8() + ":" + oldPath);

        b.generateAutoCaptions(clipId, "en");
        QTRY_VERIFY_WITH_TIMEOUT(!b.busy(), 30000);
        QCOMPARE(b.textsList().size(), 1);
        const QVariantMap cap = b.textsList().first().toMap();
        // The cue is 1 s into the transcribed span, so it belongs 1 s after the clip's start.
        QVERIFY2(std::abs(cap.value("startSec").toDouble() - 5.0) < 0.01, qPrintable(cap.value("startSec").toString()));
        // Only the 3 s the clip plays was transcribed, not the whole 6 s file.
        QVERIFY2(QFileInfo::exists(copied), "whisper-cli never received the audio");
        const double d = probeDurationSec(copied);
        QVERIFY2(std::abs(d - 3.0) < 0.3, qPrintable(QString::number(d)));
    }

    // Only regenerable previews are swept; recordings survive, old previews go, kept and recent stay.
    void testCachePrune() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QDateTime monthAgo = QDateTime::currentDateTime().addDays(-30);
        auto makeFile = [&](const QString &path, bool old) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
            f.close();
            if (old) {
                QFile g(path);
                QVERIFY(g.open(QIODevice::ReadWrite));
                QVERIFY(g.setFileTime(monthAgo, QFileDevice::FileModificationTime));
                g.close();
            }
        };
        const QString voOld = dir.path() + "/vo_old.wav";
        const QString ttsOld = dir.path() + "/tts_old.wav";
        const QString nobgOld = dir.path() + "/nobg_old.png";
        const QString thumbOld = dir.path() + "/thumb_old.jpg";
        const QString stripOld = dir.path() + "/strip_old.jpg";
        const QString subsOld = dir.path() + "/subs_old.ass";
        const QString thumbRecent = dir.path() + "/thumb_recent.jpg";
        const QString thumbKept = dir.path() + "/thumb_keep.jpg";
        for (const QString &path : {voOld, ttsOld, nobgOld, thumbOld, stripOld, subsOld, thumbKept})
            makeFile(path, true);
        makeFile(thumbRecent, false);

        QCOMPARE(MediaEngine::pruneCache(dir.path(), 7, {thumbKept}), 3);
        QVERIFY(QFileInfo::exists(voOld));
        QVERIFY(QFileInfo::exists(ttsOld));
        QVERIFY(QFileInfo::exists(nobgOld));
        QVERIFY(!QFileInfo::exists(thumbOld));
        QVERIFY(!QFileInfo::exists(stripOld));
        QVERIFY(!QFileInfo::exists(subsOld));
        QVERIFY(QFileInfo::exists(thumbRecent));
        QVERIFY(QFileInfo::exists(thumbKept));
        QVERIFY(MediaEngine::mediaDir() != MediaEngine::cacheDir());
    }

    void splitAtPlayheadSplitsWholeSelection() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "split.mp4", "white", 3.0);
        QVERIFY(!vid.isEmpty());
        Backend b;
        b.importMediaAt({vid}, 0, 0.0);
        waitForImports(b);
        const QString assetId = b.assetsList().last().toMap().value("id").toString();
        b.addAssetToTimeline(assetId, 1, 0.0);
        QCOMPARE(b.clipsList().size(), 2);

        b.selectAllClips();
        QCOMPARE(b.selectedClipCount(), 2);
        b.setPlayheadSec(1.5);
        b.splitAtPlayhead();

        QCOMPARE(b.clipsList().size(), 4);
        QCOMPARE(b.selectedClipCount(), 2);
        int rightHalves = 0;
        for (const QVariant &v : b.clipsList()) {
            if (std::abs(v.toMap().value("timelineStartSec").toDouble() - 1.5) < 1e-6)
                ++rightHalves;
        }
        QCOMPARE(rightHalves, 2);

        b.undo();
        QCOMPARE(b.clipsList().size(), 2);
    }

    // espeak reads any argument starting with "-" as an option, so TTS text like "-rf /etc/passwd"
    // must come after a "--" separator. The separator has to sit directly before the text argument.
    void testEspeakTtsArgsEndOptionsBeforeText() {
        const QStringList args = MediaEngine::buildEspeakTtsArgs("/tmp/out.wav", "-rf /etc/passwd");
        const QStringList expected{"-w", "/tmp/out.wav", "--", "-rf /etc/passwd"};
        QVERIFY2(args == expected, qPrintable(args.join(' ')));
        QCOMPARE(args.indexOf("--"), 2);
        QVERIFY(args.last().startsWith("-"));

        // Plain text is also spoken after "--", at the same position.
        const QStringList plain = MediaEngine::buildEspeakTtsArgs("/tmp/out.wav", "hello world");
        QCOMPARE(plain.indexOf("--"), 2);
        QCOMPARE(plain.last(), QString("hello world"));
    }

    // A failing import must not move the user's last clip onto the audio lane, and must not
    // replace the import error with a success status.
    void testTtsImportFailureLeavesTimeline() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vid = makeSampleVideo(dir.path(), "existing.mp4", "red", 1.0);
        QVERIFY(!vid.isEmpty());

        Backend b;
        b.importMediaAt({vid}, 0, 0.0);
        waitForImports(b);
        QCOMPARE(b.clipsList().size(), 1);
        const QString clipId = b.clipsList().first().toMap().value("id").toString();
        QCOMPARE(b.clipsList().first().toMap().value("track").toInt(), 0);

        // A TTS engine that writes the requested output, and an ffprobe that fails, so the
        // synthesis succeeds and only the import of the result fails.
        const QString bin = dir.path() + "/faketools";
        QVERIFY(!makeTool(bin, "piper", "#!/bin/sh\nout=\nwhile [ $# -gt 0 ]; do\n"
                                        "  if [ \"$1\" = \"--output_file\" ]; then out=\"$2\"; fi\n"
                                        "  shift\ndone\ncat >/dev/null\n: > \"$out\"\n").isEmpty());
        QVERIFY(!makeTool(bin, "ffprobe", "#!/bin/sh\nexit 1\n").isEmpty());

        const QByteArray oldPath = qgetenv("PATH");
        PathRestore restore{oldPath};
        qputenv("PATH", bin.toUtf8() + ":" + oldPath);

        b.generateLocalTts("hello from the test");
        QTRY_VERIFY_WITH_TIMEOUT(b.statusMessage() != "Synthesizing local TTS...", 20000);

        QCOMPARE(b.clipsList().size(), 1);
        const QVariantMap clip = b.clipsList().first().toMap();
        QCOMPARE(clip.value("id").toString(), clipId);
        QCOMPARE(clip.value("track").toInt(), 0);
        QCOMPARE(clip.value("timelineStartSec").toDouble(), 0.0);
        QVERIFY2(b.statusMessage().contains("ffprobe"), qPrintable(b.statusMessage()));
    }

    void testUnsavedChangesGuard() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vid = makeSampleVideo(dir.path(), "dirty.mp4", "0x336699", 1.0);
        QVERIFY(!vid.isEmpty());

        QQuickStyle::setStyle("Material");
        Theme theme(dir.path());
        Design design(theme);
        Backend backend;
        // Register the module's types before injecting instances: a singleton instance seals the
        // module version, and the module's own registration would then be dropped.
        extern void qml_register_types_CutCap();
        qml_register_types_CutCap();
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Theme", &theme);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Design", &design);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Backend", &backend);
        QStringList qmlErrors;
        g_qmlErrors = &qmlErrors;
        g_previousHandler = qInstallMessageHandler(captureQmlErrors);
        QQmlApplicationEngine engine;
        engine.load(QUrl("qrc:/qml/Main.qml"));
        qInstallMessageHandler(g_previousHandler);
        g_qmlErrors = nullptr;
        QVERIFY2(engine.rootObjects().size() == 1, qPrintable(qmlErrors.join("\n")));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        backend.importSingleMedia(vid);
        waitForImports(backend);
        QVERIFY(backend.dirty());

        window->close();
        QTest::qWait(300);
        QVERIFY2(window->isVisible(), "closing a project with unsaved edits must be refused");
        auto *dialog = window->findChild<QObject *>("discardDialog");
        QVERIFY2(dialog, "the unsaved-changes dialog does not exist");
        QVERIFY2(dialog->property("opened").toBool(), "the unsaved-changes dialog did not open");

        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTest::qWait(100);
        QVERIFY(window->isVisible());
        QVERIFY(backend.dirty());

        // Saving clears the edits, so the next close needs no prompt.
        QVERIFY(backend.saveProject(dir.path() + "/guard.cutcap"));
        QVERIFY(!backend.dirty());
        window->close();
        QTest::qWait(100);
        QVERIFY2(!window->isVisible(), "a saved project must close without a prompt");

        // Discard is the other exit: it closes without saving the edits.
        window->show();
        QTest::qWait(200);
        QVERIFY(window->isVisible());
        backend.importSingleMedia(vid);
        waitForImports(backend);
        QVERIFY(backend.dirty());
        window->close();
        QTest::qWait(300);
        QVERIFY(window->isVisible());
        auto *discardButton = window->findChild<QObject *>("discardDiscardButton");
        QVERIFY2(discardButton, "the Discard button does not exist");
        QVERIFY(QMetaObject::invokeMethod(discardButton, "clicked"));
        QTest::qWait(200);
        QVERIFY2(!window->isVisible(), "Discard must close the window");
        QVERIFY2(backend.dirty(), "Discard must not save the project");
    }

    void oneClipListBuildPerEdit() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vid = makeSampleVideo(dir.path(), "builds.mp4", "0x336699", 2.0);
        QVERIFY(!vid.isEmpty());

        QQuickStyle::setStyle("Material");
        Theme theme(dir.path());
        Design design(theme);
        Backend backend;
        extern void qml_register_types_CutCap();
        qml_register_types_CutCap();
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Theme", &theme);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Design", &design);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Backend", &backend);
        QStringList qmlErrors;
        g_qmlErrors = &qmlErrors;
        g_previousHandler = qInstallMessageHandler(captureQmlErrors);
        QQmlApplicationEngine engine;
        engine.load(QUrl("qrc:/qml/Main.qml"));
        qInstallMessageHandler(g_previousHandler);
        g_qmlErrors = nullptr;
        QVERIFY2(engine.rootObjects().size() == 1, qPrintable(qmlErrors.join("\n")));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        backend.importSingleMedia(vid);
        waitForImports(backend);
        QTest::qWait(300);
        QCOMPARE(backend.clipCount(), 1);
        const QString clipId = backend.selectedClipId();
        QVERIFY(!clipId.isEmpty());

        const int before = backend.clipsListBuilds();
        backend.updateClipProperty(clipId, "brightness", 0.2);
        QTest::qWait(200);
        const int builds = backend.clipsListBuilds() - before;
        QVERIFY2(builds >= 1, "the timeline did not refresh after the edit");
        QVERIFY2(builds <= 2, qPrintable(QString("one edit rebuilt the clip list %1 times").arg(builds)));
    }

    // One slider tick (a single clip-property edit) must reach the timeline as dataChanged for the
    // changed row only: the lane delegates are updated in place, not destroyed and recreated the
    // way they were when every timelineChanged re-read a QVariantList clipsList.
    void oneEditCreatesNoNewDelegates() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vid = makeSampleVideo(dir.path(), "delegates.mp4", "0x336699", 2.0);
        QVERIFY(!vid.isEmpty());

        UiHarness ui = UiHarness::loadMain(dir.path());
        QVERIFY2(ui.engine->rootObjects().size() == 1,
                 qPrintable(ui.qmlErrors.isEmpty() ? QString("Main.qml did not load") : ui.qmlErrors.join("\n")));
        QVERIFY(ui.window);
        QVERIFY(QTest::qWaitForWindowExposed(ui.window));
        Backend &backend = *ui.backend;
        QQuickWindow *window = ui.window;

        backend.importSingleMedia(vid);
        waitForImports(backend);
        // A second clip on the same lane, so "all delegates recreated" is visible, not just one.
        const QString assetId = backend.assetsList().last().toMap().value("id").toString();
        backend.addAssetToTimeline(assetId, 0, 2.0);
        QCOMPARE(backend.clipCount(), 2);
        QString firstId, secondId;
        for (const QVariant &v : backend.clipsList()) {
            const QString id = v.toMap().value("id").toString();
            if (firstId.isEmpty())
                firstId = id;
            else
                secondId = id;
        }
        QVERIFY(!firstId.isEmpty() && !secondId.isEmpty());
        QTest::qWait(300);

        QTRY_VERIFY2_WITH_TIMEOUT(findClipBody(window->contentItem(), firstId) != nullptr &&
                                      findClipBody(window->contentItem(), secondId) != nullptr,
                                  "both clip delegates must exist in the timeline", 10000);
        auto *firstBody = findClipBody(window->contentItem(), firstId);
        auto *secondBody = findClipBody(window->contentItem(), secondId);
        QVERIFY(firstBody && secondBody);

        auto *model = backend.clipModel();
        QVERIFY(model);
        QCOMPARE(model->rowCount(), 2);
        const int idRole = model->roleNames().key("id");
        const int brightnessRole = model->roleNames().key("brightness");
        QVERIFY(idRole != 0 && brightnessRole != 0);
        QSignalSpy dataSpy(model, &QAbstractItemModel::dataChanged);
        QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);
        QSignalSpy insertedSpy(model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removedSpy(model, &QAbstractItemModel::rowsRemoved);
        QVERIFY(dataSpy.isValid() && resetSpy.isValid() && insertedSpy.isValid() && removedSpy.isValid());

        // One slider tick: a single property edit on one clip.
        backend.updateClipProperty(firstId, "brightness", 0.25);
        QTest::qWait(300);

        // The edit arrived as row data, and only as row data: no reset, no insert, no remove.
        QVERIFY2(dataSpy.count() >= 1, "the edit must reach the clip model as dataChanged");
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(insertedSpy.count(), 0);
        QCOMPARE(removedSpy.count(), 0);

        // The edited value is readable through the model row.
        int firstRow = -1;
        for (int i = 0; i < model->rowCount(); ++i) {
            if (model->data(model->index(i), idRole).toString() == firstId)
                firstRow = i;
        }
        QVERIFY2(firstRow >= 0, "the edited clip must be a row of the clip model");
        QCOMPARE(model->data(model->index(firstRow), brightnessRole).toDouble(), 0.25);

        // Zero delegate creations for the edit: the very same items are still in the tree.
        // (QVERIFY, not QCOMPARE: a recreated delegate is already destroyed, and QTest would
        // dereference the stale pointer while formatting a QCOMPARE failure.)
        auto *firstBodyAfter = findClipBody(window->contentItem(), firstId);
        auto *secondBodyAfter = findClipBody(window->contentItem(), secondId);
        QVERIFY2(firstBodyAfter == firstBody,
                 qPrintable(QString("the edited clip's delegate was recreated (%1 -> %2)")
                                .arg(quintptr(firstBody)).arg(quintptr(firstBodyAfter))));
        QVERIFY2(secondBodyAfter == secondBody,
                 qPrintable(QString("the untouched clip's delegate was recreated (%1 -> %2)")
                                .arg(quintptr(secondBody)).arg(quintptr(secondBodyAfter))));
    }

    void timelineCreatesOnlyVisibleItems() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vid = makeSampleVideo(dir.path(), "long.mp4", "0x336699", 600.0);
        QVERIFY(!vid.isEmpty());

        QQuickStyle::setStyle("Material");
        Theme theme(dir.path());
        Design design(theme);
        Backend backend;
        // Register the module's types before injecting instances: a singleton instance seals the
        // module version, and the module's own registration would then be dropped.
        extern void qml_register_types_CutCap();
        qml_register_types_CutCap();
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Theme", &theme);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Design", &design);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Backend", &backend);
        QStringList qmlErrors;
        g_qmlErrors = &qmlErrors;
        g_previousHandler = qInstallMessageHandler(captureQmlErrors);
        QQmlApplicationEngine engine;
        engine.load(QUrl("qrc:/qml/Main.qml"));
        qInstallMessageHandler(g_previousHandler);
        g_qmlErrors = nullptr;
        QVERIFY2(engine.rootObjects().size() == 1, qPrintable(qmlErrors.join("\n")));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        backend.importSingleMedia(vid);
        waitForImports(backend);
        window->resize(1280, 800);
        backend.setZoomPxPerSec(240.0);   // maximum zoom: the clip is 144,000 px wide
        QTest::qWait(400);

        auto *flick = findItem(window->contentItem(), "timelineFlick");
        QVERIFY(flick);
        QVERIFY2(flick->property("contentWidth").toDouble() > 100000.0, "the timeline is not long enough to test culling");

        auto countChildren = [&](const QString &name) {
            std::function<int(QQuickItem *)> walk = [&](QQuickItem *it) -> int {
                if (it->objectName() == name && it->isVisible())
                    return int(it->childItems().size());
                for (QQuickItem *child : it->childItems()) {
                    const int n = walk(child);
                    if (n >= 0)
                        return n;
                }
                return -1;
            };
            return walk(window->contentItem());
        };
        auto audit = [&](const char *when) {
            for (const char *name : {"clipStrip", "timeRuler", "timeGrid"}) {
                const int n = countChildren(name);
                QVERIFY2(n >= 4, qPrintable(QString("%1: %2 shows too few items (%3)").arg(when, name).arg(n)));
                QVERIFY2(n <= 80, qPrintable(QString("%1: %2 created %3 items for one viewport").arg(when, name).arg(n)));
            }
        };
        audit("at the start");

        // Scrolled far into the clip, the visible tiles must be the ones under the viewport.
        flick->setProperty("contentX", 60000.0);
        QTest::qWait(300);
        audit("scrolled to 60000 px");
        std::function<QQuickItem *(QQuickItem *)> findStrip = [&](QQuickItem *it) -> QQuickItem * {
            if (it->objectName() == "clipStrip" && it->isVisible())
                return it;
            for (QQuickItem *child : it->childItems())
                if (auto *found = findStrip(child))
                    return found;
            return nullptr;
        };
        auto *strip = findStrip(window->contentItem());
        QVERIFY(strip);
        const QRectF view(flick->mapToScene(QPointF(0, 0)), QSizeF(flick->width(), flick->height()));
        int tilesInView = 0;
        for (QQuickItem *tile : strip->childItems()) {
            if (tile->width() <= 0)
                continue;
            const qreal left = tile->mapToScene(QPointF(0, 0)).x();
            if (left + tile->width() > view.left() && left < view.right())
                ++tilesInView;
        }
        QVERIFY2(tilesInView >= 4, qPrintable(QString("only %1 filmstrip tiles are under the viewport").arg(tilesInView)));
    }

    void layoutSurvivesTiledSizes() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vid = makeSampleVideo(dir.path(), "tiles.mp4", "0x336699", 2.0);
        QVERIFY(!vid.isEmpty());

        QQuickStyle::setStyle("Material");
        Theme theme(dir.path());
        Design design(theme);
        Backend backend;
        extern void qml_register_types_CutCap();
        qml_register_types_CutCap();
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Theme", &theme);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Design", &design);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Backend", &backend);
        QStringList qmlErrors;
        g_qmlErrors = &qmlErrors;
        g_previousHandler = qInstallMessageHandler(captureQmlErrors);
        QQmlApplicationEngine engine;
        engine.load(QUrl("qrc:/qml/Main.qml"));
        qInstallMessageHandler(g_previousHandler);
        g_qmlErrors = nullptr;
        QVERIFY2(engine.rootObjects().size() == 1, qPrintable(qmlErrors.join("\n")));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        backend.importSingleMedia(vid);
        waitForImports(backend);

        struct Tile { int w; int h; bool bin; bool inspector; };
        // Sizes Hyprland produces on a 2560x1440 monitor, plus the documented floating minimum.
        const QList<Tile> tiles{{1273, 697, true, true}, {2550, 697, true, true}, {800, 500, true, true},
                                {780, 500, true, false}, {848, 466, true, true},  {640, 697, true, false},
                                {520, 697, false, false}};
        for (const Tile &t : tiles) {
            const QString label = QString("%1x%2").arg(t.w).arg(t.h);
            window->setGeometry(0, 0, t.w, t.h);
            QTest::qWait(400);
            QCOMPARE(window->size(), QSize(t.w, t.h));
            const QRectF win(0, 0, t.w, t.h);

            auto *bin = findItem(window->contentItem(), "mediaBinPanel");
            auto *inspector = findItem(window->contentItem(), "inspectorPanel");
            QVERIFY2(bin && inspector, qPrintable(label + ": side panels missing"));
            QVERIFY2(bin->isVisible() == t.bin, qPrintable(label + ": media bin visibility"));
            QVERIFY2(inspector->isVisible() == t.inspector, qPrintable(label + ": inspector visibility"));

            int visiblePanels = 0;
            for (const char *name : {"topBar", "mediaBinPanel", "previewPanel", "inspectorPanel", "timelinePanel", "exportButton"}) {
                auto *item = findItem(window->contentItem(), name);
                QVERIFY2(item, qPrintable(label + ": missing " + name));
                if (!item->isVisible())
                    continue;
                ++visiblePanels;
                const QRectF r(item->mapToScene(QPointF(0, 0)), QSizeF(item->width(), item->height()));
                QVERIFY2(r.width() > 0 && r.height() > 0, qPrintable(QString("%1: %2 has no area").arg(label, name)));
                QVERIFY2(win.contains(r.adjusted(1, 1, -1, -1)),
                         qPrintable(QString("%1: %2 (%3,%4 %5x%6) leaves the window")
                                        .arg(label, name).arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())));
            }
            QVERIFY2(visiblePanels >= 4, qPrintable(label + ": too few panels inspected"));

            auto *canvas = findItem(window->contentItem(), "previewCanvas");
            QVERIFY2(canvas, qPrintable(label + ": previewCanvas missing"));
            QVERIFY2(canvas->height() >= 100,
                     qPrintable(QString("%1: the viewer picture is only %2 px tall").arg(label).arg(canvas->height())));

            QStringList problems;
            int inspected = 0;
            collectUnreadableButtons(window->contentItem(), &problems, &inspected);
            QVERIFY2(problems.isEmpty(), qPrintable(label + ": " + problems.join("\n")));
            QVERIFY2(inspected >= 8, qPrintable(label + ": too few labelled controls inspected"));
        }
    }

    // Modal dialogs swallow the editing keys. Space activates a control only when the keyboard
    // moved focus there; for everything else it is play/pause.
    void testShortcutsRespectModalsAndFocus() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vid = makeSampleVideo(dir.path(), "keys.mp4", "0x663399", 2.0);
        QVERIFY(!vid.isEmpty());

        QQuickStyle::setStyle("Material");
        Theme theme(dir.path());
        Design design(theme);
        Backend backend;
        // Register the module's types before injecting instances: a singleton instance seals the
        // module version, and the module's own registration would then be dropped.
        extern void qml_register_types_CutCap();
        qml_register_types_CutCap();
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Theme", &theme);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Design", &design);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Backend", &backend);
        QStringList qmlErrors;
        g_qmlErrors = &qmlErrors;
        g_previousHandler = qInstallMessageHandler(captureQmlErrors);
        QQmlApplicationEngine engine;
        engine.load(QUrl("qrc:/qml/Main.qml"));
        qInstallMessageHandler(g_previousHandler);
        g_qmlErrors = nullptr;
        QVERIFY2(engine.rootObjects().size() == 1, qPrintable(qmlErrors.join("\n")));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        backend.importSingleMedia(vid);
        waitForImports(backend);
        QCOMPARE(backend.clipsList().size(), 1);
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
        backend.setPlayheadSec(1.0);

        auto *exportDialog = window->findChild<QObject *>("exportDialog");
        QVERIFY2(exportDialog, "the export dialog does not exist");

        // A modal dialog swallows the editing keys.
        QVERIFY(QMetaObject::invokeMethod(exportDialog, "open"));
        QTRY_VERIFY(exportDialog->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_S);
        QTest::keyClick(window, Qt::Key_Delete);
        QTest::keyClick(window, Qt::Key_Space);
        QTest::qWait(200);
        QCOMPARE(backend.clipsList().size(), 1);
        QVERIFY2(!backend.playing(), "Space reached the transport behind a modal dialog");
        QVERIFY(QMetaObject::invokeMethod(exportDialog, "close"));
        QTRY_VERIFY(!exportDialog->property("opened").toBool());

        // Positive control: with no dialog the same key does reach the timeline.
        QTest::keyClick(window, Qt::Key_S);
        QTRY_COMPARE(backend.clipsList().size(), 2);

        // Keyboard focus on a button: Space presses the button, it does not start playback.
        auto *exportButton = window->findChild<QQuickItem *>("exportButton");
        QVERIFY2(exportButton, "the Export button does not exist");
        exportButton->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(exportButton->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(exportDialog->property("opened").toBool());
        QVERIFY2(!backend.playing(), "Space on a keyboard-focused button started playback");
        QVERIFY(QMetaObject::invokeMethod(exportDialog, "close"));
        QTRY_VERIFY(!exportDialog->property("opened").toBool());

        // Focus that came from a click: Space is still play/pause.
        const QPoint centre = exportButton->mapToScene(QPointF(exportButton->width() / 2,
                                                               exportButton->height() / 2)).toPoint();
        // "opened" drops at close(), but the exit transition keeps the modal overlay up and would
        // swallow the click, so wait until the dialog is hidden.
        QTRY_VERIFY(!exportDialog->property("visible").toBool());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, centre);
        QTRY_VERIFY(exportDialog->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(exportDialog, "close"));
        QTRY_VERIFY(!exportDialog->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY2(backend.playing(), "Space after a mouse click must still play");
        QVERIFY(!exportDialog->property("opened").toBool());
        backend.setPlaying(false);
    }

    // Arrow keys on a keyboard-focused inspector slider adjust the slider only; the application-level
    // Left/Right playhead shortcuts must not fire on top of them.
    void sliderArrowsAdjustSliderNotPlayhead() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "sliderkeys.mp4", "0x336699", 2.0);
        QVERIFY(!vid.isEmpty());

        QQuickStyle::setStyle("Material");
        Theme theme(dir.path());
        Design design(theme);
        Backend backend;
        // Register the module's types before injecting instances: a singleton instance seals the
        // module version, and the module's own registration would then be dropped.
        extern void qml_register_types_CutCap();
        qml_register_types_CutCap();
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Theme", &theme);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Design", &design);
        qmlRegisterSingletonInstance("CutCap", 1, 0, "Backend", &backend);
        QStringList qmlErrors;
        g_qmlErrors = &qmlErrors;
        g_previousHandler = qInstallMessageHandler(captureQmlErrors);
        QQmlApplicationEngine engine;
        engine.load(QUrl("qrc:/qml/Main.qml"));
        qInstallMessageHandler(g_previousHandler);
        g_qmlErrors = nullptr;
        QVERIFY2(engine.rootObjects().size() == 1, qPrintable(qmlErrors.join("\n")));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QVERIFY2(qmlErrors.isEmpty(), qPrintable(qmlErrors.mid(0, 5).join("\n")));

        backend.importSingleMedia(vid);
        waitForImports(backend);
        QCOMPARE(backend.clipsList().size(), 1);
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
        backend.setPlayheadSec(1.0);
        QCOMPARE(backend.playheadSec(), 1.0);

        // The selected clip opens the clip inspector, which holds one slider per value row.
        QList<QQuickItem *> sliders;
        const auto items = window->findChildren<QQuickItem *>();
        for (auto *item : items) {
            if (item->inherits("QQuickSlider") && item->isVisible())
                sliders.append(item);
        }
        QVERIFY2(sliders.size() >= 5,
                 qPrintable(QString("only %1 visible sliders in the inspector").arg(sliders.size())));
        auto *slider = sliders.first();
        QVERIFY(slider->property("stepSize").toDouble() > 0);
        QVERIFY(slider->property("value").toDouble() < slider->property("to").toDouble());

        slider->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(slider->hasActiveFocus());
        const double valueBefore = slider->property("value").toDouble();
        const double playheadBefore = backend.playheadSec();

        QTest::keyClick(window, Qt::Key_Right);
        QTest::qWait(100);

        QVERIFY2(slider->property("value").toDouble() > valueBefore,
                 "Right arrow did not adjust the keyboard-focused slider");
        QVERIFY2(backend.playheadSec() == playheadBefore,
                 "Right arrow on a focused slider moved the playhead");

        // Positive control: with focus off the slider, Right still nudges the playhead.
        auto *exportButton = window->findChild<QQuickItem *>("exportButton");
        QVERIFY2(exportButton, "the Export button does not exist");
        exportButton->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(exportButton->hasActiveFocus());
        QTRY_VERIFY(!slider->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Right);
        QTest::qWait(100);
        QVERIFY2(backend.playheadSec() > playheadBefore,
                 "Right with no slider focused no longer nudges the playhead");
    }

    // Media I/O must not block the GUI thread: the call returns at once and the asset, its real
    // duration and its previews arrive afterwards.
    void testImportIsAsync() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "ten.mp4", "olive", 10.0);
        QVERIFY(!vid.isEmpty());

        Backend b;
        b.importSingleMedia(vid);
        // Asynchronous means the call has returned while the probe is still running: the job is
        // counted and nothing has reached the bin or the timeline yet.
        QVERIFY2(b.importing() > 0, "importSingleMedia finished the import before returning");
        QVERIFY2(b.assetsList().isEmpty() && b.clipsList().isEmpty(),
                 "importSingleMedia added the asset before returning");

        QTRY_VERIFY_WITH_TIMEOUT(b.clipsList().size() == 1, 30000);
        QVERIFY(std::abs(b.clipsList().first().toMap().value("durationSec").toDouble() - 10.0) < 0.2);
        QTRY_VERIFY_WITH_TIMEOUT(b.importing() == 0, 60000);

        const QVariantMap asset = b.assetsList().first().toMap();
        QVERIFY2(!asset.value("thumbnailPath").toString().isEmpty(), "the thumbnail never arrived");
        QVERIFY2(!asset.value("filmstripPath").toString().isEmpty(), "the filmstrip never arrived");
        QCOMPARE(asset.value("waveformPeaks").toList().size(), 80);
    }

    // A job started for one project must not pour its result into the next one.
    void testStaleCaptionResultDropped() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "speech.mp4", "purple", 2.0);
        QVERIFY(!vid.isEmpty());

        Backend b;
        b.importSingleMedia(vid);
        waitForImports(b);
        QCOMPARE(b.clipsList().size(), 1);
        const QString clipId = b.clipsList().first().toMap().value("id").toString();

        // A whisper-cli that takes a second and then writes a caption, so the job is still running
        // when the project is replaced.
        const QString bin = dir.path() + "/faketools";
        QVERIFY(!makeTool(bin, "whisper-cli", "#!/bin/sh\nout=\n"
                                              "while [ $# -gt 0 ]; do\n"
                                              "  if [ \"$1\" = \"-of\" ]; then out=\"$2\"; fi\n"
                                              "  shift\n"
                                              "done\n"
                                              "sleep 1\n"
                                              "printf '1\\n00:00:00,000 --> 00:00:01,000\\nStale caption\\n\\n' > \"$out.srt\"\n"
                                              "").isEmpty());
        const QByteArray oldPath = qgetenv("PATH");
        PathRestore restore{oldPath};
        qputenv("PATH", bin.toUtf8() + ":" + oldPath);

        b.generateAutoCaptions(clipId, "en");
        QVERIFY2(b.busy(), "the caption job must mark the app busy");
        b.newProject();
        QVERIFY(b.clipsList().isEmpty());
        QCOMPARE(b.textsList().size(), 0);

        QTRY_VERIFY_WITH_TIMEOUT(!b.busy(), 30000);
        QCOMPARE(b.textsList().size(), 0);
        QVERIFY2(!b.statusMessage().contains("auto captions", Qt::CaseInsensitive), qPrintable(b.statusMessage()));
    }

    // A multi-file drop must not keep feeding files into the project that replaced its own.
    void testImportChainStopsAtNewProject() {
        QTemporaryDir dir;
        QVariantList files;
        for (const QString &name : {QStringLiteral("one.mp4"), QStringLiteral("two.mp4"), QStringLiteral("three.mp4")}) {
            const QString vid = makeSampleVideo(dir.path(), name, "teal", 1.0);
            QVERIFY(!vid.isEmpty());
            files << vid;
        }

        // Positive control: left alone, the chain delivers all three clips.
        Backend control;
        control.importMediaAt(files, 0, 0.0);
        waitForImports(control);
        QCOMPARE(control.clipsList().size(), 3);

        Backend b;
        b.importMediaAt(files, 0, 0.0);
        QVERIFY2(b.importing() > 0, "the first file must still be importing when the project is replaced");
        b.newProject();
        waitForImports(b);
        QTest::qWait(300);
        waitForImports(b);
        QCOMPARE(b.clipsList().size(), 0);
        QCOMPARE(b.assetsList().size(), 0);
    }

    // A project that fails to open leaves the current one in place, so its running jobs still count.
    void testFailedOpenKeepsRunningJobs() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "keep.mp4", "navy", 1.0);
        QVERIFY(!vid.isEmpty());
        const QString broken = dir.path() + "/broken.cutcap";
        QFile f(broken);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a project");
        f.close();

        Backend b;
        b.importSingleMedia(vid);
        QVERIFY2(b.importing() > 0, "the import must still be running when the open fails");
        QVERIFY2(!b.openProject(broken), "a corrupt project must fail to open");
        waitForImports(b);
        QCOMPARE(b.clipsList().size(), 1);
    }

private:
    QTemporaryDir m_xdgRoot;
};

// Video textures, MultiEffect and shapes need an RHI scene graph; the offscreen platform otherwise
// falls back to the software renderer. OpenGL works offscreen (Mesa provides it even without a GPU).
static void useRhiSceneGraph() {
    if (!qEnvironmentVariableIsSet("QT_QUICK_BACKEND"))
        qputenv("QT_QUICK_BACKEND", "rhi");
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", "opengl");
}
Q_CONSTRUCTOR_FUNCTION(useRhiSceneGraph)

QTEST_MAIN(CutCapTests)
#include "cutcap_tests.moc"
