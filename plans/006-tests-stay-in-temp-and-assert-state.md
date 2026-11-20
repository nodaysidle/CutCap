# Plan 006: The test suite writes only to temporary directories and checks state, not timing

> **Executor instructions**: Follow this plan step by step. Run every verification command and
> confirm the expected result before moving to the next step. If anything in "STOP conditions"
> occurs, stop and report; do not improvise. Do not edit anything under `plans/`.
>
> **Drift check (run first)**: `git log --oneline -1` must show a commit on branch
> `advisor/audit-fixes`. Compare the "Current state" excerpts with the live code before editing;
> line numbers are approximate, the quoted code is the anchor. A mismatch is a STOP condition.

## Status

- **Priority**: P2
- **Effort**: S
- **Risk**: LOW
- **Depends on**: plan 001 (it adds `MediaEngine::mediaDir()`, which this plan isolates too)
- **Category**: tests
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

CutCap's QtTest suite is one file, `tests/cutcap_tests.cpp`. Three defects make it less trustworthy
than its green result suggests:

1. Every run writes thumbnails and filmstrips into the developer's real cache,
   `~/.cache/cutcap_tests`. Cache keys include the temporary path, so nothing is ever reused: the
   directory held 7,574 files (31 MB) on the audit machine and grows on every run. Five screenshots
   are also written to fixed `/tmp/cutcap_*.png` paths, which two concurrent runs overwrite.
2. `testImportIsAsync` asserts that `importSingleMedia` returns in under 200 ms. A fully synchronous
   probe takes about 75 ms on the sample file, so the test stays green if imports become
   synchronous again, which is the regression it exists to catch. The repo rule is "Never assert on
   coordinates, counts or timings the harness picked for itself."
3. The layout audit in `uiSmokeButtonsPipAndDrop` resizes the window to several sizes but never
   checks that the window actually took the size, and its helper silently skips a control that was
   squeezed to zero size.

## Current state

All in `tests/cutcap_tests.cpp`.

- There is no `initTestCase()` or `cleanupTestCase()` slot. The class starts with:

```cpp
class CutCapTests : public QObject {
    Q_OBJECT

private slots:
```

- The app's code resolves `QStandardPaths::CacheLocation` and (after plan 001)
  `QStandardPaths::AppDataLocation`. On Linux Qt derives both from the environment variables
  `XDG_CACHE_HOME` and `XDG_DATA_HOME` each time they are asked for.

- Fixed screenshot paths (five lines):

```cpp
        QVERIFY(shot.save("/tmp/cutcap_verify.png"));
        QVERIFY(desktop.save("/tmp/cutcap_desktop.png"));
        QVERIFY(compact.save("/tmp/cutcap_compact.png"));
        auditDialog(exportDlg, "exportDialog", "/tmp/cutcap_export_dialog.png");
        auditDialog(shortcutsDlg, "shortcutsDialog", "/tmp/cutcap_shortcuts_dialog.png");
```

  All five are inside `uiSmokeButtonsPipAndDrop`, which begins with `QTemporaryDir dir;`.

- `testImportIsAsync`:

```cpp
        Backend b;
        QElapsedTimer timer;
        timer.start();
        b.importSingleMedia(vid);
        const qint64 elapsedMs = timer.elapsed();
        QVERIFY2(elapsedMs < 200, qPrintable(QString("importSingleMedia blocked the GUI for %1 ms").arg(elapsedMs)));

        QTRY_VERIFY_WITH_TIMEOUT(b.clipsList().size() == 1, 30000);
```

- The layout audit's resizes, each followed by a wait and `auditViewportLayout("<label>")`:

```cpp
        window->resize(1280, 800);
        ...
        window->resize(900, 600);
        ...
        window->resize(880, 580);
        ...
        window->resize(780, 500);
```

- The helper that checks labelled controls starts by skipping zero-size items:

```cpp
static void collectUnreadableButtons(QQuickItem *item, QStringList *problems, int *inspected = nullptr) {
    if (!item || !item->isVisible() || item->width() <= 0 || item->height() <= 0)
        return;
```

  `ActionButton` (the app's only button component) exposes a `labelItem` property; the helper
  already uses `item->property("labelItem")` to recognise one.

Repo rules that apply (from `AGENTS.md`): "A test that passes for the wrong reason is a defect.
Before trusting a green run, confirm the assertion can fail: break the behaviour it covers, watch it
fail, restore." and "A guard that inspects nothing is not a guard."

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |
| Count files in the real cache | `ls ~/.cache/cutcap_tests 2>/dev/null \| wc -l` | a number |

## Scope

**In scope** (the only file you may modify):
- `tests/cutcap_tests.cpp`

**Out of scope** (do NOT touch):
- Everything under `src/`.
- Splitting `uiSmokeButtonsPipAndDrop` into smaller tests: wanted, but a separate job.
- Deleting the existing `~/.cache/cutcap_tests` directory: leave the developer's files alone.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only `tests/cutcap_tests.cpp`.
  Message: `A06: tests keep their cache in a temp dir and assert import state, not timing`

## Steps

### Step 1: Isolate the cache and data directories

Record the baseline: run the "Count files in the real cache" command and note the number (call it
`BEFORE`).

Add a member and two slots at the top of the class, directly after `private slots:`:

```cpp
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
```

and, in a `private:` section at the very end of the class (after the last slot, before `};`):

```cpp
private:
    QTemporaryDir m_xdgRoot;
```

If the class already has a `private:` section, add the member there instead. Do not add a
`cleanupTestCase`; `QTemporaryDir` removes the directory when the test object is destroyed.

**Verify**: full test command → 0 failed. Then the cache-count command → the same number as
`BEFORE` (no new files in the real cache).

### Step 2: Screenshots go to the test's own temp directory

Replace the five fixed paths with paths under the slot's `dir`:
`dir.path() + "/cutcap_verify.png"`, and likewise `cutcap_desktop.png`, `cutcap_compact.png`,
`cutcap_export_dialog.png`, `cutcap_shortcuts_dialog.png`.

**Verify**: `grep -c '"/tmp/cutcap_' tests/cutcap_tests.cpp` → `0`.

### Step 3: `testImportIsAsync` checks state

Replace the `QElapsedTimer` block (the five lines from `QElapsedTimer timer;` through the
`QVERIFY2(elapsedMs < 200, ...)`) with:

```cpp
        b.importSingleMedia(vid);
        // Asynchronous means the call has returned while the probe is still running: the job is
        // counted and nothing has reached the bin or the timeline yet.
        QVERIFY2(b.importing() > 0, "importSingleMedia finished the import before returning");
        QVERIFY2(b.assetsList().isEmpty() && b.clipsList().isEmpty(),
                 "importSingleMedia added the asset before returning");
```

Leave the rest of the slot as it is. Remove `#include <QElapsedTimer>` only if nothing else in the
file uses `QElapsedTimer` (`grep -c QElapsedTimer tests/cutcap_tests.cpp`).

**Verify**: full test command → 0 failed.

### Step 4: The layout audit checks the window size and reports a squeezed button

1. After each of the four `window->resize(W, H);` calls listed in "Current state", once the existing
   `QTest::qWait(...)` that follows it has run, add
   `QCOMPARE(window->size(), QSize(W, H));` with the same numbers.

2. In `collectUnreadableButtons`, report a visible `ActionButton` that has no area instead of
   skipping it. Replace the first `if` with:

```cpp
    if (!item || !item->isVisible())
        return;
    if (item->width() <= 0 || item->height() <= 0) {
        // A button the layout squeezed to nothing is the worst kind of unreadable.
        if (item->property("labelItem").value<QQuickItem *>() && !item->property("text").toString().isEmpty())
            problems->append(QString("ActionButton '%1' has no area (%2x%3)")
                                 .arg(item->property("text").toString()).arg(item->width()).arg(item->height()));
        return;
    }
```

**Verify**: full test command → 0 failed, and the `Totals` line shows the same number of passed
tests as before this plan (this plan adds no test slots; `initTestCase` is already counted by
QtTest whether or not it is defined).

### Step 5: Prove the new assertions can fail

1. In `testImportIsAsync`, temporarily add `waitForImports(b);` on the line directly after
   `b.importSingleMedia(vid);`. Run the full test command → `FAIL!  : CutCapTests::testImportIsAsync`
   must appear. Remove the line.
2. Temporarily change one size check to a wrong value, e.g. `QCOMPARE(window->size(), QSize(781, 500));`.
   Run the full test command → `FAIL!  : CutCapTests::uiSmokeButtonsPipAndDrop` must appear. Restore it.
3. Run the full test command → 0 failed.

### Step 6: Commit

Commit with the message from "Git workflow".

## Done criteria

- [ ] Full test run has 0 failed and the same passed count as before this plan
- [ ] `grep -c '"/tmp/cutcap_' tests/cutcap_tests.cpp` → `0`
- [ ] `grep -c "elapsedMs" tests/cutcap_tests.cpp` → `0`
- [ ] The real-cache file count is unchanged by a full test run
- [ ] Step 5 showed both assertions failing when broken
- [ ] `git show --stat HEAD` lists only `tests/cutcap_tests.cpp`

## STOP conditions

- After Step 1 a test fails because it expects files in the real cache, or the real-cache count
  still grows. Report which test and what it wrote.
- After Step 4.2 the suite reports an `ActionButton ... has no area` problem. That is a real finding
  about the UI, not something to hide: report the label and the window size, and leave the check in.
- `QCOMPARE(window->size(), ...)` fails for a size the test did not get. Report the actual size.
- `QVERIFY2(b.importing() > 0, ...)` fails in `testImportIsAsync` without the deliberate break.

## Maintenance notes

- Any test that needs a cache or data path must go through `MediaEngine::cacheDir()` /
  `mediaDir()` or its own `QTemporaryDir`, never a literal path.
- `uiSmokeButtonsPipAndDrop` is still one slot of roughly 580 lines covering about fifteen
  features; its first failure hides the rest. Splitting it was deliberately left for later.
