# Plan 004: Paths loaded from a project file are local absolute paths or nothing

> **Executor instructions**: Follow this plan step by step. Run every verification command and
> confirm the expected result before moving to the next step. If anything in "STOP conditions"
> occurs, stop and report; do not improvise. Do not edit anything under `plans/`.
>
> **Drift check (run first)**: `git log --oneline -1` must show a commit on branch
> `advisor/audit-fixes`. Compare the "Current state" excerpts with the live code before editing;
> line numbers are approximate, the quoted code is the anchor. A mismatch is a STOP condition.

## Status

- **Priority**: P1
- **Effort**: S
- **Risk**: LOW
- **Depends on**: none
- **Category**: security
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

CutCap is a local-only video editor: a documented product rule is "No network calls. The app is
100 % local." A `.cutcap` project is a JSON file that may come from someone else. Its media paths
are read without any check and handed to `ffmpeg -i <path>` as soon as the project is opened (to
build thumbnails, filmstrips and waveforms) and again at export. ffmpeg treats a value such as an
`http://`, `https://` or `tcp://` address as a network input and connects to it, so merely opening
a crafted project makes the machine contact a server of the author's choosing. After this plan the
loader keeps a media path only when it is an absolute local path, so nothing else can reach ffmpeg
or a QML image source.

## Current state

- `src/project.h` declares free helpers near the top, e.g.
  `QString sanitizedChromaColor(const QString &raw);`
- `src/project.cpp` defines them near the top (`sanitizedChromaColor` around line 12,
  `int evenDimension(int v)` around line 25). These are the exemplars for a small validation helper.
- `src/project.cpp`, `MediaAsset MediaAsset::fromJson(const QJsonObject &obj)`:

```cpp
    a.path = obj.value("path").toString();
    ...
    a.thumbnailPath = obj.value("thumbnailPath").toString();
    a.filmstripPath = obj.value("filmstripPath").toString();
```

- `src/project.cpp`, `TimelineClip TimelineClip::fromJson(const QJsonObject &obj)`:

```cpp
    c.sourcePath = obj.value("sourcePath").toString();
    ...
    c.processedPath = obj.value("processedPath").toString();
```

- Consumers (for understanding only, do not edit): `src/mediaengine.cpp` runs
  `ffmpeg ... -i asset.path` in `ensureThumbnail`, `ensureFilmstrip` and `extractWaveform`, and
  `-i path` in `buildExportArgs`. `ensureThumbnail`, `ensureFilmstrip` and `extractWaveform` already
  return early for an empty path. QML loads `"file://" + thumbnailPath` / `filmstripPath`.
- Paths the app writes itself are always absolute (`QFileInfo(path).absoluteFilePath()` in
  `Backend::importAssetAsync`, cache and data directory paths elsewhere).
- `tests/cutcap_tests.cpp`, slot `testFromJsonClampsRanges` (search for it) is the pattern: it
  builds a `QJsonObject`, calls `TimelineClip::fromJson(...)` / `MediaAsset::fromJson(...)` and
  checks fields.

Repo rules that apply (from `AGENTS.md`): "A test that passes for the wrong reason is a defect.
Before trusting a green run, confirm the assertion can fail: break the behaviour it covers, watch it
fail, restore." and "Fix the cause, and keep one rule for it." The `.cutcap` JSON key names must not
change.

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |

## Scope

**In scope** (the only files you may modify):
- `src/project.h`
- `src/project.cpp`
- `tests/cutcap_tests.cpp`

**Out of scope** (do NOT touch):
- `src/mediaengine.cpp`, `src/backend.cpp`, `src/cli.cpp`: paths typed by the user on the command
  line or picked in a file dialog are trusted; only the project file is not.
- `toJson` functions and JSON key names.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only the files you changed (`git add <files>`, not `git add -A`).
  Message: `A04: project files can only name absolute local media paths`

## Steps

### Step 1: Add the test first

Add a private slot `testProjectPathsMustBeLocalFiles` to `tests/cutcap_tests.cpp`, directly after
`testFromJsonClampsRanges`. Build the URL strings from parts so no literal address sits in the
source as one token:

```cpp
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
```

**Verify (the test must FAIL now)**: full test command → a line
`FAIL!  : CutCapTests::testProjectPathsMustBeLocalFiles`.

### Step 2: One helper, used at the five load sites

In `src/project.h`, next to `sanitizedChromaColor`, declare:

```cpp
// Keeps a media path from a project file only when it is an absolute local path.
QString localMediaPath(const QString &raw);
```

In `src/project.cpp`, next to `sanitizedChromaColor`, define:

```cpp
// Anything that is not an absolute path (a URL, an ffmpeg protocol such as concat:, a relative
// path) is dropped: ffmpeg would open it as a network or protocol input.
QString localMediaPath(const QString &raw) {
    return QDir::isAbsolutePath(raw) ? raw : QString();
}
```

Add `#include <QDir>` to `src/project.cpp` if it is not already included.

Wrap the five reads shown in "Current state": `a.path`, `a.thumbnailPath`, `a.filmstripPath`,
`c.sourcePath`, `c.processedPath`, e.g. `a.path = localMediaPath(obj.value("path").toString());`.

**Verify**: `grep -c "localMediaPath(obj.value" src/project.cpp` → `5`.

### Step 3: Run everything

**Verify**: full test command → 0 failed, no `FAIL` line, exactly one more passing test than before
this plan.

### Step 4: Commit

Commit with the message from "Git workflow".

## Done criteria

- [ ] Step 1 showed the new test failing before the helper existed
- [ ] Full test run has 0 failed and one more passing test than before
- [ ] `grep -c "localMediaPath(obj.value" src/project.cpp` → `5`
- [ ] `git show --stat HEAD` lists only the three in-scope files

## STOP conditions

- The quoted reads in "Current state" are not in `src/project.cpp`, or another field in a
  `fromJson` function there holds a file path this plan does not list (report it, do not guess).
- An existing test fails after Step 2 because it loads a project with a relative media path. Report
  the test name and the path; do not relax the helper.

## Maintenance notes

- Any new path field added to the project format must go through `localMediaPath` on load.
- A project whose media has moved now simply has an empty path for a non-absolute entry; relinking
  missing media is not implemented and was left out of this plan.
