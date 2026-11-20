# Plan 012: The preview does no pixel work while it cannot be seen, and loads stills off the GUI thread

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
- **Risk**: MED (touches the preview pipeline)
- **Depends on**: none
- **Category**: perf
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

`ClipFrameItem` (`src/clipframeitem.cpp`) is the Qt Quick item that applies a clip's look
(brightness, saturation, blur, chroma key) to each video frame on the CPU. Measured on the target
machine it costs 44–54 ms per 1400×788 frame, a full core during playback. Two avoidable costs:

1. It processes every frame even when nobody can see the result: when the item is hidden, and when
   the whole window is not being shown. On a tiling compositor the editor often sits on another
   workspace while it keeps playing; Qt marks such a window as not exposed, but the item keeps a
   core busy anyway.
2. For a still image with a look, `setImageSource` decodes the whole image file on the GUI thread
   (`QImage(path)`), and the QML resets the source every time playback leaves the clip, so every
   entry into an image clip freezes the UI for the length of the decode (a 24-megapixel JPEG takes
   a noticeable fraction of a second).

## Current state

`src/clipframeitem.cpp`:

```cpp
ClipFrameItem::ClipFrameItem(QQuickItem *parent) : QQuickItem(parent), m_sink(new QVideoSink(this)) {
    setFlag(ItemHasContents, true);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        // Frames queue up from the decoder thread; only the newest one is worth processing.
        if (!m_imageSource.isEmpty() || !frame.isValid() || frame != m_sink->videoFrame())
            return;
        setVideoFrame(frame);
    });
    connect(&m_watcher, &QFutureWatcher<QImage>::finished, this, [this] { ... });
    connect(this, &ClipFrameItem::lookChanged, this, &ClipFrameItem::scheduleProcessing);
}
```

```cpp
void ClipFrameItem::setImageSource(const QString &source) {
    if (source == m_imageSource)
        return;
    m_imageSource = source;
    emit imageSourceChanged();
    const QString path = source.startsWith("file:") ? QUrl(source).toLocalFile() : source;
    setSource(source.isEmpty() ? QImage() : QImage(path), false);
}
```

```cpp
void ClipFrameItem::scheduleProcessing() {
    if (m_watcher.isRunning()) {
        m_dirtyWhileBusy = true;
        return;
    }
    if (m_source.isNull() && !m_sourceFrame.isValid()) {
        ...
        return;
    }
    ...
    m_watcher.setFuture(QtConcurrent::run([source, frame, target, look, blur, referenceHeight]() mutable {
```

`setSource(const QImage &, bool)` and `setVideoFrame(const QVideoFrame &)` store the input and call
`scheduleProcessing()`. `hasFrame()` returns `!m_output.isNull()`. Private members are declared in
`src/clipframeitem.h` (`bool m_dirtyWhileBusy = false;`, `QFutureWatcher<QImage> m_watcher;`, ...).
The file already includes QtConcurrent and uses `QFutureWatcher`.

Existing coverage: `uiSmokeButtonsPipAndDrop` in `tests/cutcap_tests.cpp` renders the real window
and checks that a look change (saturation 0) turns the preview grey; `previewLookMatchesFfmpegFilters`
checks the static `ClipFrameItem::process`. Both must keep passing.

Repo rule (from `AGENTS.md`): "A test that passes for the wrong reason is a defect. Before trusting
a green run, confirm the assertion can fail: break the behaviour it covers, watch it fail, restore."

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |
| Build app | `./bin/build > /tmp/claude-1000/a-build.log 2>&1; echo $?` | `0` |

## Scope

**In scope** (the only files you may modify):
- `src/clipframeitem.h`
- `src/clipframeitem.cpp`
- `tests/cutcap_tests.cpp`

**Out of scope** (do NOT touch):
- `ClipFrameItem::process` and the look math: the preview must keep matching the export.
- `src/qml/PreviewLane.qml` and every other QML file.
- Moving the look to a GPU shader.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only the files you changed (`git add <files>`, not `git add -A`).
  Message: `A12: the preview skips pixel work while unseen and loads stills off the GUI thread`

## Steps

### Step 1: Test first

Add a private slot `hiddenPreviewDoesNoWork` to `tests/cutcap_tests.cpp`, directly after
`previewLookMatchesFfmpegFilters`:

```cpp
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
```

**Verify (must FAIL now)**: full test command → `FAIL!  : CutCapTests::hiddenPreviewDoesNoWork`
with the message `a hidden preview processed its source`.

### Step 2: Skip processing while unseen

In `src/clipframeitem.h` add a private member `bool m_pendingWhileUnseen = false;` and two private
declarations: `bool canBeSeen() const;` and `void resumeIfPending();`. Also declare the override
`void itemChange(ItemChange change, const ItemChangeData &data) override;` in the `protected:`
section next to `updatePaintNode`.

In `src/clipframeitem.cpp`:

```cpp
// Hidden, or in a window the compositor is not showing (another workspace, minimised): Qt reports
// such a window as not exposed and renders nothing, so processing frames for it is wasted work.
bool ClipFrameItem::canBeSeen() const {
    return isVisible() && (!window() || window()->isExposed());
}

void ClipFrameItem::resumeIfPending() {
    if (m_pendingWhileUnseen && canBeSeen()) {
        m_pendingWhileUnseen = false;
        scheduleProcessing();
    }
}

void ClipFrameItem::itemChange(ItemChange change, const ItemChangeData &data) {
    // A swapped frame is the sign the window is being shown again after it was not exposed.
    if (change == ItemSceneChange && data.window)
        connect(data.window, &QQuickWindow::frameSwapped, this, &ClipFrameItem::resumeIfPending,
                Qt::UniqueConnection);
    QQuickItem::itemChange(change, data);
}
```

At the very top of `scheduleProcessing()`, before the `m_watcher.isRunning()` check:

```cpp
    if (!canBeSeen()) {
        m_pendingWhileUnseen = true;
        return;
    }
```

In the constructor add:
`connect(this, &QQuickItem::visibleChanged, this, &ClipFrameItem::resumeIfPending);`

Make sure `<QQuickWindow>` is included in `src/clipframeitem.cpp`.

### Step 3: Decode stills in a worker

Replace the last two lines of `setImageSource` (the `path` line and the `setSource(...)` call) with:

```cpp
    if (source.isEmpty()) {
        setSource(QImage(), false);
        return;
    }
    // Decoding a large photo takes long enough to stall the UI; do it in a worker and take the
    // result only if this is still the image being shown.
    const QString path = source.startsWith("file:") ? QUrl(source).toLocalFile() : source;
    auto *loader = new QFutureWatcher<QImage>(this);
    connect(loader, &QFutureWatcher<QImage>::finished, this, [this, loader, source] {
        loader->deleteLater();
        if (source == m_imageSource)
            setSource(loader->result(), false);
    });
    loader->setFuture(QtConcurrent::run([path] { return QImage(path); }));
```

**Verify**: build command → `0`. Full test command → 0 failed, exactly one more passing test than
before this plan. `grep -n "QImage(path)" src/clipframeitem.cpp` → exactly one line, and it is the
one inside the `QtConcurrent::run` lambda.

### Step 4: Prove the test can fail

Temporarily remove the three-line `if (!canBeSeen())` block from `scheduleProcessing()` → the full
test command must show `FAIL!  : CutCapTests::hiddenPreviewDoesNoWork`. Restore → 0 failed.

### Step 5: Commit

Commit with the message from "Git workflow".

## Done criteria

- [ ] Step 1 showed the new test failing before the fix
- [ ] Full test run has 0 failed and one more passing test than before this plan
- [ ] `./bin/build` exits 0
- [ ] `grep -c "canBeSeen()" src/clipframeitem.cpp` → at least `3`
- [ ] Step 4 showed the test failing without the gate
- [ ] `git show --stat HEAD` lists only the three in-scope files (plus `cutcap.qmltypes` if the build changed it)

## STOP conditions

- `uiSmokeButtonsPipAndDrop` fails after Step 2 or Step 3 (its preview pixel checks did not see the
  processed frame). Report the failure message. Do not add waits to that test or change its
  thresholds; a preview that no longer updates when it becomes visible is a real regression.
- The new test passes before Step 2.
- `QQuickWindow::frameSwapped` cannot be connected as written (signature mismatch on this Qt).

## Maintenance notes

- The exposure gate relies on Qt's Wayland backend reporting a window on a hidden workspace as not
  exposed. It was reasoned from Qt's behaviour and covered here only for the hidden-item case; a
  manual check on Hyprland (play a clip with a look, switch workspace, watch CPU use) is worth doing.
- The look still runs on the CPU at roughly 20 fps for a large viewer. A shader is the real fix.
