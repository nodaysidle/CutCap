# Plan 015: Tests cover save/open, a finished export and the CLI; the package runs the suite

> **Executor instructions**: Follow this plan step by step. Run every verification command and
> confirm the expected result before moving to the next step. If anything in "STOP conditions"
> occurs, stop and report; do not improvise. Do not edit anything under `plans/`.
>
> **Drift check (run first)**: `git log --oneline -1` must show a commit on branch
> `advisor/audit-fixes`. Compare the "Current state" excerpts with the live code before editing;
> line numbers are approximate, the quoted code is the anchor. A mismatch is a STOP condition.

## Status

- **Priority**: P2
- **Effort**: M
- **Risk**: LOW (tests and packaging only; no app code changes)
- **Depends on**: plans 004, 006 and 011
- **Category**: tests
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

CutCap's test suite never opens a saved project through the app, never lets an export finish
successfully, and never runs a single command of the headless CLI. A field dropped from the project
file format, a broken export success path or a broken CLI command would all ship with a green
suite. The Arch package (`pkgbuild/PKGBUILD`) also has no `check()` step, so the suite is not run
when the package is built. This plan adds those tests and the `check()` function. It changes no
application code: if a new test exposes a real bug, that is a finding to report, not something to
fix here.

## Current state

`tests/cutcap_tests.cpp` (one QtTest class, `CutCapTests`):

- `projectRoundTripAndSrtParsing` saves and loads a `Project` but compares only the title, the clip
  count, one bool and the text count.
- `testExportErrorHasReason` and `testCancelRemovesPartial` cover export failure and cancel only.
  The second shows how to start an export through `Backend` and wait:

```cpp
        Backend b;
        QTRY_VERIFY_WITH_TIMEOUT(b.capabilities().value("encoders").toMap().value("h264").toBool(), 15000);
        b.importSingleMedia(vid);
        waitForImports(b);
        const QString out = dir.path() + "/partial.mp4";
        QSignalSpy finished(&b, &Backend::exportFinished);
        b.startExport(out, "high", "h264", "project");
```

  `exportFinished(bool ok, QString message)`; on success the message starts with `Saved to `.
- `launcherArgumentsOpenNoPhantomFile` is the only CLI test and covers only `Cli::guiFiles`.
- Helpers: `makeSampleVideo(dir, name, color, seconds)` returns the path of a small 320×240 video
  with a sine tone; `probeDurationSec(path)` returns a media file's duration via ffprobe;
  `waitForImports(backend)`.

`src/project.h` declares `struct MediaAsset`, `struct TimelineClip`, `struct TextOverlay` and
`struct Project`, each with `toJson()` and a static `fromJson(...)`; `Project` has
`saveToFile(path)` and static `loadFromFile(path, &ok, &err)`. `fromJson` clamps numbers to ranges
(for example speed 0.25–4, track 0–2, opacity 0–1) and, after plan 004, keeps only absolute media
paths.

`src/backend.h`: `bool saveProject(const QString &pathOrUrl = {})`, `bool openProject(const QString &pathOrUrl)`,
`void splitAtPlayhead()`, `void addTextAtPlayhead(const QString &text = "New Title", bool isCaption = false)`,
`QString projectTitle() const`, `void setProjectTitle(const QString &)`, `bool dirty() const`,
`QVariantList clipsList() / textsList() / assetsList()`.

`src/cli.h`: `namespace Cli { int run(const QStringList &args); ... }`. `Cli::run` takes the full
argument list including the program name and returns the exit code (0 on success, 1 on error).
Commands (from its help text):

```
cutcap new <project.cutcap> [--title T] [--json]
cutcap add-clip <project.cutcap> <media> [--track 0] [--start -1] [--json]
cutcap split <project.cutcap> <clip-id> <at-sec> [--json]
cutcap add-text <project.cutcap> --text "..." [--start 0] [--duration 3] [--caption] [--json]
cutcap export <project.cutcap> <out.mp4> [--preset high|medium|fast] [--codec h264|h265|av1] [--json]
cutcap probe <media> [--json]
```

`pkgbuild/PKGBUILD` today:

```bash
makedepends=('qt6-base' 'qt6-declarative' 'qt6-multimedia')

build() {
    mkdir -p "$srcdir/build"
    cd "$srcdir/build"
    qmake6 "$startdir/../cutcap.pro"
    make
}
```

`bin/test` builds and runs the suite like this: `qmake6 "$ROOT/tests/cutcap_tests.pro"`, `make`,
`QT_QPA_PLATFORM=offscreen ./cutcap_tests`.

Repo rules (from `AGENTS.md`): "A test that passes for the wrong reason is a defect. Before trusting
a green run, confirm the assertion can fail: break the behaviour it covers, watch it fail, restore."
"A guard that inspects nothing is not a guard — assert a minimum count alongside 'nothing is wrong'."

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |
| PKGBUILD syntax | `bash -n pkgbuild/PKGBUILD; echo $?` | `0` |

## Scope

**In scope** (the only files you may modify):
- `tests/cutcap_tests.cpp`
- `pkgbuild/PKGBUILD`

**Out of scope** (do NOT touch):
- Everything under `src/`. A new test that fails because the app is wrong is a STOP condition.
- Running `makepkg`.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only the files you changed (`git add <files>`, not `git add -A`).
  Message: `A15: tests for save/open, a finished export and the CLI; PKGBUILD runs the suite`

## Steps

### Step 1: The project file keeps every field

Add a private slot `projectFileKeepsEveryField` directly after `projectRoundTripAndSrtParsing`.
Read `src/project.h` and, in the test, build one `MediaAsset`, one `TimelineClip` and one
`TextOverlay` in which **every declared field** is set to a value that differs from its default and
lies inside the range `fromJson` clamps to (read `src/project.cpp` for the ranges; media paths must
be absolute, e.g. `/tmp/a.mp4`; colours `#rrggbb`). Put them in a `Project` with a non-default
title, width, height and fps, plus any lane state the struct holds. Then:

```cpp
        QTemporaryDir dir;
        const QString path = dir.path() + "/full.cutcap";
        QVERIFY(p.saveToFile(path));
        bool ok = false;
        const Project loaded = Project::loadFromFile(path, &ok);
        QVERIFY(ok);
        QCOMPARE(loaded.toJson(), p.toJson());
```

Add a guard against the test going stale when a field is added later: assert the number of keys the
JSON actually carries, so a new field forces a look at this test:

```cpp
        // If this count changes a field was added or removed: set it above, then update the number.
        QCOMPARE(p.clips.first().toJson().keys().size(), <the number you observe>);
```

(and the same for the asset and the text overlay). Print the observed numbers once with `qInfo()`
while writing the test, then hard-code them and remove the `qInfo()`.

**Verify**: full test command → 0 failed. Then prove it can fail: in the test, temporarily leave
one clip field at its default → still passes (expected, defaults round-trip); instead temporarily
change the final comparison to compare against a project whose clip `name` was altered after saving
→ must fail. Restore.

### Step 2: Save and reopen through the app

Add `backendSaveAndOpenRoundTrip` after it:

```cpp
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
```

**Verify**: full test command → 0 failed.

### Step 3: An export that finishes

Add `exportFinishesAndPlays` after `testCancelRemovesPartial`. Import a 2.0 s sample, add a text
with `addTextAtPlayhead("Burned in")`, then for each of the resolution presets `"project"` and
`"vertical"`: start an export with preset `"fast"`, codec `"h264"`, wait for
`exportFinished` (timeout 120000), and assert: the signal's first argument is `true`; the message
starts with `Saved to `; the output file exists and is larger than 1,000 bytes;
`std::abs(probeDurationSec(out) - 2.0) < 0.3`; `!b.exporting()`; `b.exportProgress()` is `1.0`.
For `"vertical"` also read the stream size with ffprobe
(`ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=p=0 <out>`) and
assert it is `1080,1920`. Use a different output file per run.

**Verify**: full test command → 0 failed.

### Step 4: The CLI, end to end

Add `cliCommandsWorkEndToEnd` after `launcherArgumentsOpenNoPhantomFile`:

```cpp
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
```

`Cli::run` prints to stdout/stderr; that is fine in a test.

**Verify**: full test command → 0 failed, exactly four more passing tests than before this plan.

### Step 5: The package runs the suite

In `pkgbuild/PKGBUILD` add `checkdepends=('ffmpeg' 'mesa')` under `makedepends`, and after
`build()`:

```bash
check() {
    mkdir -p "$srcdir/build-tests"
    cd "$srcdir/build-tests"
    qmake6 "$startdir/../tests/cutcap_tests.pro"
    make
    QT_QPA_PLATFORM=offscreen ./cutcap_tests
}
```

**Verify**: PKGBUILD syntax command → `0`. Then run the same three commands by hand in a scratch
directory to prove they work outside `./bin/test`:
`D=$(mktemp -d /tmp/claude-1000/pkgcheck.XXXXXX) && (cd "$D" && qmake6 "$OLDPWD/tests/cutcap_tests.pro" > /dev/null && make -j"$(nproc)" > make.log 2>&1 && QT_QPA_PLATFORM=offscreen ./cutcap_tests > run.log 2>&1; grep '^Totals' run.log)`
→ a `Totals: ... 0 failed` line.

### Step 6: Commit

Commit with the message from "Git workflow".

## Done criteria

- [ ] Full test run has 0 failed and four more passing tests than before this plan
- [ ] `grep -c '^check()' pkgbuild/PKGBUILD` → `1`
- [ ] The scratch-directory run in Step 5 printed a `Totals` line with `0 failed`
- [ ] `git show --stat HEAD` lists only `tests/cutcap_tests.cpp` and `pkgbuild/PKGBUILD`

## STOP conditions

- `QCOMPARE(loaded.toJson(), p.toJson())` fails in Step 1 for a field you set inside its documented
  range: a field is written but not read back (or the reverse). That is a real bug in
  `src/project.cpp`; report the field name. Do not remove the field from the test.
- A step in `backendSaveAndOpenRoundTrip`, `exportFinishesAndPlays` or `cliCommandsWorkEndToEnd`
  fails because the app behaves differently from what the test expects. Report the assertion and
  the actual value. Do not weaken it and do not edit `src/`.
- The `--track 2` image check returns 0: plan 011 has not landed.
- The `"vertical"` export takes longer than 120 s.

## Maintenance notes

- `check()` has not been run under `makepkg` in a clean chroot; the suite needs ffmpeg with
  libx264 and AAC, Mesa's software OpenGL and the fonts in `depends`. If the chroot build fails,
  that list is where to look.
- Not covered: h265 and AV1 encodes and the 4K preset (slow), theme hot-reload, the Hyprland socket
  listener, and undo for every edit operation.
