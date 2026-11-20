# Plan 013: Filmstrip tiles and ruler ticks exist only for the visible part of the timeline

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
- **Depends on**: none
- **Category**: perf
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

`src/qml/TimelinePanel.qml` draws each video clip's filmstrip as one `Image` per tile across the
clip's whole width, and the time ruler and grid as one item per two seconds across the whole
timeline, whether or not they are on screen. A tile is about 75 px wide, so one 10-minute clip is
386 images at the default zoom and 1,929 at maximum zoom, and a 30-minute timeline has about 3,600
ruler and grid items, all rebuilt on every edit. The audio waveform in the same file already draws
only the visible slice; this plan gives the filmstrip, ruler and grid the same rule.

## Current state

All in `src/qml/TimelinePanel.qml`. The scrollable area is `Flickable { id: timelineFlick; objectName: "timelineFlick"`
with `contentWidth: root.timelineWidth`; `timelineFlick.contentX` is the scroll offset and
`timelineFlick.width` the visible width. `root.pxPerSec` is the zoom.

Grid (inside the Flickable's content):

```qml
                // Time grid, drawn behind the clips.
                Repeater {
                    model: Math.ceil(root.timelineWidth / (2 * root.pxPerSec))
                    delegate: Rectangle {
                        x: index * 2 * root.pxPerSec
                        y: 24
                        width: 1
                        height: timelineFlick.contentHeight - 24
                        color: Design.borderHairline
                        opacity: 0.5
                        z: -1
                    }
                }
```

Ruler:

```qml
                    // Time Ruler
                    Rectangle {
                        width: root.timelineWidth
                        height: 24
                        color: Design.surfaceSunken
                        border.color: Design.borderHairline
                        border.width: 1

                        Repeater {
                            model: Math.ceil(root.timelineWidth / (2 * root.pxPerSec))
                            delegate: Item {
                                x: index * 2 * root.pxPerSec
                                height: 24
                                Rectangle { width: 1; height: 8; anchors.bottom: parent.bottom; color: Design.borderStrong }
                                Text {
                                    // The playhead handle is 13 px wide and centred on t=0, so the
                                    // first tick needs to clear it; later ticks keep the hairline inset.
                                    x: index === 0 ? Design.s4 + Design.s1 : Design.s3
                                    y: 4
                                    text: Math.floor(index * 2 / 60) + ":" + String(index * 2 % 60).padStart(2, "0")
                                    ...
```

Filmstrip, inside the clip component (`clipBox` is the clip's Rectangle; `clipBox.x` is its position
in the Flickable's content):

```qml
            Row {
                id: strip
                objectName: "clipStrip"
                readonly property real tileWidth: height * 16 / 9
                anchors.fill: parent
                visible: clipBox.visual && (clipData.filmstripPath || "").length > 0
                Repeater {
                    model: strip.visible && strip.tileWidth > 0 ? Math.ceil(clipBox.width / strip.tileWidth) : 0
                    delegate: Image {
                        readonly property int frames: clipData.filmstripFrames || 1
                        readonly property int tileWidth: clipData.filmstripTileWidth || 1
                        readonly property int tileHeight: clipData.filmstripTileHeight || 1
                        readonly property int frameIndex: Math.max(0, Math.min(frames - 1,
                            Math.floor(clipBox.sourceTimeAt((index + 0.5) * strip.tileWidth) / clipBox.sourceDuration * frames)))
                        width: strip.tileWidth
                        height: strip.height
                        source: "file://" + clipData.filmstripPath
                        sourceClipRect: Qt.rect(frameIndex * tileWidth, 0, tileWidth, tileHeight)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: true
                    }
                }
            }
```

The exemplar for viewport culling is the waveform `Canvas { id: wave` directly below the strip:

```qml
                readonly property real viewLeft: timelineFlick.contentX - clipBox.x
                readonly property real firstX: Math.max(2, viewLeft)
                readonly property real lastX: Math.min(clipBox.width - 2, viewLeft + timelineFlick.width)
```

Existing test that touches the strip (`tests/cutcap_tests.cpp`, inside `uiSmokeButtonsPipAndDrop`):

```cpp
        auto firstVisible = [&](const QString &name, bool needsChildren = true) -> QQuickItem * {
            std::function<QQuickItem *(QQuickItem *)> walk = [&](QQuickItem *it) -> QQuickItem * {
                if (it->objectName() == name && it->isVisible() &&
                    (!needsChildren || it->childItems().size() > 0))
                    return it;
                ...
        auto *strip = firstVisible("clipStrip");
        QVERIFY2(strip, "video clips must show filmstrip frames");
        QVERIFY(strip->childItems().first()->property("source").toUrl().isLocalFile());
```

It runs with the timeline scrolled to `contentX` 400, so clips left of the viewport will have no
tiles after this plan. A `Repeater` is itself a child item of its parent, placed after its
delegates, so "has children" must become "has more than one child" (Step 1 does that).

Repo rules (from `AGENTS.md`): "A test that passes for the wrong reason is a defect. Before trusting
a green run, confirm the assertion can fail." and "A guard that inspects nothing is not a guard —
assert a minimum count alongside 'nothing is wrong'."

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |
| QML lint | `/usr/lib/qt6/bin/qmllint -I /usr/lib/qt6/qml src/qml/*.qml > /tmp/claude-1000/a-lint.log 2>&1; echo $?` | `0` |

## Scope

**In scope** (the only files you may modify):
- `src/qml/TimelinePanel.qml` (the grid Repeater, the ruler Repeater, the `strip` item)
- `tests/cutcap_tests.cpp`

**Out of scope** (do NOT touch):
- The waveform `Canvas`, clip dragging/trimming code, lane headers.
- `src/backend.cpp` and the clip model.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only the files you changed (`git add <files>`, not `git add -A`).
  Message: `A13: filmstrip tiles and ruler ticks are created for the visible viewport only`

## Steps

### Step 1: Test first

1. In `uiSmokeButtonsPipAndDrop`, change the `firstVisible` lambda's condition
   `it->childItems().size() > 0` to `it->childItems().size() > 1` and add the comment
   `// more than the Repeater itself: at least one tile`.

2. Add a private slot `timelineCreatesOnlyVisibleItems` directly after `testUnsavedChangesGuard`.
   Copy that test's window setup through `QVERIFY(QTest::qWaitForWindowExposed(window));`, with a
   600.0 second sample video (`makeSampleVideo(dir.path(), "long.mp4", "0x336699", 600.0)`; it
   encodes in a few seconds), then:

```cpp
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
```

   Check `Backend::setZoomPxPerSec` exists in `src/backend.h` and what the maximum zoom is (search
   `zoomPxPerSec` in `src/backend.cpp`); if the maximum is not 240, use the maximum and keep the
   `contentWidth > 100000` assertion honest by adjusting only the comment.

**Verify (must FAIL now)**: full test command →
`FAIL!  : CutCapTests::timelineCreatesOnlyVisibleItems` (the `timeRuler` / `timeGrid` items do not
exist yet, or the strip count is in the thousands).

### Step 2: Cull the grid and the ruler

Wrap the grid `Repeater` in an `Item { objectName: "timeGrid"; z: -1 ... }` that holds it, and give
the ruler `Rectangle` `objectName: "timeRuler"`. In both Repeaters create only the ticks in view:

```qml
                    // Only the ticks under the viewport exist; scrolling moves them, it does not
                    // create one item per two seconds of timeline.
                    readonly property real step: 2 * root.pxPerSec
                    readonly property int firstTick: Math.max(0, Math.floor(timelineFlick.contentX / step))
                    readonly property int tickCount: Math.min(
                        Math.ceil(root.timelineWidth / step) - firstTick,
                        Math.ceil(timelineFlick.width / step) + 2)
```

Put those three properties on the element that owns each Repeater (the new `timeGrid` Item and the
ruler Rectangle, with ids such as `grid` and `ruler`), set `model: Math.max(0, <owner>.tickCount)`,
and inside each delegate replace every use of `index` with a property
`readonly property int tick: <owner>.firstTick + index` (so `x: tick * <owner>.step`, the label
text uses `tick * 2`, and `index === 0` becomes `tick === 0`). The `timeGrid` Item must not change
how the grid looks: size it `width: root.timelineWidth; height: timelineFlick.contentHeight`, keep
each line's `y: 24`, height, colour and opacity, and move `z: -1` from the line to the Item.

### Step 3: Cull the filmstrip

Change `strip` from a `Row` to an `Item` (keep `id`, `objectName`, `tileWidth`, `anchors.fill` and
`visible`) and create only the tiles under the viewport:

```qml
                // Like the waveform: only the tiles under the viewport exist.
                readonly property int tileCount: tileWidth > 0 ? Math.ceil(clipBox.width / tileWidth) : 0
                readonly property real viewLeft: timelineFlick.contentX - clipBox.x
                readonly property int firstTile: Math.max(0, Math.min(tileCount, Math.floor(viewLeft / Math.max(1, tileWidth))))
                readonly property int lastTile: Math.max(firstTile, Math.min(tileCount,
                    Math.ceil((viewLeft + timelineFlick.width) / Math.max(1, tileWidth))))
                Repeater {
                    model: strip.visible ? strip.lastTile - strip.firstTile : 0
                    delegate: Image {
                        readonly property int tile: strip.firstTile + index
                        x: tile * strip.tileWidth
                        ...
```

In the delegate, `frameIndex` uses `(tile + 0.5) * strip.tileWidth` instead of
`(index + 0.5) * strip.tileWidth`. Everything else in the delegate stays.

**Verify**: QML lint command → `0`. Full test command → 0 failed, exactly one more passing test
than before this plan.

### Step 4: Prove the test can fail

Temporarily change the strip Repeater's model back to
`strip.visible ? strip.tileCount : 0` → the full test command must show
`FAIL!  : CutCapTests::timelineCreatesOnlyVisibleItems` with a "created ... items" message.
Restore → 0 failed.

### Step 5: Commit

Commit with the message from "Git workflow".

## Done criteria

- [ ] Step 1 showed the new test failing before the QML changed
- [ ] Full test run has 0 failed and one more passing test than before this plan
- [ ] QML lint command prints `0`
- [ ] `grep -c 'objectName: "timeRuler"\|objectName: "timeGrid"' src/qml/TimelinePanel.qml` → `2`
- [ ] Step 4 showed the test failing without the culling
- [ ] `git show --stat HEAD` lists only the two in-scope files

## STOP conditions

- The excerpts in "Current state" are not in `src/qml/TimelinePanel.qml`.
- `uiSmokeButtonsPipAndDrop` fails after the change in a way Step 1.1 does not cover (for example
  its pixel checks of the timeline). Report the failure message; do not alter other assertions.
- The 600-second sample video takes longer than the 20 s `makeSampleVideo` allows and the helper
  returns an empty path. Report it (do not change the helper); a 300-second sample with the
  `contentX` value lowered to 30000 is an allowed adjustment, say so in your notes.
- A grabbed frame shows the grid drawn over the clips (the `z` order changed).

## Maintenance notes

- Anything new drawn per unit of timeline length must follow the same viewport rule.
- The tiles are still separate `Image` items sharing one cached source; that is fine at one
  viewport's worth.
