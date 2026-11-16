# CutCap — Handoff: what was done, what must be done

Free, 100 % local-first video editor for **Arch / Omarchy 4.5** on **Wayland + Hyprland**.
No account, watermark, telemetry or upload; all processing is local FFmpeg/Qt.

Verified on Qt **6.12.0**, FFmpeg **n9.0.2**, Hyprland **0.56.2**, PipeWire, Mesa/Intel Arc
(OpenGL 4.6), Wayland. `CutCap/` is not a git repository, so line numbers drift — trust symbol
names. Suite: **30 tests, all passing** (`./bin/test`).

---

## 1. Where things live

```text
cutcap.pro                 qmake6 project (Core Gui Qml Quick Controls2 Multimedia Concurrent DBus)
bin/build                  build → build/cutcap
bin/test                   build + run the offscreen QtTest suite
bin/install                makepkg -fsi (not exercised in this work)
pkgbuild/                  PKGBUILD, cutcap.desktop, cutcap.svg
src/
  main.cpp                 CLI vs GUI entry; registers ClipFrameItem + Design singleton
  cli.{h,cpp}              headless --json CLI
  project.{h,cpp}          MediaAsset / TimelineClip / TextOverlay / Keyframe / LaneState / Project
  mediaengine.{h,cpp}      ffprobe, thumbnails, filmstrips, waveforms, .ass, export filtergraph
  backend.{h,cpp}          state bridge: lanes, playback clock, snapping, undo, presets, QProcess
  clipframeitem.{h,cpp}    live look pipeline (export-matching eq/boxblur/vignette/chromakey)
  design.{h,cpp}           design tokens (QML singleton `Design`)
  theme.{h,cpp}            live Omarchy colors.toml + rounding; secondaryForeground contrast guard
  qml/                     Main, ActionButton, MediaBinPanel, PreviewPanel, PreviewLane,
                           TimelinePanel, InspectorPanel, ExportDialog, ShortcutsDialog
tests/cutcap_tests.cpp     the 30 tests
DESIGN.md                  design system: tokens, component rules, delivered structural work
AGENTS.md                  reporting + test rules for agents (read before editing)
```

Build / test / install:

```sh
./bin/build
./bin/test          # expect: Totals: 30 passed
install -Dm755 build/cutcap ~/.local/bin/cutcap
install -Dm644 pkgbuild/cutcap.desktop ~/.local/share/applications/cutcap.desktop
install -Dm644 pkgbuild/cutcap.svg ~/.local/share/icons/hicolor/scalable/apps/cutcap.svg
```

Menu: **Apps lists the `.desktop` entry automatically.** Do not add a static `apps.cutcap` row to
`~/.config/omarchy/extensions/omarchy-menu.jsonc` — that id collides with the generated one.
Verify: `omarchy-menu refresh`, then `uwsm-app -- gtk-launch cutcap.desktop`.

---

## 2. What was done

Each item names its code and the test or measurement that backs it. Every test marked *bite* was
confirmed to fail when its behaviour was removed.

### 2.1 Bugs you reported

| # | Fix | Where | Proof |
|---|---|---|---|
| 1 | Compact buttons rendered as blank grey pills. Replaced Material `Button` with `ActionButton` (height derives from the label + padding, 22 px floor). | `qml/ActionButton.qml`, all panels | `uiSmokeButtonsPipAndDrop` inspects ≥ 12 labelled controls, fails if a label exceeds its control (bite: `implicitHeight: 12`). |
| 2 | Strange colour: preview (Material `MultiEffect`) and export (FFmpeg `eq`/`boxblur`/`vignette`/`chromakey`) used different math, and the key sat on the wrong side of `eq`. One pipeline now runs FFmpeg's own formulas on decoded Y'CbCr, in export order, before the pad. | `clipframeitem.cpp::process`, `mediaengine.cpp::buildExportArgs` | `previewLookMatchesFfmpegFilters`: four looks through both, mean error < 6/255 per channel and in alpha (bite: swap filter order → red). |
| 3 | Placeholders: no thumbnails for stills (`-ss` into a single frame), no filmstrips, no waveforms for CLI projects. Added `ensureFilmstrip`, `preparePreviews`, backfill on open, 24-frame strips on timeline clips, waveforms on audio clips and audio cards. | `mediaengine.cpp`, `backend.cpp`, `TimelinePanel.qml`, `MediaBinPanel.qml` | `preparePreviewsFillsMissingPreviews` (bite: seek stills again → red); `uiSmokeButtonsPipAndDrop` finds a visible `clipStrip` with a local source and a visible `clipWave`. |
| 4 | Muted text failed AA (3.6–4.4:1 at 10–11 px). `Theme::secondaryForeground` pushes `dark_foreground` toward `foreground` until the **worst** panel surface clears 4.5:1. | `theme.cpp` | `secondaryTextMeetsContrastOnAllSurfaces` over dark + light palettes (bite: return the raw colour → red, reports 4.03:1). |
| 5 | Design was generic; tokens alone changed nothing visible. Composition rebuilt: flat borderless panels on 1 px hairline seams, 36 px command strip, media bin as a 64 px list with hover actions, segmented switch, one continuous inspector, 42 px lanes, square clips, draggable playhead. | `design.{h,cpp}`, all QML, `DESIGN.md` | 41 % of pixels differ at a fixed 1280×720 window (top bar 26 %, bin 24 %, timeline 16 %). |
| 6 | Clunky controls: no keyboard focus, no hover states, no typed values, no presets, tabs everywhere. | `ActionButton`, `ValueRow`, `LaneToggle`, `InspectorPanel` | `designTokensAreUsedAndConsistent` (no literal font sizes/radii), QML-error guard in the UI test. |

### 2.2 Timeline, editing, export

| Area | What it does now | Where | Proof |
|---|---|---|---|
| Playback | One `MediaPlayer` per lane; the playing lane is the clock, the 16 ms timer only covers gaps/images/text. Pause never seeks; the clock lane is never re-seeked while playing; paused frames decode without a play/pause kick. | `backend.cpp::clockLane`, `qml/PreviewLane.qml` | `uiSmokeButtonsPipAndDrop` (both lanes paused with valid frames); audio capture below. |
| Audio continuity | Verified by tapping the app's own PipeWire stream through a null sink (`pw-link` + `parec`) across a split and a V2 entry: 6.7 s of 440 Hz, window RMS within ±1 %, no dropouts, no discontinuities after stream start. | — | Re-run the procedure in §5. |
| V1 + V2 | V2 renders aspect-fitted at its keyframed x/y/scale above V1; inspector transforms edit the **selected** clip. | `qml/PreviewPanel.qml`, `PreviewLane.qml`, `Backend::selectedClipTransform` | Pixel assertions: teal V1, red PiP at 75 %/75 % at 40 %. |
| Drag & drop | Bin cards drag to lanes at the pointer with a ghost, filtered by media kind; external files land on the lane and time under the cursor; multi-file drops pack. Internal drags no longer swallowed by the file drop area. | `TimelinePanel.qml` (`TrackDropZone`, `dropTargetFor`), `Backend::importMediaAt` | `fileDropLandsOnLaneUnderPointer`; `uiSmokeButtonsPipAndDrop` synthesises `QDragEnter/Move/Drop` with `text/uri-list`. |
| Snapping | One rule (`Backend::snapTime`: neighbour edge, playhead or zero, 0.15 s, same lane) for drops **and** moves; live guide line while dragging (`previewSnap`/`snapGuideSec`). | `backend.cpp`, `TimelinePanel.qml` | `dropSnapsToNeighbourEdges`, `previewSnapExposesGuide` (bite each). |
| Scrolled drop hit-test | Hit test clamped to the Flickable viewport, so a drop over the track-label column no longer claims a lane. | `TimelinePanel.qml::dropTargetFor` | `uiSmokeButtonsPipAndDrop` (bite: delete the clamp → red). |
| Lane controls | Mute / solo / lock per lane, with state text. Mute and solo reach the export audio chain; lock refuses moves, trims, property edits, deletes. | `project.cpp::laneAudible`, `backend.cpp::setLaneState`, `TimelinePanel.qml` | `laneMuteAndSoloControlAudio`, `laneLockBlocksEdits`. |
| Transitions | V1 clips with `fade|dissolve|wipeleft|wiperight|slideleft|slideright` chain through `xfade`; timing preserved by extending the outgoing clip (`tpad` hold when no spare frames); audio crossfades with `afade` in/out. | `mediaengine.cpp` | `exportArgsChainXfadeForAdjacentTransition`, `exportRendersCrossfadeAtTimelinePosition` (real render: length, blend, clean neighbours). |
| Keyframes | x/y/scale/rotation animated on export through per-frame `perspective` expressions with the preview's smoothstep curve; animated opacity via `geq`. | `mediaengine.cpp::keyframeExpr/transformFilters` | `exportAnimatesOverlayKeyframes` (bar slides 25→75 % and turns 90°). |
| Undo | Continuous edits of one property merge into one step; presets are discrete steps. | `Backend::pushUndo(mergeKey)`, `applyLookPreset` | `sliderDragIsOneUndoStep`, `lookPresetAppliesAndUndoes`. |
| Values | `ValueRow`: label + numeric entry + reset + slider for speed/brightness/contrast/saturation/blur. Look presets Neutral/Punch/Mono/Warm/Soft. | `InspectorPanel.qml`, `Backend::applyLookPreset` | `lookPresetAppliesAndUndoes`. |
| Information architecture | Bin is assets-only; captions (language, auto-caption, `.srt` in/out) and voice (record, local TTS) are inspector sections; one inspector, no tabs; track headers carry the lane controls. | `MediaBinPanel.qml`, `InspectorPanel.qml`, `TimelinePanel.qml` | QML-error guard + suite. |
| Theme | All QML colours come from `Design` roles derived from the live Omarchy theme; a theme switch restyles without QML edits. | `design.cpp` | `secondaryTextMeetsContrastOnAllSurfaces`, `designTokensAreUsedAndConsistent`. |

### 2.3 Test and process guards added

- **QML-error guard**: `uiSmokeButtonsPipAndDrop` captures Qt messages while the window builds and
  fails on any `.qml:` error (bite: unregister the `Design` singleton → red).
- **Token guard**: `designTokensAreUsedAndConsistent` — no literal `font.pixelSize:`/`radius:` in
  QML, scales must ascend, primary text ≥ 7:1, secondary ≥ 4.5:1 (bite: it found six real
  bypasses on first run).
- **Vacuous-pass guard**: the button audit asserts ≥ 12 labelled controls were inspected.
- **Test-quality rules** are recorded in `AGENTS.md`: a green test must be shown to fail when its
  behaviour is broken; never assert on coordinates/counts the harness invented; read app-owned
  state (scroll offset, zoom) at the moment of the action.

### 2.4 Test inventory (30)

`themeReadsOmarchyColorsToml` · `designTokensAreUsedAndConsistent` ·
`secondaryTextMeetsContrastOnAllSurfaces` · `preparePreviewsFillsMissingPreviews` ·
`keyframeInterpolationSmoothstep` · `projectRoundTripAndSrtParsing` ·
`exportArgsContainFiltersAndSubtitles` · `backendSplitAndUndoRedo` ·
`exportArgsChainXfadeForAdjacentTransition` · `exportRendersCrossfadeAtTimelinePosition` ·
`previewLookMatchesFfmpegFilters` · `dropSnapsToNeighbourEdges` ·
`fileDropLandsOnLaneUnderPointer` · `laneMuteAndSoloControlAudio` · `laneLockBlocksEdits` ·
`previewSnapExposesGuide` · `lookPresetAppliesAndUndoes` · `sliderDragIsOneUndoStep` ·
`exportAnimatesOverlayKeyframes` · `uiSmokeButtonsPipAndDrop` ·
`multiSelectAdditiveAndRange` · `multiSelectDeleteIsOneUndoStep` ·
`multiSelectMovePreservesOffsetsAndClamps` · `multiSelectRippleDeleteShiftsPerLane` ·
`splitAtPlayheadSplitsWholeSelection` · `launcherArgumentsOpenNoPhantomFile` · `transitionBetweenAdjacentClips` · `audioWithCoverArtLandsOnA1` (+ init/cleanup).

### 2.5 Multi-select and the defects found while building it

| Item | What it does now | Where | Proof |
|---|---|---|---|
| Multi-select | `selectedClipIds` is an ordered list; `selectedClipId` stays the **primary** for the inspector. Click replaces, Ctrl+click toggles, Shift+click ranges over the anchor's lane, Ctrl+A selects all, Esc clears. The timeline draws every selected clip, and a headless click on a selected clip keeps the group. | `backend.cpp` selection block, `TimelinePanel.qml` drag body | `multiSelectAdditiveAndRange`; Ctrl+click and group-preserving click asserted through the real delegate in `uiSmokeButtonsPipAndDrop` (both bite). |
| Group move | Dragging any selected clip moves the whole selection by one delta, offsets preserved, clamped at 0, one undo step. Audio pins the track delta; a locked lane refuses the move. | `Backend::moveSelectedClips` | `multiSelectMovePreservesOffsetsAndClamps` (bites on the lock check). |
| Group delete | Deletes every selected clip on an unlocked lane in one undo step; ripple shifts each lane by the removed time before each survivor, read from original positions. | `Backend::deleteSelected` | `multiSelectDeleteIsOneUndoStep`, `multiSelectRippleDeleteShiftsPerLane` (both bite). |
| Group split | Splits every selected clip under the playhead in one undo step and leaves the right halves selected. | `Backend::splitAtPlayhead` | `splitAtPlayheadSplitsWholeSelection` (bites). |
| Lane-header defect | The header rows mapped `index` straight to a backend track while `lanesList()` returns `V1, V2, A1`, so V2 toggled V1, T1 toggled A1 audio and the A1 row was dead. Rows now carry an explicit track; the text row has no M/S/L. | `TimelinePanel.qml` header Repeater | Clicking the V2 mute toggles lane 1 only (bites when reverted to `index`). |
| Lane-header geometry | The header was 104 px: a 59 px lane name and a 58 px M/S/L strip cannot share that, so the controls sat on top of the name. The header is 148 px, the name column reserves the strip's width and elides, and M/S/L occupy a fixed column so they align on every row. A1 keeps its solo button. | `TimelinePanel.qml` header delegate | Each lane name renders at full width and ends left of the toggles (bites at 104 px: "squeezed to 28 of 59 px"). |
| Transition junction | Where two clips touch on one lane, a chip sits on the seam. Its menu sets the transition that carries the left clip into the right one (none, fade, dissolve, wipe or slide), length capped at half the shorter clip. The exporter already chains these through `xfade`. | `Backend::transitionPoints`/`setTransitionBetween`, `TimelinePanel.qml` `TransitionJunctions` | `transitionBetweenAdjacentClips` (export contains `xfade`/`dissolve`, gap and locked lane refuse); UI test opens the chip's menu, triggers Dissolve and reads it back. |
| Media card overlap | The name and metadata ran underneath a hover action strip pinned to the right. The metadata line and the actions now share one fixed-height slot: hovering swaps one for the other, so nothing is ever drawn on top of the name. | `MediaBinPanel.qml` | Hover state forced in the UI test: metadata and actions are never both visible, the name never intersects the strip (bites when the hover visibility is removed). |
| Audio on A1 | An MP3/FLAC/M4A reports its cover art as a `video` stream, and `probeMedia` treated any video stream as a picture track. The file became a "video" asset with the artwork's 768×768, so it was placed on V1, could not go to A1, and showed `Video · 3:12 · 768×768` in the bin. Streams with `disposition.attached_pic` are now skipped. | `mediaengine.cpp:104-118` | `audioWithCoverArtLandsOnA1`: a generated MP3 with cover art probes as `audio`, imports onto track 2, and accepts an explicit A1 placement (bites when the attached_pic check is removed). |
| Desktop launch | `Exec=cutcap open %F` expands `%F` to nothing, and the bare `open` was taken as a filename, so starting from the Apps menu showed "File not found: open". Argument parsing is now `Cli::guiFiles`, and `cutcap open` with no file opens an empty project. | `main.cpp`, `cli.cpp` | `launcherArgumentsOpenNoPhantomFile`. |
| Inspector defect | The clip inspector put three sibling `ColumnLayout`s inside one `ScrollView`, which lays every child at the same origin — sections drew on top of each other. They now stack in one `Column`. | `InspectorPanel.qml` | Section y-positions are strictly sequential and total ≥ 700 px (bites when the container is a `Row`). |

---

## 3. What must be done

Ordered by user impact. Nothing below is implemented; do not claim it is.

### 3.1 Editing that is still missing
1. **Ripple / roll / slip edits** — only `Shift+Del` ripples. No ripple-on-drag, no roll between
   neighbours, no slip inside a clip.
2. **Keyboard-only placement** — a clip can only be put on a lane with the mouse or the bin's
   `→ V1/V2/A1` actions; there is no focusable timeline cursor, no `[`/`]` nudging, no
   keyboard trimming.
3. **Track collapse / add-track / reorder** — lanes are the fixed four (`V1`, `V2`, `T1`, `A1`).
4. **Numeric entry for the transform rows** — `ValueRow` covers speed and colour; X/Y/scale/
   rotation/opacity still use label + slider.

### 3.2 Fidelity and export
6. **Keyframed grading** — colour looks are per-clip constants; only transform + opacity animate.
7. **Rotation in the `.ass` stage** — captions are burned at a fixed position/size; no per-caption
   transforms, no drag-to-position after export differs from preview at extreme zoom (preview
   processes at display size).
8. **Export progress** is parsed from FFmpeg stderr with one regex; `.srt`/`.ass` styling wins are
   limited to what the ASS writer emits. No two-pass or hardware encoder discovery.
9. **CLI parity** for what the GUI gained: no `set-lane`, no `preset`, no `--snap`; `transcode`
   options are the same three presets.

### 3.3 Verification that is missing
10. **Real desktop drag-and-drop** — covered only with synthesised `QDrag*Event`s; never exercised
    through a compositor handoff.
11. **Whisper / TTS / `rembg` success paths** — only the "not installed" messages are tested.
12. **`bin/install` (makepkg)** — never run; installation was manual `install -Dm755`.
13. **Other Omarchy themes and HiDPI** — one dark theme at 1× was exercised; the contrast guard is
    palette-parametric but rendering at 2× is unverified.
14. **Long-project performance** — every timeline clip builds a filmstrip `Image` row and a waveform
    `Repeater` over its width; no virtualisation, no cap. A 200-clip timeline will be slow. Measure
    before optimising, then likely: shared texture atlas, tiles only for the visible viewport.

### 3.4 Product/UX still open (from DESIGN.md §4)
15. Inspector is long; needs collapsible sections and a "reset all looks" action.
16. No first-run/sample project; empty state is a single import prompt.
17. No autosave or crash recovery (only dirty-flag + Ctrl+S).
18. Export dialog is three combos and a progress bar; no estimate, no codec capability check.

---

## 3.5 UI: what still looks wrong — this is the next agent's job

**Read this before claiming the UI is done.** Three overlap defects were shipped twice because
each was fixed in isolation, declared finished, and the rest of the app was never audited:

| Defect | Where | State |
|---|---|---|
| Lane name ran under the M/S/L toggles | `qml/TimelinePanel.qml:12,172-215` | fixed, guarded |
| Clip inspector drew three sections on top of each other | `qml/InspectorPanel.qml:259` | fixed, guarded |
| Media card name ran under the hover action strip | `qml/MediaBinPanel.qml:94-215` | fixed, guarded |

The class of bug is always the same: **an element positioned absolutely (`anchors.right`,
`anchors.fill`, a floating `Row`) drawn over content that never reserved its space.** Grep for
`anchors.right: parent.right` and compare with what sits under it.

These are *not* fixed. Each needs a redesign decision, not a patch:

1. **Top bar** — `qml/Main.qml:300-330`. The free-form `backend.statusMessage` shares one row with
   the title field (fixed 190 px), the file actions and Export. Long messages
   (`dissolve into desktop-drop.mp4 (0.5 s)`) crowd the buttons at 900 px wide. Decide: fixed-width
   status with elide, or move it to the toast. Done = at 900 px and 1600 px nothing is clipped and
   the title keeps ≥ 120 px.
2. **Media bin metadata at narrow widths** — `qml/MediaBinPanel.qml:176`. The meta line elides to
   `Video · 0:03 · 320…`. Drop the resolution before the kind and duration, and put the full
   filename in a tooltip. Done = at 240 px the card still shows kind and duration.
3. **Inspector** — `qml/InspectorPanel.qml:259`. One continuous scroll of ~20 controls, no
   collapsible sections, no scroll affordance. In a 600 px-tall window the user sees Brightness and
   cannot tell there is more below. This is why §3.4 item 15 exists. Done = collapsible sections,
   a visible scroll indication, and no control clipped at the panel edge.
4. **Export dialog** — `qml/ExportDialog.qml`. Three combos and a progress bar: no duration
   estimate, no codec capability check, resolution derived from `currentIndex` instead of the model
   value. Needs the density rules from `DESIGN.md` applied, not a patch.
5. **Dialogs have never been rendered.** `qml/ExportDialog.qml` and `qml/ShortcutsDialog.qml` are
   only ever loaded closed; `uiSmokeButtonsPipAndDrop` never opens them. Open both, screenshot,
   inspect. Assume they are wrong until seen.
6. **Timeline at small heights** — at 900×600 the timeline is ~150 px for a ruler plus four lanes,
   and there is no horizontal scroll affordance even when clips run past the edge.

### How to audit (do not skip)

- Screenshot the real window at **1280×800 and 900×600**, in each of: empty project, one clip,
  three clips with long filenames, an audio clip on A1, a text overlay selected, and with each
  dialog open.
- Any two visible `Text` or control rectangles that intersect are a defect. The only intentional
  overlaps are: a clip's name chip over its filmstrip, text overlays over the preview canvas, and
  tooltips.
- Guard each fix the way the three above are guarded: give the parts `objectName`s, force the state
  in `uiSmokeButtonsPipAndDrop` (hover was forced for the media card), compare `mapToScene` rects,
  and assert the text is not squeezed (`width >= implicitWidth`). Then break the fix and watch the
  guard fail before trusting it.

### Audio on the A1 lane

Fixed: `probeMedia` skipped `attached_pic` cover art, so an MP3 became a `video` asset with the
artwork's dimensions, landed on V1, and refused A1. Verified with a generated MP3 carrying cover
art (`audioWithCoverArtLandsOnA1`). If a user still cannot put audio on A1, check
`MediaAsset::kind` first with `cutcap probe <file> --json` — a wrong `kind` is what breaks
placement, not the drop target.

---

## 4. Known limitations (accurate as of this handoff)

- Preview processes looks at displayed canvas size; blur is resampled at extreme zoom (export is
  always full resolution).
- The preview draws video through the RHI scene graph; Qt's software renderer is not supported
  (forcing it on Wayland misrenders the whole window). The UI test fails rather than skipping.
- Overlaps on one lane are allowed and composite in start order; no auto-ripple.
- Waveforms are 80 buckets over the whole file; long clips look coarse.
- `acrossfade` is deliberately not used (it shortens the timeline and desyncs captions/overlays).
- Lane mute/solo affect audio only; muted video lanes still render.

---

## 5. Audit procedure

```sh
cd CutCap
./bin/test                                                        # Totals: 30 passed
QT_QPA_PLATFORM=offscreen build/tests/cutcap_tests -v1 | grep -c '^PASS'   # 30
~/.local/bin/cutcap capabilities --json | jq -c .                 # ffmpeg/ffprobe true

# bite check (pick any test): break the behaviour, watch it go red, restore
cp src/backend.cpp /tmp/b.bak
sed -i 's/const bool merge = !mergeKey.isEmpty()/const bool merge = false/' src/backend.cpp
(cd build/tests && make -s && QT_QPA_PLATFORM=offscreen ./cutcap_tests sliderDragIsOneUndoStep | grep -c '^FAIL')
cp /tmp/b.bak src/backend.cpp

# real-display sanity: open a project, play 2 s, measure the preview region
~/.local/bin/cutcap open /tmp/project.cutcap & sleep 3
grim -g "$(hyprctl clients -j | jq -r '.[]|select(.class=="cutcap")|"\(.at[0]),\(.at[1]) \(.size[0])x\(.size[1])"')" /tmp/shot.png
# expect: preview region has thousands of unique colours (not a black rectangle)
```

Audio continuity (optional, needs PipeWire):

```sh
MOD=$(pactl load-module module-null-sink sink_name=cutcap_verify rate=48000 channels=1)
# start the app, play, then link the new cutcap node's output ports into the null sink and record:
pw-link <cutcap-node>:output_1 <sink-input-port>
parec -d cutcap_verify.monitor --format=float32le --rate=48000 --channels=1 --raw > cap.raw
pactl unload-module $MOD
# analyse: 10 ms RMS windows — no window < 50 % of the median, no second-difference spikes
```
