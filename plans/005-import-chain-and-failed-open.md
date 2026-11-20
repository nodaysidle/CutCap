# Plan 005: Background imports belong to the project they were started in

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
- **Category**: bug
- **Planned at**: commit `733897d`, 2026-11-20

## Why this matters

CutCap imports media in background jobs. `Backend` keeps a counter, `m_jobGeneration`, that
`newProject()` and `openProject()` increase; a job remembers the value it started with and its
result is dropped when the counter has moved on, so a result never lands in a different project.
Two holes remain:

1. A multi-file drop is a chain: each finished file starts the next one. The next file is started
   *after* the counter moved, so it captures the new value. Drop five files, press New while the
   first is being probed, and files 2–5 are added to the new, supposedly empty project.
2. `openProject()` increases the counter before it knows the file can be loaded. Opening a corrupt
   project fails and the current project stays, but every job still running for it (imports,
   captions, text-to-speech, previews) is now silently thrown away.

## Current state

All in `src/backend.cpp`.

`openProject` (around line 513):

```cpp
bool Backend::openProject(const QString &pathOrUrl) {
    ++m_jobGeneration;
    const QString path = normalizeLocalPath(pathOrUrl);
    bool ok = false;
    QString err;
    Project loaded = Project::loadFromFile(path, &ok, &err);
    if (!ok) {
        fail("Failed to open project: " + err);
        return false;
    }
    m_undoStack.clear();
    m_redoStack.clear();
```

The chain (around lines 703-746):

```cpp
struct Backend::ImportChain {
    Backend *backend = nullptr;
    QStringList queue;
    double cursor = 0.0;
    int track = 0;
};

void Backend::importMediaAt(const QVariantList &pathsOrUrls, int track, double atSec) {
    auto chain = std::make_shared<ImportChain>();
    chain->backend = this;
    chain->track = track;
    chain->cursor = std::max(0.0, atSec);
    for (const auto &v : pathsOrUrls)
        chain->queue.append(v.toString());
    processImportChain(chain);
}

void Backend::processImportChain(const std::shared_ptr<ImportChain> &chain) {
    while (!chain->queue.isEmpty()) {
        const QString path = normalizeLocalPath(chain->queue.takeFirst());
        if (path.endsWith(".cutcap", Qt::CaseInsensitive)) {
            openProject(path);
            continue;
        }
        if (path.endsWith(".srt", Qt::CaseInsensitive)) {
            importSrtFile(path);
            continue;
        }
        importAssetAsync(path, [this, chain](const QString &assetId) {
            const int idx = m_project.findAssetIndex(assetId);
            if (idx >= 0) {
                ...
            }
            processImportChain(chain);
        });
        return;
    }
}
```

In `importAssetAsync`, the finished handler calls `onReady({})` (an empty id) when
`generation != m_jobGeneration`; that is what lets the chain continue today.

Tests (`tests/cutcap_tests.cpp`): helper `makeSampleVideo(dir, name, color, seconds)` creates a
small video with ffmpeg and returns its path; `waitForImports(backend)` waits until
`backend.importing() == 0`. `testStaleCaptionResultDropped` and `testImportIsAsync` show how a
plain `Backend b;` is used without a window.

Repo rule that applies (from `AGENTS.md`): "A test that passes for the wrong reason is a defect.
Before trusting a green run, confirm the assertion can fail: break the behaviour it covers, watch it
fail, restore."

## Commands you will need

Run all commands from the repository root (the directory that contains `cutcap.pro`).

| Purpose | Command | Expected on success |
|---|---|---|
| Build + run all tests | `./bin/test > /tmp/claude-1000/a-test.log 2>&1; grep -E '^Totals\|^FAIL' /tmp/claude-1000/a-test.log` | `Totals: N passed, 0 failed, ...` and no `FAIL` line |

## Scope

**In scope** (the only files you may modify):
- `src/backend.cpp`
- `tests/cutcap_tests.cpp`

**Out of scope** (do NOT touch):
- `importAssetAsync` itself and the other job watchers (captions, TTS, background removal,
  previews): their generation checks are correct.
- `newProject()`.
- `src/backend.h` (the `ImportChain` struct is defined in the `.cpp`).

## Git workflow

- Work directly on the current branch (`advisor/audit-fixes`). No new branches, no push.
- One commit at the end, adding only the files you changed (`git add <files>`, not `git add -A`).
  Message: `A05: an import chain stops with its project; a failed open keeps running jobs`

## Steps

### Step 1: Add two tests first

Add two private slots to `tests/cutcap_tests.cpp`, directly after `testStaleCaptionResultDropped`:

```cpp
    // A multi-file drop must not keep feeding files into the project that replaced its own.
    void testImportChainStopsAtNewProject() {
        QTemporaryDir dir;
        QVariantList files;
        for (const QString &name : {QStringLiteral("one.mp4"), QStringLiteral("two.mp4"), QStringLiteral("three.mp4")}) {
            const QString vid = makeSampleVideo(dir.path(), name, "teal", 1.0);
            QVERIFY(!vid.isEmpty());
            files << vid;
        }

        // Positive control: left alone, the chain delivers all three clips.
        Backend control;
        control.importMediaAt(files, 0, 0.0);
        waitForImports(control);
        QCOMPARE(control.clipsList().size(), 3);

        Backend b;
        b.importMediaAt(files, 0, 0.0);
        QVERIFY2(b.importing() > 0, "the first file must still be importing when the project is replaced");
        b.newProject();
        waitForImports(b);
        QTest::qWait(300);
        waitForImports(b);
        QCOMPARE(b.clipsList().size(), 0);
        QCOMPARE(b.assetsList().size(), 0);
    }

    // A project that fails to open leaves the current one in place, so its running jobs still count.
    void testFailedOpenKeepsRunningJobs() {
        QTemporaryDir dir;
        const QString vid = makeSampleVideo(dir.path(), "keep.mp4", "navy", 1.0);
        QVERIFY(!vid.isEmpty());
        const QString broken = dir.path() + "/broken.cutcap";
        QFile f(broken);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a project");
        f.close();

        Backend b;
        b.importSingleMedia(vid);
        QVERIFY2(b.importing() > 0, "the import must still be running when the open fails");
        QVERIFY2(!b.openProject(broken), "a corrupt project must fail to open");
        waitForImports(b);
        QCOMPARE(b.clipsList().size(), 1);
    }
```

**Verify (both must FAIL now)**: full test command → two `FAIL!` lines, one for each new slot.
If either passes before the fix, that is a STOP condition.

### Step 2: The chain remembers its project

In `src/backend.cpp`:

1. Add `int generation = 0;` to `struct Backend::ImportChain`.
2. In `importMediaAt`, after `chain->backend = this;`, add `chain->generation = m_jobGeneration;`.
3. At the very top of `processImportChain`, before the `while`, add:

```cpp
    // The project this drop was made into is gone; the files still queued do not belong to the new one.
    if (chain->generation != m_jobGeneration)
        return;
```

4. A `.cutcap` inside the chain replaces the project on purpose and the files after it belong to
   the project it opened, so keep the chain alive across it:

```cpp
        if (path.endsWith(".cutcap", Qt::CaseInsensitive)) {
            openProject(path);
            chain->generation = m_jobGeneration;
            continue;
        }
```

### Step 3: Only a successful open ends the old project's jobs

In `openProject`, remove the first line `++m_jobGeneration;` and put it directly after the
`if (!ok) { ... return false; }` block, before `m_undoStack.clear();`.

**Verify**: `grep -n "++m_jobGeneration" src/backend.cpp` → exactly two lines, one in `newProject`
and one in `openProject` after the failure return.

### Step 4: Run everything

**Verify**: full test command → 0 failed, no `FAIL` line, exactly two more passing tests than
before this plan.

### Step 5: Commit

Commit with the message from "Git workflow".

## Done criteria

- [ ] Step 1 showed both new tests failing before the fix
- [ ] Full test run has 0 failed and two more passing tests than before
- [ ] `grep -c "chain->generation" src/backend.cpp` → `3`
- [ ] `git show --stat HEAD` lists only `src/backend.cpp` and `tests/cutcap_tests.cpp`

## STOP conditions

- The quoted code in "Current state" is not in `src/backend.cpp`.
- A new test passes before the fix, or `QVERIFY2(b.importing() > 0, ...)` fails (the import finished
  synchronously, so the test cannot observe the race). Report it; do not remove the assertion.
- `openProject` on the corrupt file returns true.
- An existing test fails after the change and one fix attempt does not resolve it.

## Maintenance notes

- Any new chained or multi-step background work needs the same rule: capture `m_jobGeneration` once
  when the user starts it, not once per step.
