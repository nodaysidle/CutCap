# Plan 003: Medium and semibold UI text renders upright

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
- **Risk**: LOW (visual change: a different typeface)
- **Depends on**: none
- **Category**: bug (UI)
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

CutCap's UI font is `Nimbus Sans`, used at weights 400, 500 and 600. The installed Nimbus Sans
family has only Regular, Italic, Bold and Bold Italic. Asked for weight 500 Qt picks the *Italic*
face, and for 600 the *Bold Italic* face (measured on the target machine with Qt 6.12). Every
button, panel title and section label is therefore slanted, which nobody designed. `Adwaita Sans`
is installed on the target system (Arch package `adwaita-fonts`) and resolves 400 → Regular,
500 → Medium, 600 → SemiBold. After this plan the UI uses Adwaita Sans, the package depends on the
fonts it needs, and a test fails if the UI font ever resolves to a slanted face again.

## Current state

- `src/design.h` (around line 109):

```cpp
    QString fontUi() const { return QStringLiteral("Nimbus Sans"); }
    QString fontMono() const { return QStringLiteral("JetBrains Mono"); }
```

  and (around lines 118-120) `wRegular()` returns 400, `wMedium()` 500, `wSemibold()` 600.

- `src/main.cpp` (around line 35):

```cpp
    QFont uiFont("Nimbus Sans");
    uiFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(uiFont);
```

- `DESIGN.md` (around line 74):

```
Typography: `Nimbus Sans` carries the UI; `JetBrains Mono` carries timecode, ruler labels, numeric
```

- `pkgbuild/PKGBUILD` line 8:

```
depends=('qt6-base' 'qt6-declarative' 'qt6-multimedia' 'qt6-svg' 'qt6-wayland' 'ffmpeg' 'pipewire')
```

- `tests/cutcap_tests.cpp`, slot `designTokensAreUsedAndConsistent` contains
  `QVERIFY(!d.fontUi().isEmpty() && !d.fontMono().isEmpty());` where `d` is a `Design` built from a
  `Theme` on a temporary directory. Copy how that slot constructs `Theme` and `Design`.

- `grep -rn "Nimbus" src tests DESIGN.md pkgbuild` finds exactly three places: `src/design.h`,
  `src/main.cpp`, `DESIGN.md`.

Repo rules that apply (from `AGENTS.md`): "A test that passes for the wrong reason is a defect.
Before trusting a green run, confirm the assertion can fail: break the behaviour it covers, watch it
fail, restore." No new runtime code dependencies.

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Font is installed | `fc-list "Adwaita Sans" family \| head -1` | a line containing `Adwaita Sans` |
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |
| Build app | `./bin/build > /tmp/claude-1000/a-build.log 2>&1; echo $?` | `0` |

## Scope

**In scope** (the only files you may modify):
- `src/design.h`
- `src/main.cpp`
- `DESIGN.md` (the one Typography sentence only)
- `pkgbuild/PKGBUILD` (the `depends` line only)
- `tests/cutcap_tests.cpp`

**Out of scope** (do NOT touch):
- Any QML file. QML reads the family from `Design.fontUi`; no QML names a font family literally.
- The weight tokens (`wRegular`, `wMedium`, `wSemibold`) and every size token.
- `fontMono` / JetBrains Mono.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only the files you changed (`git add <files>`, not `git add -A`).
  Message: `A03: UI font is Adwaita Sans, which has real medium and semibold weights`

## Steps

### Step 1: Add a test that describes the bug

Add a new private slot `uiFontWeightsAreUpright` to `tests/cutcap_tests.cpp`, directly after
`designTokensAreUsedAndConsistent`:

```cpp
    // A family without the weights the UI asks for makes Qt fall back to a slanted face (Nimbus
    // Sans has no 500/600 and resolved to Italic and Bold Italic).
    void uiFontWeightsAreUpright() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        Design d(theme);
        for (int weight : {d.wRegular(), d.wMedium(), d.wSemibold()}) {
            QFont font(d.fontUi());
            font.setWeight(QFont::Weight(weight));
            const QFontInfo info(font);
            // Qt may append the foundry, e.g. "Nimbus Sans [UKWN]".
            QVERIFY2(info.family().startsWith(d.fontUi()),
                     qPrintable(QString("UI font %1 is not installed; got %2").arg(d.fontUi(), info.family())));
            const QString style = info.styleName();
            QVERIFY2(!style.contains("Italic", Qt::CaseInsensitive) && !style.contains("Oblique", Qt::CaseInsensitive),
                     qPrintable(QString("weight %1 of %2 resolves to the slanted face '%3'")
                                    .arg(weight).arg(d.fontUi(), style)));
        }
    }
```

If `Theme`/`Design` are constructed differently in `designTokensAreUsedAndConsistent`, match that
slot. Add `#include <QFontInfo>` with the other includes at the top of the file.

**Verify (the test must FAIL now)**: full test command → a line
`FAIL!  : CutCapTests::uiFontWeightsAreUpright` mentioning weight 500 and `Italic`. If it passes
before the fix, that is a STOP condition.

### Step 2: Switch the family

- `src/design.h`: `fontUi()` returns `QStringLiteral("Adwaita Sans")`.
- `src/main.cpp`: `QFont uiFont("Adwaita Sans");`
- `DESIGN.md`: in the Typography sentence replace `` `Nimbus Sans` `` with `` `Adwaita Sans` `` and
  append one sentence to that paragraph: "The UI family must ship real 500 and 600 weights; Nimbus
  Sans does not, and Qt substituted its italic faces."
- `pkgbuild/PKGBUILD`: add `'adwaita-fonts'` and `'ttf-jetbrains-mono'` to the end of the `depends`
  array.

**Verify**: `grep -rn "Nimbus" src pkgbuild` → no output.
`grep -c "adwaita-fonts" pkgbuild/PKGBUILD` → `1`.

### Step 3: Run everything

**Verify**: build command → `0`. Full test command → `Totals: 42 passed, 0 failed` if plan 002 has
not landed yet, or `43 passed` if it has; in both cases no `FAIL` line and exactly one more passing
test than before your change.

### Step 4: Commit

Commit with the message from "Git workflow". If `./bin/build` changed `cutcap.qmltypes`, include it.

## Done criteria

- [ ] Step 1 showed `uiFontWeightsAreUpright` failing before the family changed
- [ ] Full test run has 0 failed and one more passing test than before
- [ ] `grep -rn "Nimbus" src pkgbuild` prints nothing
- [ ] `git show --stat HEAD` lists only in-scope files (plus `cutcap.qmltypes` if the build changed it)

## STOP conditions

- `fc-list "Adwaita Sans"` prints nothing (the font is not installed on this machine).
- The new test passes before Step 2 (then it does not detect the bug).
- After Step 2 an existing test fails, in particular `uiSmokeButtonsPipAndDrop` (it checks that
  every labelled control fits its label and audits the layout at 780×500; a wider typeface can trip
  it). Report the exact failure message. Do not change any size, padding or layout to make it pass.

## Maintenance notes

- Changing `fontUi` again requires a family with real 400/500/600 weights; the new test enforces it.
- The package now depends on `adwaita-fonts` and `ttf-jetbrains-mono`; without them Qt silently
  falls back to another family.
