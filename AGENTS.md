# CutCap — agent notes

## Reporting

Report only whether the app works. Fix defects and weak tests silently; do not narrate
them. The final message states the working state, one line of proof, and stops.

## Rules

- **A test that passes for the wrong reason is a defect.** Before trusting a green run,
  confirm the assertion can fail: break the behaviour it covers, watch it fail, restore.
  A guard that inspects nothing (no items, no rows, no files) is not a guard — assert a
  minimum count alongside "nothing is wrong".
- **Never assert on coordinates, counts or timings the harness picked for itself.** Read
  the state the way a user reaches it (visible viewport, current scroll offset, pointer
  position), or the test proves only that the code agrees with itself.
- **When a test needs a value the app owns** (scroll offset, zoom, layout), capture it at
  the moment of the action instead of assuming the value set earlier still holds.
- **Fix the cause, and keep one rule for it.** Duplicate conventions (one snap rule for
  drops, another for drags) drift apart; extract the shared one.
