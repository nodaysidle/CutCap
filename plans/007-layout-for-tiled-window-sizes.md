# Plan 007: The layout works at the sizes a tiling compositor hands out

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
- **Risk**: MED (changes the main window structure)
- **Depends on**: plans/006-tests-stay-in-temp-and-assert-state.md
- **Category**: bug (UX on tiling window managers)
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

CutCap runs on Hyprland, a tiling Wayland compositor. A tiled window gets whatever size the layout
gives it: the compositor ignores the window's minimum size (780×500), and Qt accepts the smaller
size (confirmed in the Wayland protocol log). On the target machine (2560×1440) common tiles are
1273×697, 2550×697, 848×466 and 640 px wide columns. What happens today:

- At 640 px wide the three panels' minimum widths (200 + 340 + 230) do not fit, and the inspector
  is cut off at the right window edge.
- The timeline has a fixed preferred height of 278 px and there is no splitter between it and the
  workspace above. At 848×466 the viewer picture is about 60 px tall; at 2550×697 it is about
  470×265 in a 1,930 px wide pane, and the user cannot trade timeline height for viewer height.

After this plan: the timeline height is draggable and starts proportional to the window; below
780 px wide the inspector hides, below 560 px the media bin hides too, so the viewer and timeline
always fit.

## Current state

`src/qml/Main.qml` is the application window (`ApplicationWindow { id: win`). Its body is one
`ColumnLayout` with: the top bar, a horizontal `SplitView`, a 1 px hairline `Rectangle`, and the
timeline. The part this plan changes (near the end of the file):

```qml
        // Main Workspace: Media Bin | Preview | Inspector
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            MediaBinPanel {
                objectName: "mediaBinPanel"
                onImportRequested: importDialog.open()
                SplitView.preferredWidth: 272
                SplitView.minimumWidth: 200
                SplitView.maximumWidth: 400
            }

            // 1px seams are the structure: no cards, no rounded panels.
            Rectangle {
                SplitView.preferredWidth: Design.gutter
                SplitView.fillHeight: true
                color: Design.borderHairline
            }

            PreviewPanel {
                objectName: "previewPanel"
                onImportRequested: importDialog.open()
                SplitView.fillWidth: true
                SplitView.minimumWidth: 340
            }

            Rectangle {
                SplitView.preferredWidth: Design.gutter
                SplitView.fillHeight: true
                color: Design.borderHairline
            }

            InspectorPanel {
                objectName: "inspectorPanel"
                onSrtImportRequested: importSrtDialog.open()
                onSrtExportRequested: exportSrtDialog.open()
                SplitView.preferredWidth: 320
                SplitView.minimumWidth: 230
                SplitView.maximumWidth: 440
            }
        }

        // Bottom Timeline
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Design.gutter
            color: Design.borderHairline
        }

        TimelinePanel {
            id: timelinePanel
            objectName: "timelinePanel"
            Layout.fillWidth: true
            Layout.preferredHeight: 278
            Layout.minimumHeight: 180
        }
    }
}
```

The window declares `minimumWidth: 780` and `minimumHeight: 500` near the top; leave those (they
are right for a floating window).

In the top bar (same file) there are, among others: two `ActionButton`s with `text: "Undo"` and
`text: "Redo"`, and a status `Text` with `id: topBarStatus`.

Other code refers to `timelinePanel` by id (`timelinePanel.dropUrlsAt(x, y, urls)`), and tests find
panels by `objectName` (`mediaBinPanel`, `previewPanel`, `inspectorPanel`, `timelinePanel`,
`topBar`, `previewCanvas`, `exportButton`). Keep every id and objectName.

Test patterns in `tests/cutcap_tests.cpp`:
- `testUnsavedChangesGuard` shows the window setup to copy (Theme, Design, Backend, registration,
  `QQmlApplicationEngine`, `QQuickWindow *window`, sample video import, `waitForImports`).
- `static QQuickItem *findItem(QQuickItem *root, const QString &objectName)` finds an item.
- `static void collectUnreadableButtons(QQuickItem *item, QStringList *problems, int *inspected)`
  appends a line to `problems` for every button whose label does not fit or (after plan 006) that
  has no area.

Design rules that apply (from `DESIGN.md`): panels are square and borderless and meet at 1 px
`Design.borderHairline` seams; sizes and colours come from `Design.*` tokens; no animations on
panels. Repo rule (from `AGENTS.md`): "A test that passes for the wrong reason is a defect. Before
trusting a green run, confirm the assertion can fail: break the behaviour it covers, watch it fail,
restore."

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |
| Failure detail | `grep -B2 -A12 '^FAIL' /tmp/claude-1000/a-test.log` | (for diagnosis) |
| QML lint | `/usr/lib/qt6/bin/qmllint -I /usr/lib/qt6/qml src/qml/*.qml > /tmp/claude-1000/a-lint.log 2>&1; echo $?` | `0` |

## Scope

**In scope** (the only files you may modify):
- `src/qml/Main.qml`
- `tests/cutcap_tests.cpp`
- `DESIGN.md` (append the short section in Step 4 only)

**Out of scope** (do NOT touch):
- `MediaBinPanel.qml`, `PreviewPanel.qml`, `InspectorPanel.qml`, `TimelinePanel.qml`: the panels
  themselves stay as they are; only how `Main.qml` arranges and shows them changes.
- `minimumWidth` / `minimumHeight` of the window.
- A user-facing toggle or shortcut for hiding panels: not part of this plan.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only the files you changed (`git add <files>`, not `git add -A`).
  Message: `A07: timeline height is draggable; side panels hide when a tiled window is too narrow`

## Steps

### Step 1: Add the test first

Add a private slot `layoutSurvivesTiledSizes` to `tests/cutcap_tests.cpp`, directly after
`testUnsavedChangesGuard`. Copy that test's setup through
`QVERIFY(QTest::qWaitForWindowExposed(window));`, import one 2.0 s sample video, wait for imports,
then:

```cpp
        struct Tile { int w; int h; bool bin; bool inspector; };
        // Sizes Hyprland produces on a 2560x1440 monitor, plus the documented floating minimum.
        const QList<Tile> tiles{{1273, 697, true, true}, {2550, 697, true, true}, {780, 500, true, true},
                                {848, 466, true, true},  {640, 697, true, false}, {520, 697, false, false}};
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
```

**Verify (the test must FAIL now)**: full test command → `FAIL!  : CutCapTests::layoutSurvivesTiledSizes`.
Note the failure message; it should name one of the sizes below 780 px wide or 848x466.

### Step 2: Hide side panels when the window is too narrow

In `src/qml/Main.qml`, next to the other window properties near the top (after `allowClose`), add:

```qml
    // A tiled window can be narrower than minimumWidth: the compositor ignores that hint. The
    // viewer and timeline are the editor; the side panels step aside when they do not fit.
    readonly property bool showInspector: width >= 780
    readonly property bool showMediaBin: width >= 560
```

In the horizontal `SplitView`: add `visible: win.showMediaBin` to `MediaBinPanel` and to the hairline
`Rectangle` directly after it; add `visible: win.showInspector` to `InspectorPanel` and to the
hairline `Rectangle` directly before it.

In the top bar: add `visible: win.width >= 560` to the `Undo` and `Redo` `ActionButton`s (they keep
their Ctrl+Z / Ctrl+Shift+Z shortcuts) and to the hairline `Rectangle` directly before `Undo`; add
`visible: win.width >= 780` to `topBarStatus`.

### Step 3: Make the timeline height draggable and proportional

Replace the horizontal `SplitView`, the hairline `Rectangle` under it and the `TimelinePanel` (the
whole excerpt in "Current state") with one vertical `SplitView` that contains the existing
horizontal `SplitView` and the `TimelinePanel`:

```qml
        // Workspace over timeline; the handle between them trades viewer height for timeline height.
        SplitView {
            objectName: "workspaceSplit"
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            // Main Workspace: Media Bin | Preview | Inspector
            SplitView {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 140
                orientation: Qt.Horizontal

                ... the five existing children, unchanged apart from Step 2 ...
            }

            TimelinePanel {
                id: timelinePanel
                objectName: "timelinePanel"
                // Starts at about a third of the window so a short tile still shows the picture.
                SplitView.preferredHeight: Math.max(180, Math.min(278, Math.round(win.height * 0.36)))
                SplitView.minimumHeight: 180
            }
        }
```

The inner `SplitView` loses its `Layout.fillWidth` / `Layout.fillHeight` lines (it is no longer a
direct child of the `ColumnLayout`), and the `TimelinePanel` loses its three `Layout.*` lines. The
"Bottom Timeline" hairline `Rectangle` is removed; the split handle replaces it.

**Verify**: QML lint command → `0`. Full test command → 0 failed, including the new slot, and
exactly one more passing test than before this plan.

### Step 4: Record the rule in `DESIGN.md`

Append to the end of section "## 3. Component rules" list in `DESIGN.md` one bullet:

```
- **Tiled sizes are first-class.** A tiling compositor ignores the window minimum (780 × 500). The
  timeline starts at about a third of the window height and its top edge is a split handle; below
  780 px wide the inspector hides, below 560 px the media bin, Undo/Redo and the status text hide
  too. `layoutSurvivesTiledSizes` audits 1273×697, 2550×697, 780×500, 848×466, 640×697 and 520×697.
```

### Step 5: Prove the test can fail

Temporarily change `readonly property bool showInspector: width >= 780` to `width >= 0`. Run the
full test command → `FAIL!  : CutCapTests::layoutSurvivesTiledSizes` must appear. Restore, run again
→ 0 failed.

### Step 6: Commit

Commit with the message from "Git workflow".

## Done criteria

- [ ] Step 1 showed the new test failing before the layout changed
- [ ] Full test run has 0 failed and one more passing test than before this plan
- [ ] QML lint command prints `0`
- [ ] `grep -c 'objectName: "workspaceSplit"' src/qml/Main.qml` → `1`
- [ ] `grep -c "Layout.preferredHeight: 278" src/qml/Main.qml` → `0`
- [ ] Step 5 showed the test failing when the rule was broken
- [ ] `git show --stat HEAD` lists only the three in-scope files

## STOP conditions

- The excerpt in "Current state" is not in `src/qml/Main.qml`.
- After Steps 2–3 the new test still fails at some size and the cause is inside one of the panel
  QML files (for example a panel whose own content cannot shrink to the width it is given), or the
  viewer picture stays under 100 px at 848×466. Report the size and the exact failure message. Do
  not lower the test's thresholds, remove a size, or edit a panel file.
- An existing test fails after the change (likely candidates: `uiSmokeButtonsPipAndDrop`,
  `fileDropLandsOnLaneUnderPointer`, `dropSnapsToNeighbourEdges`) and one fix attempt inside
  `Main.qml` does not resolve it. Report the failure message.
- `window->setGeometry` does not produce the requested size on the offscreen platform.

## Maintenance notes

- A new side panel or top-bar control must decide at which width it hides and be added to the test.
- There is no user override: someone who wants the inspector in a 700 px column cannot have it. A
  toggle with a shortcut is the natural follow-up.
- The timeline's proportional height is only its starting value; once the user drags the handle
  their choice holds until the app restarts (nothing is persisted yet).
