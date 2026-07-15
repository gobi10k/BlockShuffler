# WINDOWS_SLICE_CHECKLIST.md — Windows execution slice

Derived from `docs/VALIDATION_PLAN.md` on HEAD `b829f75` (2026-07-15). Covers every row with a blank
Windows-Release cell + Section C (C1–C9). Work top to bottom on the Windows box; each group is one
session. Tick `[x]` and fill the plan's Windows-Release column (and Section A/B/C result cells) as you go.

**CI status (no Windows box needed for this part — already GREEN):**
- `b829f75` ✅ https://github.com/gobi10k/BlockShuffler/actions/runs/29419333753 — artifacts
  `BlockShuffler-Standalone-Windows` (3.36 MB) + `BlockShuffler-VST3-Windows` (3.51 MB)
- Also green: `ff56714` (run 29418931870), `243dce3` (run 29418551205). `0fe680a` and `c6c79a6` were
  covered by the ff56714 / b829f75 push-head runs respectively — all Carter-correction code is CI-built.

**Binary for the manual pass:** download `BlockShuffler-Standalone-Windows` from the b829f75 run (or build
locally: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release --target
BlockShuffler_Standalone`). Confirm the exe is from b829f75 before starting.

---

## Group W0 — MSVC harness (C9) — closes ALL objective rows for the Windows column
Run in "x64 Native Tools" (or any shell with CMake + MSVC):
`cmake -B build-diag && cmake --build build-diag --config Debug --target ResolverDiag && build-diag\ResolverDiag_artefacts\Debug\ResolverDiag.exe`

- [ ] **C9** — Harness builds and runs → `STEP6 RESULT: ALL PASS` (T1–T46 incl. T14b, T17b, reframed T17/T28).
      On ALL PASS, mark the Windows-Release cell of every harness-backed row with its T# (same set as the
      Mac column: 2.6, 2.7, 2.8, 3.1–3.5, 4.2–4.7, 5.1–5.10, 6.1, 6.2, 6.4, 7.1, 7.7, 8.1, 8.2, 9.1,
      10.2–10.5, 11.2, 11.5, 13.1–13.4 harness halves + Sections A/B harness halves).

## Group W1 — fresh 5-block project (Standalone exe)

- [ ] **1.1** — "+" ×5 → tile at strip end, palette colour, default name, each click.
- [ ] **1.2** — Delete key (block selected, no clip) removes; right-click → Delete Block removes (confirm if clips).
- [ ] **1.3** — Double-click rename + right-click rename → names persist after Enter / focus loss.
- [ ] **1.4** — Set Color → header + body adopt immediately.
- [ ] **13.1 (MAN)** — All 8 palette colours → true hues, yellow is YELLOW, no blue tint.
- [ ] **1.5 (MAN ×10)** — 10 consecutive gap-drops → all land at the released gap, zero snap-backs.
- [ ] **1.6 (MAN ×10)** — 10 drops ONTO blocks → stack first attempt, at DROP TARGET's slot, one Ctrl+Z reverts.
- [ ] **5.11** — Shift+drag whole stack; gap-drop unstacks in place; solo remainder auto-unstacks.
- [ ] **5.12** — One Mode + How-Many per stack; per-block chance sliders once with effective values.
- [ ] **13.2 (MAN)** — 9-block stack → every tile visible/reachable.
- [ ] **6.2 (MAN)** — Block Done → only dim/badge changes.
- [ ] **6.3 (MAN)** — DONE badge never covers the name (full-height + short stacked tiles).
- [ ] **12.3 (Windows half)** — Logo left of Save As, bg matches bar, ~2/3 bar height (drawn 37px/56px = 0.66 on Mac — should match).
- [ ] **GLYPHS** — Select a stacked block → inspector stack panel: bullet before member names, arrow after
      the selected member, and the "Group N · M blocks" middle dot ALL render as glyphs, not boxes/mojibake
      (CharPointer_UTF8 decode paths, fixed 15b0b58/243dce3-era).

## Group W2 — stress (Release exe)

- [ ] **12.1** — 50-block project: ≥20 fast mixed gap/stack drops, then 15 rapid Ctrl+Z → no crash.

## Group W3 — project with audio clips

- [ ] **2.1** — Explorer-drag WAV onto waveform → clip in SELECTED block, waveform renders.
- [ ] **2.2** — Explorer-drag WAV onto another block's tile → lands in THAT block.
- [ ] **2.3** — "+ Add Clip" → audio-filtered chooser → clip created.
- [ ] **2.4** — Clip Rename / Set Color / Remove all work.
- [ ] **2.5** — One weight slider drag → other clips' values unchanged.
- [ ] **2.8 (MAN)** — Marker snaps ON grid line; Shift+drag lands off-grid at pointer.
- [ ] **2.9** — Clip drag to a later block → strip scroll position unchanged.
- [ ] **2.7 (MAN spot)** — Set a clip to 0% → shows 0.0% effective, never plays; all-0% block skipped (Carter semantics).
- [ ] **7.1 (MAN)** — Repeated plays → arrangements vary per weights/links/stacks.
- [ ] **7.2** — Playing indicator visible on light AND dark blocks.
- [ ] **7.3** — Waveform follows, playhead moves, time updates.
- [ ] **7.4** — Selection unchanged during play; Stop reverts waveform to selection.
- [ ] **7.5 (×10)** — Play Block ×5 + Play Clip ×5 → 10/10.
- [ ] **7.6** — Rapid Play/Stop ×20 → no crash, no stuck audio.
- [ ] **B4 (MAN)** — Play Clip keeps lead-in audible into the next block.
- [ ] **5.1 / B2 (MAN ×10)** — Stack of 3, play 1: exactly ONE player every time, member varies.
- [ ] **5.5 (ear)** — SIM play 2 → two heard simultaneously.
- [ ] **5.8 (ear)** — Base ON: base every play + one other; OFF: all compete.
- [ ] **B1 (MAN)** — Clip AND block Done mid-session → both still play.
- [ ] **B3 (MAN ×5)** — 1&3 linked 100%, 2 between → 3,2,1 every play.
- [ ] **4.6 (MAN spot)** — Link a follower to a stack's BASE → whole stack swaps; to a non-base member → only that block (Carter semantics, T14/T14b on MSVC via C9).
- [ ] **3.x ear spot-check (advisory)** — Joins: tail under next lead-in, no click/gap; cross-tempo continuous.
- [ ] **9.2** — One click focuses tempo field; Enter commits.
- [ ] **9.3** — Block tempo sets all; per-clip override only that clip; new clips inherit.
- [ ] **9.4** — Tempo-less block: new clip gets project default.
- [ ] **9.1 (MAN)** — Tempo change: grid spacing changes, audio identical.
- [ ] **13.4 (MAN)** — Load a >5 min clip → zoom-in reaches the beat (~0.5 s window), grid legible (Carter semantics; also still PENDING on Mac).

## Group W4 — sessions + Windows integration

- [ ] **11.1 + TITLE** — Save (Ctrl+S), Save As (Ctrl+Shift+S), Open all work; **window title reads
      exactly `BlockShuffler - <project name>` — pure ASCII hyphen separator, no mojibake** (fix 15b0b58).
- [ ] **11.2 (MAN)** — 10-block project, full quit, reopen → ALL blocks visible instantly, settings intact.
- [ ] **11.3 / B5 / C6** — App closed: double-click .bsp in Explorer → loads. App RUNNING: double-click
      another .bsp → loads in the SAME instance (single-instance open-on-launch), both identical to Open.
- [ ] **C7** — .bsp association registered per WINDOWS_PACKAGING.md (installer/registry step); .bsp files
      show the app icon in Explorer and open on double-click.
- [ ] **C8** — BlockShuffler.exe shows the owl icon in Explorer AND the running window/taskbar shows it (not generic).
- [ ] **11.4** — Ctrl+Z immediately after load → nothing happens.
- [ ] **11.6** — Missing media → warning lists paths; project loads with silent clips.
- [ ] **A10/A11 (inspect)** — Saved .bsp in a text editor: int64s quoted strings; `media/...` forward slashes only.

## Group W5 — links

- [ ] **4.1** — Link create → arc + label with block NAMES + % (no UUIDs).
- [ ] **4.7 (MAN)** — Remove link → gone; one Ctrl+Z restores link AND its %.

## Group W6 — export

- [ ] **10.1** — Export chooser offers WAV, FLAC, BSF.
- [ ] **10.2 (MAN)** — Exported WAV vs in-editor playback (Audacity) → pitch/speed/crossfades match.
- [ ] **10.3 (MAN)** — .bsf → .zip: manifest.json + model.json + clips/*.flac; int64s quoted.

## Group W7 — cross-platform round-trips (needs files from the Mac)

- [ ] **C1** — Copy a Mac-saved project folder (.bsp + media/) to Windows → opens: all blocks/clips/settings
      intact, clips RESOLVE and PLAY.
- [ ] **C2** — Save a project on Windows, copy to the Mac → opens identically (reverse of C1).
- [ ] **C3** — After C1/C2: marker positions numerically identical to the source machine (compare inspector values).
- [ ] **C4** — Open both .bsp files in a text editor: audioFile entries all `media/...`, no `\` in either direction.
- [ ] **C5** — Same project + settings, export WAV on both OSes at matched sample rate → compare
      (Audacity invert+mix → silence, or checksum); document any float-rounding delta.

## Sign-off row

- [ ] **12.2** — With W0–W7 done: "Windows behaves same as macOS for groups 1–11" — mark PASS in the plan
      with the CI run URL + this checklist; fill every remaining Windows-Release cell.

## Already closed (do not re-run)

- Windows CI build (Configure/Build, Standalone + VST3 artifacts): GREEN on b829f75 — run 29419333753.
- Mac column: complete except 13.4's manual re-verify (pending on BOTH platforms — fresh builds contain the fix).
