# Plan 011: An image clip on the audio track no longer breaks export

> **Executor instructions**: Follow this plan step by step. Run every verification command and
> confirm the expected result before moving to the next step. If anything in "STOP conditions"
> occurs, stop and report; do not improvise. Do not edit anything under `plans/`.
>
> **Drift check (run first)**: `git log --oneline -1` must show a commit on branch
> `advisor/audit-fixes`. Compare the "Current state" excerpts with the live code before editing;
> line numbers are approximate, the quoted code is the anchor. A mismatch is a STOP condition.

## Status

- **Priority**: P3
- **Effort**: S
- **Risk**: LOW
- **Depends on**: none
- **Category**: bug
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

CutCap has three kinds of lane: track 0 (V1 video), track 1 (V2 overlay) and track 2 (A1 audio).
The GUI never puts a still image on the audio track, but the command-line tool accepts
`cutcap add-clip project.cutcap picture.png --track 2`, and a hand-edited project file can say the
same. The exporter then treats the image as an audio source and asks ffmpeg for an audio stream the
picture does not have, so the whole export fails with "matches no streams". An image on the audio
lane has nothing to contribute; the exporter should leave it out, and the CLI should refuse to
create it.

## Current state

`src/mediaengine.cpp`, in `MediaEngine::buildExportArgs`, clips are sorted into three groups:

```cpp
    for (const auto &c : project.clips) {
        if (c.kind == "audio" || c.track == 2)
            audioClips.append(c);
        else if (c.track == 1)
            overlayClips.append(c);
        else
            mainVideoClips.append(c);
    }
```

`c.kind` is one of `"video"`, `"audio"`, `"image"`. A *video* file on track 2 is supported on
purpose (its sound is used); only an image there is the problem.

`src/cli.cpp`, the `add-clip` command builds the clip like this:

```cpp
        c.kind = asset.kind;
        c.track = argValue(args, "--track", asset.kind == "audio" ? "2" : "0").toInt();
```

Read the surrounding function to see how this command reports an error and which value it returns
(for example for a file that cannot be probed); use the same style.

Test pattern: `tests/cutcap_tests.cpp`, slot `exportArgsContainFiltersAndSubtitles` builds a
`Project` by hand (`TimelineClip c; c.sourcePath = "/tmp/demo.mp4"; ... p.clips.append(c);`) and
calls `MediaEngine::buildExportArgs(p, "/tmp/out.mp4", opts, "/tmp/subs.ass")`, which returns a
`QStringList` of ffmpeg arguments. No file needs to exist for that call.

Repo rule (from `AGENTS.md`): "A test that passes for the wrong reason is a defect. Before trusting
a green run, confirm the assertion can fail."

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |
| Build app (compiles `cli.cpp`) | `./bin/build > /tmp/claude-1000/a-build.log 2>&1; echo $?` | `0` |

## Scope

**In scope** (the only files you may modify):
- `src/mediaengine.cpp` (the grouping loop only)
- `src/cli.cpp` (the `add-clip` command only)
- `tests/cutcap_tests.cpp`

**Out of scope** (do NOT touch):
- `src/project.cpp`: do not clamp or move clips on load; a video on track 2 must keep working.
- `src/backend.cpp`: the GUI already keeps images off the audio track.

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only the files you changed (`git add <files>`, not `git add -A`).
  Message: `A11: an image on the audio track is left out of export and refused by the CLI`

## Steps

### Step 1: Test first

Add a private slot `exportSkipsImageOnAudioTrack` directly after
`exportArgsContainFiltersAndSubtitles`:

```cpp
    // A still on the audio lane has no sound to give; asking ffmpeg for its audio fails the export.
    void exportSkipsImageOnAudioTrack() {
        Project p;
        TimelineClip video;
        video.id = "v";
        video.kind = "video";
        video.sourcePath = "/tmp/base.mp4";
        video.track = 0;
        video.durationSec = 2.0;
        video.sourceDurationSec = 2.0;
        p.clips.append(video);
        TimelineClip still;
        still.id = "i";
        still.kind = "image";
        still.sourcePath = "/tmp/picture.png";
        still.track = 2;
        still.durationSec = 2.0;
        still.sourceDurationSec = 2.0;
        p.clips.append(still);

        const QStringList args = MediaEngine::buildExportArgs(p, "/tmp/out.mp4", ExportOptions{}, {});
        QVERIFY2(args.contains("/tmp/base.mp4"), "positive control: the video clip is an input");
        QVERIFY2(!args.contains("/tmp/picture.png"), qPrintable(args.join(' ')));
    }
```

If `TimelineClip` needs other fields set for `buildExportArgs` to run (compare with the existing
slot), set them the way that slot does.

**Verify (must FAIL now)**: full test command → `FAIL!  : CutCapTests::exportSkipsImageOnAudioTrack`.

### Step 2: Leave the image out of the audio group

In the grouping loop in `buildExportArgs`:

```cpp
    for (const auto &c : project.clips) {
        if (c.kind == "audio" || c.track == 2) {
            // A still on the audio lane has no audio stream; mapping one would fail the export.
            if (c.kind != "image")
                audioClips.append(c);
        } else if (c.track == 1) {
            overlayClips.append(c);
        } else {
            mainVideoClips.append(c);
        }
    }
```

### Step 3: The CLI refuses to create one

In `src/cli.cpp`, in `add-clip`, directly after the `c.track = ...` line, refuse an image on track
2 with a one-line message in the same style and with the same failure return value the command
already uses for bad input. Message: `An image cannot go on the audio track (2); use --track 0 or 1.`

**Verify**: build command → `0`. Full test command → 0 failed, exactly one more passing test than
before this plan.

### Step 4: Commit

Commit with the message from "Git workflow".

## Done criteria

- [ ] Step 1 showed the new test failing before the fix
- [ ] Full test run has 0 failed and one more passing test than before this plan
- [ ] `./bin/build` exits 0
- [ ] `grep -c "cannot go on the audio track" src/cli.cpp` → `1`
- [ ] `git show --stat HEAD` lists only the three in-scope files

## STOP conditions

- The grouping loop in "Current state" is not in `src/mediaengine.cpp`.
- The new test fails before the fix for a different reason (for example `buildExportArgs` returns
  an empty list for this project). Report what it returned.
- `add-clip` in `src/cli.cpp` has no existing error path to copy.

## Maintenance notes

- The lane rule (audio files on track 2, everything else on 0 or 1) is enforced in the GUI by
  `Backend::addAssetToTimeline` and `Backend::moveClip`, and now partly in the CLI. A single shared
  rule would be better; it was left alone because a video on track 2 is deliberately allowed in
  export.
