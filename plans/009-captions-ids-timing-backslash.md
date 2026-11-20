# Plan 009: Captions get unique ids, land where the clip plays, and keep typed backslashes

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
- **Risk**: LOW
- **Depends on**: none
- **Category**: bug
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

Three caption bugs in CutCap (a Qt 6 / C++17 video editor that shells out to ffmpeg and whisper):

1. **Ids repeat.** Every parsed caption gets the id `cap_<index>_<start×100>`, and the index
   restarts at 1 on each parse. Auto-captioning two clips whose speech starts at 0:00 yields two
   captions with the id `cap_1_0`; selecting, editing or deleting one then hits the other.
2. **Auto-captions ignore trim and speed.** The caption job transcribes the clip's whole source
   file and adds only the clip's timeline start. A clip trimmed to begin 20 s into its source gets
   speech from source second 25 captioned at timeline second 25 instead of 5, the cut-away part
   still produces captions, and a clip played at 2× gets captions at half the right rate.
3. **Typed backslashes are mangled on export.** The exporter writes `\` as `\\` into the ASS
   subtitle file. libass has no such escape: rendered with libass 0.17, `A\\NB` still breaks the
   line and `C:\\new` loses the `n`. Rendering confirmed that a backslash followed by the invisible
   character U+2060 (WORD JOINER) is drawn as a literal backslash and nothing after it is
   interpreted.

## Current state

### `src/mediaengine.cpp`

`parseSrt` (search `QList<TextOverlay> MediaEngine::parseSrt`):

```cpp
        TextOverlay t;
        t.id = QString("cap_%1_%2").arg(idx++).arg(int(start * 100));
        t.text = textLines.join("\n").trimmed();
        t.isCaption = asCaptions;
        t.startSec = start;
        t.durationSec = std::max(0.2, end - start);
```

`runAutoCaptions` (search `QList<TextOverlay> MediaEngine::runAutoCaptions`), declared in
`src/mediaengine.h` as
`static QList<TextOverlay> runAutoCaptions(const QString &mediaPath, const QString &language = "en", QString *error = nullptr);`
(check the exact declaration in the header). It extracts audio with:

```cpp
    ff.start("ffmpeg", {"-y", "-i", mediaPath, "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le", wavPath});
```

and ends with `return parseSrt(QString::fromUtf8(srtFile.readAll()), true);`

`generateAssSubtitles` (search `escaped.replace`):

```cpp
        QString escaped = t.text;
        escaped.replace("\\", "\\\\");
        escaped.replace("\n", "\\N");
        escaped.replace("{", "\\{").replace("}", "\\}");
```

### `src/backend.cpp`, `Backend::generateAutoCaptions`

```cpp
    const TimelineClip clip = m_project.clips[idx];
    ...
    auto future = QtConcurrent::run([clip, language]() -> std::pair<QList<TextOverlay>, QString> {
        QString err;
        auto captions = MediaEngine::runAutoCaptions(clip.sourcePath, language, &err);
        for (auto &c : captions)
            c.startSec += clip.timelineStartSec;
        return {captions, err};
    });
```

### `src/project.h`

`struct TimelineClip` has `double timelineStartSec`, `double durationSec` (length on the timeline),
`double sourceInSec` (where in the source file the clip starts) and `double speed`. The source span
a clip plays is `durationSec * speed` seconds starting at `sourceInSec` (the exporter uses exactly
`-ss sourceInSec -t durationSec * speed`). `struct TextOverlay` has `QString id`, `QString text`,
`double startSec`, `double durationSec`.

### `src/cli.cpp`

One other caller: `MediaEngine::runAutoCaptions(args[3], argValue(args, "--lang", "en"), &err);`
(captions for a whole file). It must keep compiling and behaving as today.

### Tests (`tests/cutcap_tests.cpp`)

- `testAssEscapesBackslash` currently asserts the wrong behaviour:

```cpp
        t.text = "C:\\new folder";
        ...
        QVERIFY2(dialogue.contains("C:\\\\new folder"), qPrintable(dialogue));
        QVERIFY2(!dialogue.contains("C:\\new folder"), qPrintable(dialogue));
```

- `projectRoundTripAndSrtParsing` parses SRT text with `MediaEngine::parseSrt`; check whether it
  asserts on a caption id before changing ids (`grep -n '"cap_' tests/cutcap_tests.cpp`).
- `testStaleCaptionResultDropped` shows how to fake whisper: it writes a shell script named
  `whisper-cli` with `makeTool(...)`, puts its directory first on `PATH` (restored by `PathRestore`),
  and the script writes an `.srt` next to the `-of` argument. Copy that mechanism.

Repo rule (from `AGENTS.md`): "A test that passes for the wrong reason is a defect. Before trusting
a green run, confirm the assertion can fail: break the behaviour it covers, watch it fail, restore."

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |
| Build app (also compiles `cli.cpp`) | `./bin/build > /tmp/claude-1000/a-build.log 2>&1; echo $?` | `0` |

## Scope

**In scope** (the only files you may modify):
- `src/mediaengine.h`
- `src/mediaengine.cpp`
- `src/backend.cpp` (only `generateAutoCaptions`)
- `tests/cutcap_tests.cpp`

**Out of scope** (do NOT touch):
- `src/cli.cpp`: its call keeps working through default arguments.
- SRT export, the caption UI, the `.cutcap` format.
- Text-to-speech (`espeak`) argument handling.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only the files you changed (`git add <files>`, not `git add -A`).
  Message: `A09: captions get unique ids, follow trim and speed, and keep typed backslashes`

## Steps

### Step 1: Tests first

1. Change `testAssEscapesBackslash` to assert the correct output. Replace its two final `QVERIFY2`
   lines with:

```cpp
        // libass has no escape for a backslash; a word joiner after it stops "\n", "\N" and "\h"
        // from being read as line breaks and hard spaces.
        QVERIFY2(dialogue.contains(QString("C:\\") + QChar(0x2060) + "new folder"), qPrintable(dialogue));
        QVERIFY2(!dialogue.contains("\\\\"), qPrintable(dialogue));
```

2. Add three private slots directly after `testAssEscapesBackslash`:

```cpp
    // Two parses must never hand out the same caption id, or editing one caption edits another.
    void testCaptionIdsAreUnique() {
        const QString srt = "1\n00:00:00,000 --> 00:00:01,000\nHello\n\n2\n00:00:01,000 --> 00:00:02,000\nWorld\n";
        QSet<QString> ids;
        for (int pass = 0; pass < 3; ++pass) {
            const auto items = MediaEngine::parseSrt(srt, true);
            QCOMPARE(items.size(), 2);
            for (const auto &t : items)
                ids.insert(t.id);
        }
        QCOMPARE(ids.size(), 6);
    }

    // Cue times come back relative to the part of the source the clip plays; they are placed on
    // the timeline through the clip's start and speed, and anything past the clip's end is dropped.
    void testCaptionsFollowTrimAndSpeed() {
        TimelineClip clip;
        clip.timelineStartSec = 4.0;
        clip.durationSec = 3.0;   // on the timeline
        clip.sourceInSec = 20.0;
        clip.speed = 2.0;         // plays 6 s of source
        QList<TextOverlay> cues;
        TextOverlay a; a.id = "a"; a.startSec = 1.0; a.durationSec = 2.0; cues.append(a);
        TextOverlay b; b.id = "b"; b.startSec = 5.0; b.durationSec = 4.0; cues.append(b);
        TextOverlay c; c.id = "c"; c.startSec = 6.5; c.durationSec = 1.0; cues.append(c);

        const auto placed = MediaEngine::captionsForClip(cues, clip);
        QCOMPARE(placed.size(), 2);
        QCOMPARE(placed[0].id, QString("a"));
        QVERIFY(std::abs(placed[0].startSec - 4.5) < 1e-6);
        QVERIFY(std::abs(placed[0].durationSec - 1.0) < 1e-6);
        // "b" starts inside the clip and is cut at the clip's end (timeline 7.0).
        QVERIFY(std::abs(placed[1].startSec - 6.5) < 1e-6);
        QVERIFY(std::abs(placed[1].durationSec - 0.5) < 1e-6);
    }
```

   The third slot, `testAutoCaptionsUseTheClipSpan`, is an end-to-end check with a fake whisper.
   Model it on `testStaleCaptionResultDropped`: same `makeSampleVideo` (use 6.0 s), same
   `makeTool(bin, "whisper-cli", ...)` script but without the `sleep 1` line and writing the cue
   `1\n00:00:01,000 --> 00:00:02,000\nSpoken\n\n`, same `PATH` handling. Then:

```cpp
        Backend b;
        b.importSingleMedia(vid);
        waitForImports(b);
        const QString clipId = b.clipsList().first().toMap().value("id").toString();
        // Trim: the clip now starts 2 s into its source, sits at timeline 0 and lasts 3 s.
        b.trimClip(clipId, 0.0, 3.0, 2.0);
        b.moveClip(clipId, 4.0, 0);
        b.generateAutoCaptions(clipId, "en");
        QTRY_VERIFY_WITH_TIMEOUT(!b.busy(), 30000);
        QCOMPARE(b.textsList().size(), 1);
        const QVariantMap cap = b.textsList().first().toMap();
        // The cue is 1 s into the transcribed span, so it belongs 1 s after the clip's start.
        QVERIFY2(std::abs(cap.value("startSec").toDouble() - 5.0) < 0.01, qPrintable(cap.value("startSec").toString()));
```

   Read `Backend::trimClip` and `Backend::moveClip` in `src/backend.h` first to confirm the
   argument order `(clipId, newStartSec, newDurationSec, newSourceInSec)` and `(clipId, newStartSec, newTrack)`.
   After the trim and move, assert the clip really has `sourceInSec` 2.0 and `timelineStartSec` 4.0
   (`b.clipsList().first().toMap()`), so the test does not depend on an assumption. With the bug
   present this test passes by coincidence only if the numbers line up, so also add, in the same
   slot before `generateAutoCaptions`, a check of the audio the fake whisper received: make the
   script copy its input wav (`cp "$last_arg" "$out.input.wav"` where the wav path is the script's
   last positional argument; look at how `runAutoCaptions` invokes `whisper-cli` to find the
   argument position) and after the job assert with the existing helper `probeDurationSec(...)` that
   the copied wav is about 3.0 s long (`std::abs(d - 3.0) < 0.3`), not 6.0 s.

**Verify (must FAIL now)**: full test command → `FAIL!` lines for `testAssEscapesBackslash`,
`testCaptionIdsAreUnique` and `testAutoCaptionsUseTheClipSpan`; `testCaptionsFollowTrimAndSpeed`
does not compile yet, which is expected: if the build fails only because
`MediaEngine::captionsForClip` is missing, continue to Step 2.

### Step 2: Unique ids

In `parseSrt` replace the id line with a process-unique id and remove the now unused `idx` variable:

```cpp
        // Unique across parses: ids are how the timeline tells captions apart.
        t.id = "cap_" + QUuid::createUuid().toString(QUuid::Id128).left(12);
```

Add `#include <QUuid>` to `src/mediaengine.cpp`.

### Step 3: Transcribe only the span the clip plays, and place cues on the timeline

1. `src/mediaengine.h`: extend the declaration with two defaulted parameters at the end, and add
   the helper:

```cpp
    // spanStartSec/spanLengthSec limit the transcription to part of the file; cue times are then
    // relative to spanStartSec. A negative length means "to the end".
    static QList<TextOverlay> runAutoCaptions(const QString &mediaPath, const QString &language = "en",
                                              QString *error = nullptr, double spanStartSec = 0.0,
                                              double spanLengthSec = -1.0);
    // Maps cues that are relative to the clip's source-in point onto the timeline.
    static QList<TextOverlay> captionsForClip(const QList<TextOverlay> &cues, const TimelineClip &clip);
```

   Keep the existing parameter order and defaults as they are in the header; only append.

2. `src/mediaengine.cpp`, in `runAutoCaptions`: build the ffmpeg argument list so the span is cut
   on the input side:

```cpp
    QStringList extract{"-y"};
    if (spanStartSec > 0.0)
        extract << "-ss" << QString::number(spanStartSec, 'f', 3);
    if (spanLengthSec > 0.0)
        extract << "-t" << QString::number(spanLengthSec, 'f', 3);
    extract << "-i" << mediaPath << "-ar" << "16000" << "-ac" << "1" << "-c:a" << "pcm_s16le" << wavPath;
    ff.start("ffmpeg", extract);
```

3. `src/mediaengine.cpp`, add the helper below `runAutoCaptions`:

```cpp
QList<TextOverlay> MediaEngine::captionsForClip(const QList<TextOverlay> &cues, const TimelineClip &clip) {
    QList<TextOverlay> out;
    const double speed = clip.speed > 0.0 ? clip.speed : 1.0;
    const double clipEnd = clip.timelineStartSec + clip.durationSec;
    for (TextOverlay cue : cues) {
        const double start = clip.timelineStartSec + cue.startSec / speed;
        if (start >= clipEnd)
            continue; // spoken after the part of the source this clip plays
        cue.startSec = start;
        cue.durationSec = std::min(cue.durationSec / speed, clipEnd - start);
        out.append(cue);
    }
    return out;
}
```

4. `src/backend.cpp`, in `generateAutoCaptions`, replace the lambda body:

```cpp
        QString err;
        const auto cues = MediaEngine::runAutoCaptions(clip.sourcePath, language, &err, clip.sourceInSec,
                                                       clip.durationSec * clip.speed);
        return {MediaEngine::captionsForClip(cues, clip), err};
```

### Step 4: Backslashes

In `generateAssSubtitles` replace the line `escaped.replace("\\", "\\\\");` with:

```cpp
        // libass has no backslash escape; a word joiner after it keeps "\N", "\n" and "\h" in user
        // text from being read as control codes, and draws nothing itself.
        escaped.replace("\\", QString("\\") + QChar(0x2060));
```

Keep it before the `"\n"` → `"\\N"` replacement, exactly where the old line was.

**Verify**: build command → `0`. Full test command → 0 failed, exactly three more passing tests
than before this plan.

### Step 5: Prove the tests can fail

Temporarily revert Step 3.4 (put back `runAutoCaptions(clip.sourcePath, language, &err)` plus the
old `c.startSec += clip.timelineStartSec` loop) → the full test command must show
`FAIL!  : CutCapTests::testAutoCaptionsUseTheClipSpan`. Restore → 0 failed.

### Step 6: Commit

Commit with the message from "Git workflow".

## Done criteria

- [ ] Full test run has 0 failed and three more passing tests than before this plan
- [ ] `./bin/build` exits 0
- [ ] `grep -c 'cap_%1_%2' src/mediaengine.cpp` → `0`
- [ ] `grep -c '0x2060' src/mediaengine.cpp` → `1`
- [ ] Step 5 showed the end-to-end test failing without the fix
- [ ] `git show --stat HEAD` lists only the four in-scope files (plus `cutcap.qmltypes` if the build changed it)

## STOP conditions

- An existing test asserts a specific `cap_<n>_<n>` id and cannot be kept without changing its
  meaning. Report the test and line.
- `trimClip` / `moveClip` do not produce `sourceInSec` 2.0 / `timelineStartSec` 4.0 in the
  end-to-end test (the clip state check fails). Report the actual values; do not adjust the
  expectations to whatever comes out.
- The fake `whisper-cli` is not the tool `runAutoCaptions` picks (another whisper on `PATH` wins).
- The header's existing `runAutoCaptions` declaration differs from the one quoted in a way that
  makes appending two defaulted parameters impossible.

## Maintenance notes

- `captionsForClip` is the single place that knows how source time maps to timeline time for
  captions; keyframed or variable speed would have to change it.
- Captions imported from an `.srt` file are still placed at their own timestamps, as before.
