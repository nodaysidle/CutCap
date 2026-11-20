# Plan 002: Editing shortcuts stop at modal dialogs, and Space activates a keyboard-focused control

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
- **Category**: bug / accessibility
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

CutCap is a Qt 6.12 / QML video editor. Its keyboard shortcuts are declared with
`context: Qt.ApplicationShortcut`, and Qt does not block application shortcuts while a modal popup
is open. Reproduced: with the Export dialog open, pressing `S` split the clip underneath it, and
`Delete` deletes the selection. Second problem: the `Space` shortcut (play/pause) always wins over
the focused control, so a keyboard user who tabs to a button and presses Space starts playback
instead of activating the button, although `DESIGN.md` promises "Space/Return activation". After
this plan, no editing shortcut fires while a dialog is open, and Space activates a control that
received focus from the keyboard while staying play/pause for everyone else.

## Current state

### `src/qml/Main.qml` (the application window, id `win`)

Near the top:

```qml
    readonly property bool isTyping: activeFocusItem !== null &&
        (activeFocusItem.toString().indexOf("TextInput") !== -1 ||
         activeFocusItem.toString().indexOf("TextEdit") !== -1 ||
         activeFocusItem.toString().indexOf("TextArea") !== -1 ||
         activeFocusItem.toString().indexOf("TextField") !== -1)
```

The window owns these dialogs (ids): `importDialog`, `saveProjectDialog`, `openProjectDialog`,
`importSrtDialog`, `exportSrtDialog` (all `FileDialog`, which has `visible` but no `opened`),
`exportDialog` (`ExportDialog`, objectName `"exportDialog"`), `shortcutsDialog`
(`ShortcutsDialog`) and `discardDialog` (`Dialog`, objectName `"discardDialog"`). The last three
are Qt Quick popups and have an `opened` property.

The shortcuts, in file order, with their current `enabled` line:

| Sequence | Current `enabled` | Action |
|---|---|---|
| `Ctrl+A` | `!win.isTyping` | select all clips |
| `Escape` | `!win.isTyping && !exportDialog.opened && !shortcutsDialog.opened && !importDialog.visible && !saveProjectDialog.visible && !openProjectDialog.visible && !importSrtDialog.visible && !exportSrtDialog.visible` | clear selection |
| `Space` | `!win.isTyping` | play/pause |
| `S` | `!win.isTyping` | split at playhead |
| `Ctrl+B` | (none) | split at playhead |
| `Delete` | `!win.isTyping` | delete selection |
| `Backspace` | `!win.isTyping` | delete selection |
| `Shift+Delete` | `!win.isTyping` | ripple delete |
| `StandardKey.Undo` | (none) | undo |
| `StandardKey.Redo` | (none) | redo |
| `Ctrl+O` | (none) | open import dialog |
| `Ctrl+S` | (none) | save |
| `Ctrl+E` | (none) | open export dialog |
| `Left` | `!win.isTyping` | playhead -0.1 s |
| `Right` | `!win.isTyping` | playhead +0.1 s |
| `?` | `!win.isTyping` | open shortcuts dialog |
| `Ctrl+Q` | (none) | `win.close()` |

Example of one as written today:

```qml
    Shortcut {
        sequence: "S"
        enabled: !win.isTyping
        context: Qt.ApplicationShortcut
        onActivated: Backend.splitAtPlayhead()
    }
```

### `src/qml/ActionButton.qml` (the only button component; root is `Rectangle { id: root`)

```qml
    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus()
            root.clicked()
        }
    }

    Keys.onSpacePressed: root.clicked()
    Keys.onReturnPressed: root.clicked()
```

### Four more custom controls with the same shape

Each is an item with `activeFocusOnTab: true`, a `MouseArea` whose `onClicked` calls
`<item>.forceActiveFocus()`, and a `Keys.onSpacePressed` handler:

- `src/qml/TimelinePanel.qml`: the lane toggle component (item id `toggle`,
  `Keys.onSpacePressed: toggle.toggled()`), and the transition chip
  (`Keys.onSpacePressed: chipMenu.open()`, its MouseArea calls `parent.forceActiveFocus()`).
- `src/qml/InspectorPanel.qml`: the colour swatch (item id `swatch`,
  `Keys.onSpacePressed: swatch.picked()`), and the section header (item id `headerBar`,
  `Keys.onSpacePressed: section.expanded = !section.expanded`, its MouseArea calls
  `headerBar.forceActiveFocus()`).

`grep -n "Keys.onSpacePressed" src/qml/*.qml` lists exactly these five places.

### Test pattern to copy

`tests/cutcap_tests.cpp`, slot `testUnsavedChangesGuard` (search for
`void testUnsavedChangesGuard()`), shows how a test builds the real window: it creates `Theme`,
`Design`, `Backend`, registers them, loads `qrc:/qml/Main.qml` into a `QQmlApplicationEngine`, casts
the root object to `QQuickWindow *window`, imports a sample video made by
`makeSampleVideo(dir.path(), "dirty.mp4", "0x336699", 1.0)` and waits with `waitForImports(backend)`.
Copy that setup block verbatim for the new test.

Repo rules that apply (from `AGENTS.md`): "A test that passes for the wrong reason is a defect.
Before trusting a green run, confirm the assertion can fail: break the behaviour it covers, watch it
fail, restore." No new dependencies. QML uses 4-space indentation and `Design.*` tokens; do not add
literal colours or font sizes.

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |
| QML lint | `/usr/lib/qt6/bin/qmllint -I /usr/lib/qt6/qml src/qml/*.qml > /tmp/claude-1000/a-lint.log 2>&1; echo $?` | `0` |

The full suite takes about 40 s. QML is compiled into the test binary through `src/resources.qrc`,
so `./bin/test` picks up QML edits.

## Scope

**In scope** (the only files you may modify):
- `src/qml/Main.qml`
- `src/qml/ActionButton.qml`
- `src/qml/TimelinePanel.qml` (only the two controls named above)
- `src/qml/InspectorPanel.qml` (only the two controls named above)
- `tests/cutcap_tests.cpp`

**Out of scope** (do NOT touch):
- `src/qml/ExportDialog.qml`, `src/qml/ShortcutsDialog.qml`: the guard lives in `Main.qml`.
- The shortcut key sequences themselves and `ShortcutsDialog`'s list of them.
- Any C++ file under `src/`.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only the files you changed (`git add <files>`, not `git add -A`).
  Message: `A02: shortcuts stop at modal dialogs; Space activates a keyboard-focused control`

## Steps

### Step 1: One rule for "a dialog is open"

In `src/qml/Main.qml`, directly below the `isTyping` property, add:

```qml
    // Qt does not block application shortcuts while a modal popup or a native file dialog is
    // open, so the editing shortcuts check for one themselves.
    readonly property bool modalOpen: exportDialog.opened || shortcutsDialog.opened || discardDialog.opened ||
        importDialog.visible || saveProjectDialog.visible || openProjectDialog.visible ||
        importSrtDialog.visible || exportSrtDialog.visible
    readonly property bool keysFree: !isTyping && !modalOpen
```

Then set the `enabled` line of each shortcut as follows (add the line where there is none):

- `enabled: win.keysFree` for: `Ctrl+A`, `Escape`, `Space`, `S`, `Delete`, `Backspace`,
  `Shift+Delete`, `Left`, `Right`, `?`. For `Escape` this replaces the whole long expression.
- `enabled: !win.modalOpen` for: `Ctrl+B`, `StandardKey.Undo`, `StandardKey.Redo`, `Ctrl+O`,
  `Ctrl+S`, `Ctrl+E`.
- `Ctrl+Q`: leave unchanged.

**Verify**: `grep -c "enabled: win.keysFree" src/qml/Main.qml` → `10`;
`grep -c "enabled: !win.modalOpen" src/qml/Main.qml` → `6`;
`grep -c "enabled: !win.isTyping" src/qml/Main.qml` → `0`.

### Step 2: Space activates a control only when the keyboard put focus there

In `src/qml/ActionButton.qml`:

1. Add to the root item, next to the other properties:

```qml
    // Space is play/pause everywhere, except on a control the keyboard moved focus to. A click
    // also focuses the control, and must not turn Space into "press this button again".
    property bool focusFromMouse: false
    onFocusChanged: if (!focus) focusFromMouse = false
    Keys.onShortcutOverride: (event) => {
        if (event.key === Qt.Key_Space && !root.focusFromMouse)
            event.accepted = true
    }
```

   Use `onFocusChanged` (the `focus` property), not `onActiveFocusChanged`: when a button opens a
   popup the button loses `activeFocus` but keeps `focus`, and the flag must survive that.

2. In the MouseArea's `onClicked`, set the flag before taking focus:

```qml
        onClicked: {
            root.focusFromMouse = true
            root.forceActiveFocus()
            root.clicked()
        }
```

Apply the same three changes (the `focusFromMouse` property, the `onFocusChanged` reset, the
`Keys.onShortcutOverride` handler, and setting the flag in the MouseArea's `onClicked` before the
existing `forceActiveFocus()` call) to the four other controls listed in "Current state". In each,
replace `root` with that control's own id (`toggle`, `swatch`, `headerBar`); the transition chip has
no id, so give its Rectangle `id: chip` and use that (its MouseArea currently says
`parent.forceActiveFocus()`; keep that call and add `chip.focusFromMouse = true` before it).

**Verify**: `grep -c "Keys.onShortcutOverride" src/qml/ActionButton.qml src/qml/TimelinePanel.qml src/qml/InspectorPanel.qml`
→ `ActionButton.qml:1`, `TimelinePanel.qml:2`, `InspectorPanel.qml:2`. QML lint command → `0`.

### Step 3: Add the test

Add a new private slot `testShortcutsRespectModalsAndFocus` to `tests/cutcap_tests.cpp`, directly
after `testUnsavedChangesGuard`. Copy that test's setup through `QVERIFY(QTest::qWaitForWindowExposed(window));`,
using a 2.0 second sample video, then:

```cpp
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
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, centre);
        QTRY_VERIFY(exportDialog->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(exportDialog, "close"));
        QTRY_VERIFY(!exportDialog->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY2(backend.playing(), "Space after a mouse click must still play");
        QVERIFY(!exportDialog->property("opened").toBool());
        backend.setPlaying(false);
```

**Verify**: full test command → `Totals: 42 passed, 0 failed` and no `FAIL` line.

### Step 4: Prove the test can fail

1. In `src/qml/Main.qml` temporarily change the `S` shortcut back to `enabled: !win.isTyping`. Run
   the full test command → a `FAIL!  : CutCapTests::testShortcutsRespectModalsAndFocus` line must
   appear. Restore the line.
2. In `src/qml/ActionButton.qml` temporarily delete the `Keys.onShortcutOverride` handler. Run the
   full test command → the same test must fail. Restore the handler.
3. Run the full test command again → `Totals: 42 passed, 0 failed`.

### Step 5: Commit

Commit with the message from "Git workflow". `git status --short` must then show only `?? plans/`
or nothing.

## Done criteria

- [ ] Full test run prints `Totals: 42 passed, 0 failed`
- [ ] QML lint command prints `0`
- [ ] `grep -c "enabled: !win.isTyping" src/qml/Main.qml` → `0`
- [ ] `grep -c "Keys.onShortcutOverride" src/qml/*.qml` totals 5
- [ ] Step 4 showed the new test failing both ways
- [ ] `git show --stat HEAD` lists only in-scope files

## STOP conditions

- The quoted code in "Current state" is not in the files, or `grep -n "Keys.onSpacePressed" src/qml/*.qml`
  does not list exactly five places.
- The new test's keyboard-focus part or mouse part still fails after the QML changes are in place
  and one careful fix attempt. Report which assertion fails and its message; do not weaken the
  assertion or delete that part of the test.
- `QTest::qWaitForWindowActive(window)` fails on the offscreen platform.
- Any existing test starts failing and one fix attempt does not resolve it.

## Maintenance notes

- Every new dialog owned by `Main.qml` must be added to `modalOpen`, or its keys leak to the timeline.
- Every new custom control with `Keys.onSpacePressed` needs the same `focusFromMouse` pattern.
- Not covered here: arrow keys on a focused slider still move the playhead (the `Left`/`Right`
  shortcuts win). That needs its own decision about slider focus.
