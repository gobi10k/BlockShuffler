# MAC_SLICE_CHECKLIST.md — manual Mac execution slice

**COMPLETE 2026-07-15 — all items PASS.** Results recorded in `docs/VALIDATION_PLAN.md` (Mac-Release column). 11.1 passed after the title-mojibake fix (15b0b58); 12.3 passed after the logo-2/3 fix (fee8947).

Derived from `docs/VALIDATION_PLAN.md` on HEAD `25d1e2a` (2026-07-14). Harness slice already closed:
`STEP6 RESULT: ALL PASS` (T1–T46) — 29 objective rows marked PASS in the plan; the rows below are the
pure-manual rows plus the MAN half of every hybrid "T# + MAN" row. Work top to bottom; each group is one
app session.

**Binaries (both freshly rebuilt clean on `25d1e2a`):**
- **Release** (default for every group): `build-release/BlockShuffler_artefacts/Release/Standalone/BlockShuffler.app` (built Jul 13 22:28)
- **Debug+ASan** (crash/safety rows 12.1 and 7.6 only — run from Terminal so ASan reports are visible):
  `build-asan/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app` (built Jul 13 22:27)

Note: the plan's header says results are recorded against Release builds. For 12.1/7.6 run the ASan binary
first (it catches memory bugs Release hides); if clean, a quick Release repeat satisfies the plan's
letter — record both in the row.

Marking: tick `[x]` and write PASS/FAIL + date into the plan's Mac-Release column as you go.

---

## Group A — fresh 5-block project · Release binary
Setup: launch Release app, new empty project.

- [x] **1.1** — Click "+" five times → each click adds a tile at the strip end, palette colour, default name ("Block N").
- [x] **1.3** — Double-click block 2's name, type "Verse", Enter; right-click block 3 → Rename, type, click elsewhere → both names persist.
- [x] **1.4** — Right-click block 1 → Set Color → pick a colour → header + body adopt it immediately.
- [x] **13.1 (MAN half)** — Assign all 8 palette colours across blocks → each reads as its own hue on the dark theme; yellow is YELLOW (no blue tint). (Numeric half T45 PASS.)
- [x] **1.5 (MAN ×10 half)** — 10 consecutive gap-drops, varying source/target, both directions, incl. leftmost and rightmost gaps → every drop lands exactly at the released gap, zero snap-backs. (T43 PASS.)
- [x] **1.6 (MAN ×10 half)** — 10 consecutive drops ONTO a block, varying pairs → stack forms on FIRST attempt each time; merged stack sits at the DROP TARGET's slot; one Cmd+Z fully reverts each drop. (T32–T37 PASS.)
- [x] **5.11** — Shift+drag a stack → all tiles move together. Drag one member to a gap → it unstacks, lands at that gap, stays visible. Remove members until one remains → it auto-unstacks with stack settings reset.
- [x] **5.12** — Select a stack → Mode + How-Many controls appear ONCE for the whole stack; per-block chance sliders appear once in the combined view with effective values.
- [x] **13.2 (MAN half)** — Build a 9-block stack → every tile visible or reachable via stack scroll; none lost. (T26 PASS.)
- [x] **BS#3 observe (Carter ratifies)** — With the 9-stack tiles compressed below ~72 px → block name renders inside the 20 px header band, ellipsized, black-on-light / white-on-dark colour.
- [x] **6.2 (MAN half)** — Mark a block Done → ONLY visual dim/badge changes; layout and behaviour otherwise identical. (T10 PASS.)
- [x] **6.3 (MAN half)** — With Done set on a full-height tile AND a short stacked tile → DONE badge never covers the name. (T29 PASS.)
- [x] **1.2** — Select a block (no clip selected), press Delete → removed. Right-click another → Delete Block → removed (confirm dialog if it holds clips).
- [x] **12.3 (visual, Mac half)** — Logo sits left of Save As, background matches the bar. Measured 0.79 of bar height vs client "~2/3" ≈ 0.67 — observe and note; final ratio is Carter's call.

PASS

## Group B — StressProject.bsp · **ASan binary** (run from Terminal)
Setup: `./build-asan/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app/Contents/MacOS/BlockShuffler` then open StressProject.bsp (50 blocks; if the file doesn't exist yet, build it once: 50 × "+", a few stacks, save).

- [x] **12.1** — Drag-heavy session: ≥20 mixed gap/stack drops at speed, then 15 rapid Cmd+Z → no crash, no ASan report in the terminal. (Then a quick Release repeat to record per the plan's Release rule.)
- [x] **2.9** — Scroll the strip well away from the start; drag a clip from one block into a later block → after the drop the strip's scroll position is unchanged (no jump to start).
- [x] **11.5 (MAN half)** — Drag a start marker to a new grid position, Cmd+Z → marker restores exactly on-grid, unrelated state untouched; Cmd+Shift+Z → redo works, waveform preview never blanks. (T12 PASS.)

PASS

## Group C — project with audio clips · Release binary (7.6 in ASan)
Setup: new project, 2–3 WAV files handy (different tempos ideal); blocks for stack + link scenarios.

- [x] **2.1** — Select block 1; drag a WAV from Finder onto the WAVEFORM area → clip lands in the SELECTED block; waveform renders.
- [x] **2.2** — With block 1 still selected, drag a WAV onto BLOCK 2's tile → clip lands in block 2, not block 1.
- [x] **2.3** — "+ Add Clip" → chooser filters to audio formats; picked file becomes a clip.
- [x] **2.4** — Right-click a clip → Rename, Set Color, Remove Clip all work (re-add after remove).
- [x] **2.5** — Three clips in one block: drag ONE weight slider → the other two values numerically unchanged.
- [x] **2.8 (MAN half)** — Drag a start marker near a grid line → lands ON the line; Shift+drag → lands off-grid exactly at the pointer. (T44 PASS.)
- [x] **7.1 (MAN half)** — With weights/stack/link configured, hit Play several times → arrangements visibly/audibly vary across plays. (T1–T9 PASS.)
- [x] **7.2** — Trigger playback on a LIGHT block (yellow) and a DARK block (blue/purple) → white border + dark inner line clearly visible on both.
- [x] **7.3** — Full play-through → waveform view follows the playing block, playhead moves, time display updates continuously.
- [x] **7.4** — Select block A, press Play → inspector/selection unchanged while playing; Stop → waveform reverts to the selected block.
- [x] **7.5 (×10)** — Context menu: Play Block ×5 and Play Clip ×5 → 10/10 launch playback.
- [x] **B4 (MAN half)** — Play Clip on a clip with a lead-in, followed by another block → lead-in audible ahead of the next block's body. (T23/T38/T39 PASS.)
- [x] **5.1 / B2 (MAN ×10 half)** — Stack of 3, play-count 1: Play 10×, COUNT the players each time → exactly ONE every time; member varies. (T1 PASS.)
- [x] **5.5 (ear half)** — SIM mode, play 2 → two members heard simultaneously. (T4 PASS.)
- [x] **5.8 (ear half)** — Always-play-base ON → base heard every play + one weighted other; OFF → all compete. (T6 PASS.)
- [x] **B1 (MAN half)** — Mark a clip AND a block Done mid-session → both still play.
- [x] **B3 (MAN ×5 half)** — Blocks 1&3 linked 100%, block 2 between: Play 5× → order 3,2,1 EVERY time. (Needs the Group E link — fine to run during Group E instead; tick wherever executed.)
- [x] **3.x by-ear spot-check (advisory — numeric rows 3.1–3.5 already PASS via T38/T39/T40)** — Listen across a join: tail fades under the next lead-in, no click/gap; a cross-tempo join sounds continuous with no pitch jump.
- [x] **7.6 — ASan binary** — Rapid Play/Stop 20× → no crash, no stuck/hanging audio, terminal free of ASan output. (Quick Release repeat to record per plan.)

PASS

## Group D — sessions: save / open / undo-on-load · Release binary 
- [FAIL - file name is test_D, app header sayss 'BlockShuffler å test_D'] **11.1** — Save (Cmd+S), Save As (Cmd+Shift+S), Open all work; window title shows the project name.
- [x] **11.2 (MAN half)** — Save a 10-block project (stacks, links, weights, marks, tempo set), quit FULLY, reopen via Open → ALL blocks visible the instant the window opens; every setting intact. (T11 PASS.)
- [x] **11.3 / B5** — Quit fully; double-click the .bsp in Finder → loads identically to the Open button: all blocks visible immediately, zero interaction needed.
- [x] **11.4 / A9 (MAN half)** — Immediately after load press Cmd+Z → nothing happens (history cleared).
- [x] **11.6** — Rename/move a referenced media file; open the project → warning lists the missing path(s); project still loads with silent clip(s).
- [x] **A10 (inspect half)** — Open the saved .bsp in a text editor → startMark/endMark (all int64 sample values) are QUOTED JSON strings. (T11 PASS.)
- [x] **A11 (inspect half)** — Same file: every audioFile entry is `media/...` with forward slashes, no backslashes. (T30 PASS.)

PASS except 11.1

## Group E — links · Release binary

- [x] **4.1** — Right-click a block → Link to… → click target → arc drawn between the two; label shows block NAMES + % (no UUIDs).
- [x] **4.7 (MAN half)** — Remove the link → arc gone; ONE Cmd+Z → link AND its % restored. (T15 PASS.)
- [x] *(B3 lives here if not done in Group C.)*

PASS

## Group F — export dialog + tempo fields · Release binary

- [x] **10.1** — Export → chooser offers WAV, FLAC and BSF.
- [x] **10.2 (MAN half)** — Export WAV; open in Audacity next to the in-editor playback → pitch/speed/crossfades match by ear/waveform. (Numeric identity already T24/T25 PASS.)
- [x] **10.3 (MAN half)** — Export BSF; rename .bsf → .zip and unzip → manifest.json + model.json + clips/*.flac present; int64 fields are quoted strings. (Harness STEP3E PASS.)
- [x] **9.2** — Single click a tempo field → it focuses; type a value; Enter commits.
- [x] **9.3** — Set BLOCK tempo → all its clips' grids update; override ONE clip's tempo → only that clip changes; add a new clip → inherits the block tempo.
- [x] **9.4** — Block with no tempo set: add a clip → it gets the project default tempo.
- [x] **9.1 (MAN half)** — Change a clip's tempo → grid spacing changes; play → speed and pitch identical (audio untouched; buffer hash T42 PASS).

PASS
---

## Windows slice (NOT part of this checklist — do not mark on Mac)

- **12.2** — Windows parity: CI already GREEN on `25d1e2a` (run 29282354345) and `6db392e` (run 29282352780); T30 PASS on the Mac harness. Remaining: groups 1–11 spot-run + MSVC harness on the Windows box.
- **C1–C9** (all of Section C): .bsp Mac↔Windows round-trips (C1/C2/C3/C4), export byte-compare across OSes (C5), Windows open-on-launch (C6), .bsp association (C7), Explorer/taskbar icon (C8), ResolverDiag green on MSVC (C9).
- The **Windows-Release column** of every Group 1–13 row.
- **12.3** Windows half (logo check "on both platforms").

## Not rows / already closed

- **7.2 numeric backstop** was deliberately skipped per task ruling 2026-07-13 (MAN only).
- **BEYOND-SPEC #1/#2/#3** are Carter-ratification items, not pass/fail rows (BS#3 has an observe step in Group A).
- **12.3 logo ratio** measured 0.79 vs "~2/3" — client decision, recorded in the plan row.
