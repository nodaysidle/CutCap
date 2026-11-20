# Plan 001: The startup cache sweep never deletes recordings a project uses

> **Executor instructions**: Follow this plan step by step. Run every verification command and
> confirm the expected result before moving to the next step. If anything in "STOP conditions"
> occurs, stop and report; do not improvise. Do not edit `plans/README.md`; the reviewer keeps it.
>
> **Drift check (run first)**: `git log --oneline -1` must show a commit on branch
> `advisor/audit-fixes`. Compare the "Current state" excerpts with the live code before editing;
> line numbers are approximate, the quoted code is the anchor. A mismatch is a STOP condition.

## Status

- **Priority**: P1
- **Effort**: S
- **Risk**: LOW
- **Depends on**: none
- **Category**: bug (data loss)
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

CutCap is a Qt 6 / C++17 / QML video editor. At startup it deletes every file in its cache
directory (`~/.cache/cutcap`) that is more than 7 days old. Voiceover recordings (`vo_*.wav`),
text-to-speech clips (`tts_*.wav`) and background-removal images (`nobg_*.png`) are written to that
same directory and saved projects point at them as the clip's only media. A user who records a
voiceover, saves, and reopens the app 8 days later has lost the recording for good. After this plan
the sweep removes only files the app can regenerate, and new recordings are written to a data
directory instead of the cache.

## Current state

- `src/mediaengine.cpp` (around lines 95-114): the cache directory and the sweep.

```cpp
QString MediaEngine::cacheDir() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
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
        if (keepPaths.contains(entry.absoluteFilePath()) || entry.lastModified() >= cutoff)
            continue;
        if (QFile::remove(entry.absoluteFilePath()))
            ++removed;
    }
    return removed;
}
```

- `src/mediaengine.h` (around lines 36-38) declares `static QString cacheDir();` and
  `static int pruneCache(const QString &dir, int maxAgeDays = 7, const QStringList &keepPaths = {});`
- `src/backend.cpp` (around line 64-66), in the `Backend` constructor:

```cpp
    // Thumbnails, filmstrips, voiceovers and TTS clips pile up in the cache; sweep them once at
    // start, off the GUI thread. Preview files for a project are regenerated if they are gone.
    (void)QtConcurrent::run([] { MediaEngine::pruneCache(MediaEngine::cacheDir()); });
```

- `src/backend.cpp`, three places write user media into the cache (search for `cacheDir()`):
  - `m_voiceoverPath = MediaEngine::cacheDir() + QString("/vo_%1.wav").arg(...)`
  - `const QString outPath = MediaEngine::cacheDir() + QString("/tts_%1.wav").arg(...)`
  - `const QString outPath = MediaEngine::cacheDir() + QString("/nobg_%1.png").arg(clip.id);`
- Files the app CAN regenerate, all in the cache: `thumb_<hash>.jpg`, `strip_<hash>.jpg`
  (`src/mediaengine.cpp`, search `"/thumb_"` and `"/strip_"`) and `subs_<ms>.ass`
  (`src/backend.cpp`, search `"/subs_%1.ass"`).
- `tests/cutcap_tests.cpp`, test slot `testCachePrune` (search for `void testCachePrune()`): it
  creates `tts_old.wav` (30 days old), `tts_recent.wav` and `thumb_keep.jpg` (30 days old, passed as
  a keep path) and asserts `pruneCache(dir.path(), 7, {kept}) == 1` with `tts_old.wav` deleted. That
  assertion encodes the bug.

Repo rules that apply (from `AGENTS.md`):
- "A test that passes for the wrong reason is a defect. Before trusting a green run, confirm the
  assertion can fail: break the behaviour it covers, watch it fail, restore."
- "Fix the cause, and keep one rule for it."
- Comments say why, in one or two lines, matching the existing style. No new dependencies.

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build app | `./bin/build > /tmp/claude-1000/a-build.log 2>&1; echo $?` | prints `0` |
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep '^Totals' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` |
| Run one test (after a full run built it) | `QT_QPA_PLATFORM=offscreen ./build/tests/cutcap_tests testCachePrune 2>&1 \| tail -4` | `Totals: 3 passed, 0 failed` (the slot plus init/cleanup) |

The full suite takes about 40 s. `./bin/build` may rewrite `cutcap.qmltypes`; if `git status` shows
it changed, include it in the commit.

## Scope

**In scope** (the only files you may modify):
- `src/mediaengine.h`
- `src/mediaengine.cpp`
- `src/backend.cpp`
- `tests/cutcap_tests.cpp`

**Out of scope** (do NOT touch):
- `src/project.cpp` and the `.cutcap` JSON format. Existing projects keep pointing at files in the
  cache directory; those paths must keep working, which they do once the sweep leaves them alone.
- Moving or migrating files that already exist in `~/.cache/cutcap`.
- Any QML file.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). Do not create branches, do not push.
- One commit at the end. Message style matches the repo, for example
  `F26, F27, F28: parec header, ASS backslash, cache sweep`. Use:
  `A01: cache sweep removes only regenerable previews; recordings move to the data dir`

## Steps

### Step 1: Sweep only regenerable files

In `MediaEngine::pruneCache` (`src/mediaengine.cpp`), skip every file whose name does not start with
`thumb_`, `strip_` or `subs_`. Add the check inside the existing loop, before the age check, e.g.:

```cpp
        // Only previews and temporary subtitle files can be rebuilt; anything else in here may be
        // a recording a saved project still points at.
        const QString name = entry.fileName();
        if (!name.startsWith("thumb_") && !name.startsWith("strip_") && !name.startsWith("subs_"))
            continue;
```

Keep the signature, the `keepPaths` behaviour and the return value unchanged.

**Verify**: `grep -c 'startsWith("thumb_")' src/mediaengine.cpp` → `1`

### Step 2: Write new recordings to a data directory

Add `static QString mediaDir();` to `src/mediaengine.h` next to `cacheDir()`, and implement it in
`src/mediaengine.cpp` directly below `cacheDir()`:

```cpp
// Recordings and generated media a project depends on; unlike the cache this is never swept.
QString MediaEngine::mediaDir() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/media";
    QDir().mkpath(dir);
    return dir;
}
```

In `src/backend.cpp` change exactly the three paths listed in "Current state" (`vo_`, `tts_`,
`nobg_`) from `MediaEngine::cacheDir()` to `MediaEngine::mediaDir()`. Leave the `subs_` path and
every `thumb_`/`strip_` path on `cacheDir()`.

Update the comment above the `pruneCache` call in the `Backend` constructor so it is true: the sweep
covers thumbnails, filmstrips and temporary subtitle files only.

**Verify**: `grep -c 'MediaEngine::mediaDir()' src/backend.cpp` → `3`, and
`grep -n 'cacheDir()' src/backend.cpp` shows only the `pruneCache` line and the `subs_` line.

### Step 3: Make the test assert the right thing

Rewrite the body of `testCachePrune` in `tests/cutcap_tests.cpp` (keep the slot name). In one
`QTemporaryDir` create these files, each containing `"x"`:

| File | Age | Expect after `pruneCache(dir.path(), 7, {keptThumb})` |
|---|---|---|
| `vo_old.wav` | 30 days | still exists |
| `tts_old.wav` | 30 days | still exists |
| `nobg_old.png` | 30 days | still exists |
| `thumb_old.jpg` | 30 days | removed |
| `strip_old.jpg` | 30 days | removed |
| `subs_old.ass` | 30 days | removed |
| `thumb_recent.jpg` | now | still exists |
| `thumb_keep.jpg` | 30 days, passed in `keepPaths` | still exists |

Assert the return value is `3` with `QCOMPARE`, then `QVERIFY` each file's presence or absence. Use
the existing pattern for backdating: open the file `QIODevice::ReadWrite` and call
`setFileTime(monthAgo, QFileDevice::FileModificationTime)`, each wrapped in `QVERIFY`.

Also add `QVERIFY(MediaEngine::mediaDir() != MediaEngine::cacheDir());` at the end of the slot.

**Verify**: run the full test command → `Totals: 41 passed, 0 failed`.

### Step 4: Prove the test can fail

Temporarily delete the three lines added in Step 1, rebuild and run only this test:
`./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E 'FAIL|Totals' /tmp/claude-1000/a-test.log`
→ a `FAIL!  : CutCapTests::testCachePrune` line must appear. Restore the three lines and run the
full test command again → `Totals: 41 passed, 0 failed`.

### Step 5: Commit

`git add -A && git commit -m "<message from Git workflow>"`, then `git status --short` → empty.

## Done criteria

- [ ] `./bin/build` exits 0
- [ ] Full test run prints `Totals: 41 passed, 0 failed`
- [ ] `grep -c 'startsWith("thumb_")' src/mediaengine.cpp` → `1`
- [ ] `grep -c 'MediaEngine::mediaDir()' src/backend.cpp` → `3`
- [ ] Step 4 showed the test failing without the fix
- [ ] `git show --stat HEAD` lists only in-scope files (plus `cutcap.qmltypes` if the build changed it)

## STOP conditions

- The quoted code in "Current state" is not in the files.
- More than three `cacheDir()` uses in `src/backend.cpp` write `vo_`, `tts_` or `nobg_` files, or
  another file prefix is written to the cache that this plan does not list.
- Any other test starts failing and one fix attempt does not resolve it.
- The fix seems to need a file outside the in-scope list.

## Maintenance notes

- Any new kind of file written to `cacheDir()` must be regenerable and must be added to the prefix
  list in `pruneCache`, or it is never cleaned up.
- Recordings made before this change stay in `~/.cache/cutcap`; they are safe from the app now but
  not from a user clearing `~/.cache`. Migrating them was left out on purpose.
