# Plan 008: Closing the window asks first while an export, recording or job is running

> **Executor instructions**: Follow this plan step by step. Run every verification command and
> confirm the expected result before moving to the next step. If anything in "STOP conditions"
> occurs, stop and report; do not improvise. Do not edit anything under `plans/`.
>
> **Drift check (run first)**: `git log --oneline -1` must show a commit on branch
> `advisor/audit-fixes`. Compare the "Current state" excerpts with the live code before editing;
> line numbers are approximate, the quoted code is the anchor. A mismatch is a STOP condition.

## Status

- **Priority**: P2
- **Effort**: S-M
- **Risk**: LOW
- **Depends on**: plans/002-shortcuts-respect-modals-and-focus.md (it adds `win.modalOpen`)
- **Category**: bug
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

CutCap asks "Unsaved changes" before closing a project with unsaved edits, but it checks nothing
else. With a saved project, closing the window (or Ctrl+Q, or the compositor's close-window key)
in the middle of an export closes immediately: ffmpeg is killed and a truncated output file is left
on disk looking like a finished video. Closing during a voiceover recording loses the recording.
After this plan the window asks before closing while work is running, and an export that is killed
at shutdown removes its partial file, the same way a cancelled export already does.

## Current state

### `src/qml/Main.qml` (window `id: win`)

```qml
    property var pendingDiscard: null
    property var pendingSave: null
    property bool allowClose: false
```

```qml
    onClosing: (close) => {
        if (allowClose || !Backend.dirty)
            return
        close.accepted = false
        pendingDiscard = () => {
            win.allowClose = true
            win.close()
        }
        discardDialog.open()
    }
```

The existing unsaved-changes dialog is the visual pattern to copy. It starts like this and holds a
`ColumnLayout` with two `Text` items and a `RowLayout` of `ActionButton`s:

```qml
    Dialog {
        id: discardDialog
        objectName: "discardDialog"
        title: "Unsaved changes"
        modal: true
        anchors.centerIn: parent
        width: Math.min(420, win.width - Design.s6 * 2)
        padding: Design.s5

        Material.theme: Theme.darkMode ? Material.Dark : Material.Light
        Material.accent: Design.accent
        Material.background: Design.surfaceOverlay
        Material.foreground: Design.textPrimary

        background: Rectangle {
            color: Design.surfaceOverlay
            radius: Design.rLg
            border.color: Design.borderStrong
            border.width: 1
        }
        ...
                ActionButton {
                    objectName: "discardCancelButton"
                    text: "Cancel"
                    fontPixelSize: Design.tBody
                    horizontalPadding: Design.s5
                    verticalPadding: Design.s2
                    onClicked: discardDialog.close()
                }
```

After plan 002 the window also has
`readonly property bool modalOpen: exportDialog.opened || shortcutsDialog.opened || discardDialog.opened || ...`.

`Backend` (QML singleton) exposes `Backend.exporting`, `Backend.recordingVoiceover` and
`Backend.busy` (true while a caption, text-to-speech or background-removal job runs), plus
`Backend.dirty`.

### `src/backend.cpp`

```cpp
Backend::~Backend() {
    if (m_exportProcess.state() != QProcess::NotRunning) {
        m_exportProcess.kill();
        m_exportProcess.waitForFinished(1000);
    }
    if (m_voiceoverProcess.state() != QProcess::NotRunning) {
        m_voiceoverProcess.terminate();
        m_voiceoverProcess.waitForFinished(1000);
    }
}
```

The path of the file being exported is the member `m_exportOutputPath`. A cancelled export already
removes it in the `QProcess::finished` handler (`QFile::remove(m_exportOutputPath);`).

### Test patterns (`tests/cutcap_tests.cpp`)

- `testCancelRemovesPartial` starts a real export and waits until the output file exists:

```cpp
        Backend b;
        QTRY_VERIFY_WITH_TIMEOUT(b.capabilities().value("encoders").toMap().value("h264").toBool(), 15000);
        b.importSingleMedia(vid);   // vid = makeSampleVideo(dir.path(), "long.mp4", "0x224466", 8.0)
        waitForImports(b);
        const QString out = dir.path() + "/partial.mp4";
        b.startExport(out, "high", "h264", "project");
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(out), 20000);
        QVERIFY2(b.exporting(), "the export finished before it could be cancelled");
```

- `testUnsavedChangesGuard` builds the real window and drives `window->close()`; it finds dialog
  objects with `window->findChild<QObject *>("discardDialog")` and presses buttons with
  `QMetaObject::invokeMethod(button, "clicked")`.

Repo rule (from `AGENTS.md`): "A test that passes for the wrong reason is a defect. Before trusting
a green run, confirm the assertion can fail: break the behaviour it covers, watch it fail, restore."
QML uses `Design.*` tokens only: no literal colours, font sizes or radii.

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |
| QML lint | `/usr/lib/qt6/bin/qmllint -I /usr/lib/qt6/qml src/qml/*.qml > /tmp/claude-1000/a-lint.log 2>&1; echo $?` | `0` |

## Scope

**In scope** (the only files you may modify):
- `src/qml/Main.qml`
- `src/backend.cpp` (the destructor only)
- `tests/cutcap_tests.cpp`

**Out of scope** (do NOT touch):
- Cancelling caption / text-to-speech / background-removal jobs at exit. Those run in the global
  thread pool and the process still waits for them after the window closes; stopping them needs
  cancellation support in `MediaEngine` and is not part of this plan.
- `ExportDialog.qml` and the export pipeline itself.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only the files you changed (`git add <files>`, not `git add -A`).
  Message: `A08: closing asks while work is running; a killed export removes its partial file`

## Steps

### Step 1: Add two tests first

Add two private slots to `tests/cutcap_tests.cpp`, directly after `testCancelRemovesPartial`.

```cpp
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
```

The second slot, `testCloseAsksWhileExporting`, copies the window setup of
`testUnsavedChangesGuard` through `QVERIFY(QTest::qWaitForWindowExposed(window));` (use an 8.0 s
sample video), then:

```cpp
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
```

**Verify (both must FAIL now)**: full test command → `FAIL!` lines for
`testQuitDuringExportRemovesPartial` and `testCloseAsksWhileExporting`.

### Step 2: Remove the partial file when the app dies mid-export

In `Backend::~Backend()` (`src/backend.cpp`), inside the existing export branch, after
`m_exportProcess.waitForFinished(1000);`, add:

```cpp
        // Same rule as a cancelled export: a half-written file must not pass for a finished video.
        QFile::remove(m_exportOutputPath);
```

### Step 3: Ask before closing while work is running

In `src/qml/Main.qml`:

1. Next to `property bool allowClose: false` add:

```qml
    // Closing mid-export kills ffmpeg, and a recording or job in flight is lost, so those ask too.
    property bool allowBusyClose: false
    readonly property bool workRunning: Backend.exporting || Backend.recordingVoiceover || Backend.busy
```

2. Change `onClosing` so the running-work check comes first:

```qml
    onClosing: (close) => {
        if (workRunning && !allowBusyClose) {
            close.accepted = false
            busyCloseDialog.open()
            return
        }
        if (allowClose || !Backend.dirty)
            return
        close.accepted = false
        pendingDiscard = () => {
            win.allowClose = true
            win.close()
        }
        discardDialog.open()
    }
```

3. Add a second dialog directly after the closing brace of `discardDialog`, copying
   `discardDialog`'s properties (`modal`, `anchors.centerIn`, `width`, `padding`, the four
   `Material.*` lines and the `background` Rectangle) exactly. Use:
   - `id: busyCloseDialog`, `objectName: "busyCloseDialog"`, `title: "Still working"`
   - first `Text` (same styling as `discardDialog`'s first Text):
     `text: Backend.exporting ? "An export is still running." : (Backend.recordingVoiceover ? "A voiceover is still recording." : "A background job is still running.")`
   - second `Text` (same styling as `discardDialog`'s second Text):
     `text: "Quitting now stops it and discards its result."`
   - two `ActionButton`s in the `RowLayout`, styled like `discardCancelButton`:
     - `objectName: "busyKeepButton"`, `text: "Keep working"`, `highlighted: true`,
       `onClicked: busyCloseDialog.close()`
     - `objectName: "busyQuitButton"`, `text: "Quit anyway"`,
       `onClicked: { busyCloseDialog.close(); win.allowBusyClose = true; win.close() }`

   Put "Quit anyway" first and "Keep working" last (the safe action sits on the trailing edge, as
   "Save" does in `discardDialog`).

4. Add `|| busyCloseDialog.opened` to the `modalOpen` property so editing shortcuts stay blocked
   while this dialog is open. If `modalOpen` does not exist in the file, that is a STOP condition
   (plan 002 has not landed).

**Verify**: QML lint command → `0`. Full test command → 0 failed, exactly two more passing tests
than before this plan.

### Step 4: Prove the tests can fail

1. Temporarily remove the `QFile::remove(m_exportOutputPath);` line added in Step 2 → the full test
   command must show `FAIL!  : CutCapTests::testQuitDuringExportRemovesPartial`. Restore.
2. Temporarily change `if (workRunning && !allowBusyClose) {` to `if (false) {` → must show
   `FAIL!  : CutCapTests::testCloseAsksWhileExporting`. Restore.
3. Full test command → 0 failed.

### Step 5: Commit

Commit with the message from "Git workflow".

## Done criteria

- [ ] Step 1 showed both new tests failing before the fix
- [ ] Full test run has 0 failed and two more passing tests than before this plan
- [ ] QML lint command prints `0`
- [ ] `grep -c "busyCloseDialog.opened" src/qml/Main.qml` → `1`
- [ ] Step 4 showed each test failing when its fix was removed
- [ ] `git show --stat HEAD` lists only the three in-scope files

## STOP conditions

- `modalOpen` is not defined in `src/qml/Main.qml`.
- The export in a test finishes before the close happens (`QVERIFY2(...exporting()...)` fails).
  Report it; do not delete the assertion. (A longer sample, e.g. 20.0 s, is an allowed adjustment;
  say so in your notes.)
- `testUnsavedChangesGuard` starts failing after the `onClosing` change.
- The literal-size test `designTokensAreUsedAndConsistent` fails: the new dialog used a literal
  `font.pixelSize` or `radius` instead of a `Design.*` token.

## Maintenance notes

- Any new kind of long-running work must be added to `workRunning`.
- After "Quit anyway" the process can still take up to a few minutes to exit while a caption job's
  whisper process finishes; the window is gone but the process is not. Fixing that needs job
  cancellation in `MediaEngine`.
