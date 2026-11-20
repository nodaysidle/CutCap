# CutCap design system

One page. Everything the UI is allowed to use, and the rules that keep it from drifting back
into per-file magic numbers.

Enforced by two tests, not by good intentions:

- `designTokensAreUsedAndConsistent` — QML may not contain a literal `font.pixelSize: <n>` or
  `radius: <n>`; the scales must ascend; primary text ≥ 7:1, secondary ≥ 4.5:1.
- `uiSmokeButtonsPipAndDrop` — no QML `ReferenceError` / `TypeError` is allowed to occur while
  the window builds, and every labelled control must fit its label.

Both were checked against a deliberate break before being trusted.

---

## 1. Shape of the app

**Panel anatomy** (after Descript/CapCut's desktop editors and Resolve's panel-focus model):
media bin left, viewer centre, inspector right, timeline across the bottom, one top bar. That
structure is correct for the job — cutting clips — and is not the thing to redesign. What was
missing was the layer underneath it.

**Density target**: a desktop creative tool, not a mobile app. Body text 12–14 px, rows 24–34 px,
controls 24–38 px tall. Reference: Linear/Raycast-style density for panels, CapCut's clip-first
timeline for the bottom strip, Resolve for "one panel owns one job". Every pointer target is at
least 24 × 24 px (WCAG 2.2 target-size minimum); dense controls earn that size through good
internal spacing rather than shrinking below the floor.

**Accent budget**: the accent colour is spent on exactly four things — Play, the primary action,
the focus ring, the playhead. Lane identity uses the theme's lane hues (V1 blue, V2 magenta,
T1 yellow, A1 green) and nothing else is coloured.

**Depth**: structure comes from hairlines and surface steps, never from shadows or glow. There is
one elevation: dialogs/popovers on `surfaceOverlay`.

**Motion**: hover and press only, 90 ms / 140 ms, `OutQuad`. No loops, no bounce, no reveal
animations on panels.

---

## 2. Tokens

`src/design.{h,cpp}` — a QML singleton (`import CutCap`, `Design.*`), colour roles derived from the
live Omarchy theme so a theme switch restyles the app with no QML change.

### Space (px, 2 px hairline step, 4 px base, 8 px rhythm)

| Token | px | Use |
|---|---|---|
| `s1` | 2 | hairline insets, bar gaps |
| `s2` | 4 | inner control padding, tight stacks |
| `s3` | 6 | chips, small gaps |
| `s4` | 8 | default gap between controls |
| `s5` | 12 | panel inner padding |
| `s6` | 16 | section separation |
| `s7` | 24 | panel margin |
| `s8` | 32 | window margin |

### Type

| Token | px | Weight | Use |
|---|---|---|---|
| `tMicro` | 12 | 500 | ruler, kbd badges |
| `tCaption` | 12 | 500 | section labels, metadata, buttons |
| `tBody` | 13 | 400 | default text, list rows |
| `tBodyLg` | 14 | 400/600 | values, clip names, panel title |
| `tTitle` | 18 | 600 | dialog title |
| `tDisplay` | 24 | 600 | empty-state headline |

Weights: `wRegular` 400, `wMedium` 500, `wSemibold` 600. No bold-as-structure: use a label tier
and a hairline instead.

Typography: `Adwaita Sans` carries the UI; `JetBrains Mono` carries timecode, ruler labels, numeric
fields, shortcuts and technical status. The mono face makes changing values easy to scan without
turning the whole interface into a terminal. The UI family must ship real 500 and 600 weights; Nimbus
Sans does not, and Qt substituted its italic faces.

### Controls

| Token | px | Use |
|---|---|---|
| `hCompact` | 24 | icon buttons, lane toggles, swatches |
| `hSmall` | 28 | compact buttons, sliders |
| `hStd` | 32 | standard buttons, inputs |
| `hLarge` | 38 | primary action, dialog buttons |
| `rowH` | 34 | media-bin control rows |
| `trackH` | 48 | timeline lane height |
| `topBarH` | 44 | panel headers, top strip |
| `gutter` | 1 | the only gap between panels (a hairline seam) |

### Radii (clamped by the theme's own rounding)

`rXs` 2 (chips, swatches, clips) · `rSm` 3 (inputs, thumbnails) · `rMd` 4 (buttons) · `rLg` 6
(dialogs). **Panels are square and borderless.** Circles and pills use `radius: width / 2` —
geometry, not a token.

### Motion

`msFast` 90 (hover/press) · `msBase` 140 (state change) · `msSlow` 220 (dialog open). Easing
`OutQuad` everywhere.

### Colour roles

| Role | Source | Notes |
|---|---|---|
| `surfaceSunken` | theme `darker_background` | timeline bed, canvas letterbox |
| `surfacePanel` | theme `background` | default panel |
| `surfaceRaised` | theme `lighter_background` | cards, controls |
| `surfaceOverlay` | theme `dark_background` | dialogs, tooltips |
| `borderHairline` | theme `selection` | the only structural line |
| `borderStrong` | theme `muted` | separators inside dialogs |
| `focusRing` | theme `accent` | 2 px, always visible |
| `textPrimary` | theme `bright_foreground` | ≥ 7:1 |
| `textSecondary` | derived, AA-guarded | labels, metadata |
| `textDisabled` | secondary at 55 % | disabled only, never content |
| `accent` / `onAccent` | theme accent pair | ≥ 4.5:1 by construction |
| `success`/`warning`/`danger`/`info` | theme green/yellow/red/cyan | state only |
| `laneV1/V2/Text/Audio` | theme blue/magenta/yellow/green | lane identity |

Helpers: `Design.alpha(hex, a)` for scrims/tints, `Design.mix(a, b, t)` for hover lifts. QML must
not hand-write `rgba(...)` literals.

---

## 3. Component rules

- **`ActionButton` is the only button.** Height = label + 2 × padding with a `hCompact` floor, so
  a label cannot be clipped; `activeFocusOnTab`, a 2 px `focusRing`, Space/Return activation, and
  90 ms colour transitions on hover/press.
- **Labels over content sit on a scrim** — clip names use `Design.alpha(surfaceSunken, 0.72)`,
  never a raw black.
- **The viewer behaves like a monitor.** A muted `surfaceSunken` matte surrounds the picture, with
  corner crop marks and a concise `VIEWER / resolution / LOCAL` header. The canvas itself is black
  during playback and a raised themed surface only for the empty intake state.
- **Source rows read as a cutting list.** Kind is marked by a three-pixel lane stripe, media metadata
  uses mono numerals, and hover swaps metadata for explicit `V1`, `V2`, and remove actions without
  moving the name.
- **Section labels are `tCaption`/`textSecondary` above a `Divider`.** Uppercase tracking and bold
  headings are not used except for short technical ruler/status labels where mono tracking aids
  scanning.
- **Selected state = 2 px `textPrimary` outline; hover = surface tint.** Selection is never
  colour-only.
- **Empty states say what to do**: the viewer offers "Import Media" instead of "no clip", on the
  themed `surfaceSunken` bed so the prompt uses theme ink. Only real playback is black.
- **The mark is two offset bars in `textPrimary`** — drawn in QML, never a letter in a filled
  tile and never in the accent, which is reserved for Play, the primary action, focus and the
  playhead. `pkgbuild/cutcap.svg` carries the same two bars for the desktop icon.
- **Flat buttons carry no border.** Hover tints the surface, focus adds the 2 px ring; a border is
  for buttons that own a surface, not for the command strip.
- **Custom controls are controls, not mouse areas.** Lane toggles, color swatches, transition chips
  and reset actions expose a role and name, take tab focus, and activate with Space/Return.
- **Tiled sizes are first-class.** A tiling compositor ignores the window minimum (800 × 500). The
  timeline starts at about a third of the window height and its top edge is a split handle; below
  800 px wide the inspector and the top-bar status text hide; below 560 px the media bin, Undo and
  Redo hide too. `layoutSurvivesTiledSizes` audits 1273×697, 2550×697, 800×500, 780×500, 848×466, 640×697 and
  520×697.

---

## 4. Structural items — delivered

1. **Captions and voiceover left the media bin.** The bin is assets only; captions (language,
   auto-caption, `.srt` import/export) and voice (record, local TTS) are sections of the inspector.
   Text/captions are still added from the timeline toolbar.
2. **One inspector.** No tab systems: Look, Transform, Audio, Background, Captions, Voice in one
   scrolling column, swapping to text sections for a text selection.
3. **Track headers are controls.** Each lane has mute (M), solo (S) and lock (L) with live state
   text; mute and solo reach the export audio chain (`Project::laneAudible`), lock refuses moves,
   trims, property edits and deletes for that lane.
4. **Snap is visible.** `Backend::previewSnap` returns the snapped time and publishes
   `snapGuideSec`; the timeline draws a 1 px accent guide while a drag is within snapping distance,
   using the same rule the commit applies. The playhead has a draggable grab handle.
5. **Values are editable.** `ValueRow` = label + numeric entry + reset + slider, used for speed,
   brightness, contrast, saturation and blur. Look presets (Neutral / Punch / Mono / Warm / Soft)
   apply via `Backend::applyLookPreset`, one undo step each.

### Still open
- Ripple modes; keyboard-only clip placement.
- Colour looks are per-clip constants (no keyframed grading).
- Track collapse/add-track; lanes are the fixed four.

---

## 5. Layout revision (what visibly changed)

The first token pass mapped onto the existing values, so nothing moved. The second revision
changed the composition itself — measured at a fixed 1280×720 window, **41 % of pixels differ**
from the previous build (top bar 26 %, media bin 24 %, timeline 16 %, inspector 5 % with nothing
selected).

- **Panels are flat.** No rounded cards, no per-panel borders; panels butt together with a 1 px
  `borderHairline` seam and a hairline under every header. The window bed is `surfaceSunken`.
- **Top bar is a command strip** 36 px tall: logo chip, project title as borderless text with an
  accent underline on focus, `unsaved` flag, flat text actions, status right, accent Export.
- **Media bin is a list, not cards.** 64 px rows with a hairline divider, 64×36 thumbnail, name
  over one metadata line; the add/remove actions appear on hover only. Tabs became a small
  segmented switch (`Bin / Captions / Voice`) in the panel header.
- **Inspector has no tabs.** One scrolling column: Look, Transform, Audio, Background — with the
  text sections shown instead when a text overlay is selected. Section labels are `tCaption`
  secondary over a hairline.
- **Timeline**: 42 px lanes, 3 px lane-identity bar in the header, clips square (`rXs`) with the
  lane colour as a 2 px bottom line, filmstrips full-bleed, hover tint on clips, and a 1 px
  playhead with a draggable grab handle.
- **Transport** is a flat strip on a hairline: tabular timecode, dense aspect selector.

## 6. References

- CapCut desktop / Descript / Clipchamp panel-and-timeline anatomy —
  <https://www.descript.com/blog/article/best-video-editing-software> ,
  <https://www.descript.com/blog/article/best-video-editing-software-for-windows>
- DaVinci Resolve interface & panel focus — <https://2pop.calarts.edu/technicalsupport/davinci-resolve-interface>
- Desktop toolbar grouping, primary-action placement and narrow-window behaviour —
  <https://developer.apple.com/design/human-interface-guidelines/toolbars>
- Resizable workspace split views and pane sizing —
  <https://developer.apple.com/design/human-interface-guidelines/split-views>
- WCAG 2.2 pointer target-size minimum — <https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum>
- Dense product-UI type and spacing scales (4 px grid, 13–14 px body) —
  <http://designify.design/blog/ui-type-scale>
- Linear token conventions (surface steps, tabular numerals, hairline borders) —
  <https://styles.refero.design/style/90ce5883-bb24-4466-93f7-801cd617b0d1>
