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

- [No sure what this requires] **C9** — Harness builds and runs → `STEP6 RESULT: ALL PASS` (T1–T46 incl. T14b, T17b, reframed T17/T28).
      On ALL PASS, mark the Windows-Release cell of every harness-backed row with its T# (same set as the
      Mac column: 2.6, 2.7, 2.8, 3.1–3.5, 4.2–4.7, 5.1–5.10, 6.1, 6.2, 6.4, 7.1, 7.7, 8.1, 8.2, 9.1,
      10.2–10.5, 11.2, 11.5, 13.1–13.4 harness halves + Sections A/B harness halves).

## Group W1 — fresh 5-block project (Standalone exe)

- [x] **1.1** — "+" ×5 → tile at strip end, palette colour, default name, each click.
- [x] **1.2** — Delete key (block selected, no clip) removes; right-click → Delete Block removes (confirm if clips).
- [x] **1.3** — Double-click rename + right-click rename → names persist after Enter / focus loss.
- [x] **1.4** — Set Color → header + body adopt immediately.
- [x] **13.1 (MAN)** — All 8 palette colours → true hues, yellow is YELLOW, no blue tint.
- [x] **1.5 (MAN ×10)** — 10 consecutive gap-drops → all land at the released gap, zero snap-backs.
- [x] **1.6 (MAN ×10)** — 10 drops ONTO blocks → stack first attempt, at DROP TARGET's slot, one Ctrl+Z reverts.
- [x] **5.11** — Shift+drag whole stack; gap-drop unstacks in place; solo remainder auto-unstacks.
- [FAIL - more than 12 blocks the sliders are no longer visible, they appear when the window is resized, but resizing shouldn't be necessary'] **5.12** — One Mode + How-Many per stack; per-block chance sliders once with effective values.
- [x] **13.2 (MAN)** — 9-block stack → every tile visible/reachable.
- [x] **6.2 (MAN)** — Block Done → only dim/badge changes.
- [x] **6.3 (MAN)** — DONE badge never covers the name (full-height + short stacked tiles).
- [FAIL - image distorted and low quality] **12.3 (Windows half)** — Logo left of Save As, bg matches bar, ~2/3 bar height (drawn 37px/56px = 0.66 on Mac — should match).
- [x] **GLYPHS** — Select a stacked block → inspector stack panel: bullet before member names, arrow after
      the selected member, and the "Group N · M blocks" middle dot ALL render as glyphs, not boxes/mojibake
      (CharPointer_UTF8 decode paths, fixed 15b0b58/243dce3-era).

## Group W2 — stress (Release exe)

- [x] **12.1** — 50-block project: ≥20 fast mixed gap/stack drops, then 15 rapid Ctrl+Z → no crash.
- note that double clicking on the .bsp file doesn't open the app

## Group W3 — project with audio clips

- [x] **2.1** — Explorer-drag WAV onto waveform → clip in SELECTED block, waveform renders.
- [x] **2.2** — Explorer-drag WAV onto another block's tile → lands in THAT block.
- [x] **2.3** — "+ Add Clip" → audio-filtered chooser → clip created.
- [x] **2.4** — Clip Rename / Set Color / Remove all work.
- [x] **2.5** — One weight slider drag → other clips' values unchanged.
- [x] **2.8 (MAN)** — Marker snaps ON grid line; Shift+drag lands off-grid at pointer.
- [x] **2.9** — Clip drag to a later block → strip scroll position unchanged.
- [x] **2.7 (MAN spot)** — Set a clip to 0% → shows 0.0% effective, never plays; all-0% block skipped (Carter semantics).
- [x] **7.1 (MAN)** — Repeated plays → arrangements vary per weights/links/stacks.
- [x] **7.2** — Playing indicator visible on light AND dark blocks.
- [x] **7.3** — Waveform follows, playhead moves, time updates.
- [x] **7.4** — Selection unchanged during play; Stop reverts waveform to selection.
- [x] **7.5 (×10)** — Play Block ×5 + Play Clip ×5 → 10/10.
- [x] **7.6** — Rapid Play/Stop ×20 → no crash, no stuck audio.
- [x] **B4 (MAN)** — Play Clip keeps lead-in audible into the next block.
- [x] **5.1 / B2 (MAN ×10)** — Stack of 3, play 1: exactly ONE player every time, member varies.
- [x] **5.5 (ear)** — SIM play 2 → two heard simultaneously.
- [x] **5.8 (ear)** — Base ON: base every play + one other; OFF: all compete.
- [x] **B1 (MAN)** — Clip AND block Done mid-session → both still play.
- [x] **B3 (MAN ×5)** — 1&3 linked 100%, 2 between → 3,2,1 every play.
- [x] **4.6 (MAN spot)** — Link a follower to a stack's BASE → whole stack swaps; to a non-base member → only that block (Carter semantics, T14/T14b on MSVC via C9).
- [x] **3.x ear spot-check (advisory)** — Joins: tail under next lead-in, no click/gap; cross-tempo continuous.
- [x] **9.2** — One click focuses tempo field; Enter commits.
- [x] **9.3** — Block tempo sets all; per-clip override only that clip; new clips inherit.
- [x] **9.4** — Tempo-less block: new clip gets project default.
- [x] **9.1 (MAN)** — Tempo change: grid spacing changes, audio identical.
- [x] **13.4 (MAN)** — Load a >5 min clip → zoom-in reaches the beat (~0.5 s window), grid legible (Carter semantics; also still PENDING on Mac).

## Group W4 — sessions + Windows integration

- [x] **11.1 + TITLE** — Save (Ctrl+S), Save As (Ctrl+Shift+S), Open all work; **window title reads
      exactly `BlockShuffler - <project name>` — pure ASCII hyphen separator, no mojibake** (fix 15b0b58).
- [x] **11.2 (MAN)** — 10-block project, full quit, reopen → ALL blocks visible instantly, settings intact.
- [FAIL, doesn't work] **11.3 / B5 / C6** — App closed: double-click .bsp in Explorer → loads. App RUNNING: double-click
      another .bsp → loads in the SAME instance (single-instance open-on-launch), both identical to Open.
- [FAIL - unsure what this means] **C7** — .bsp association registered per WINDOWS_PACKAGING.md (installer/registry step); .bsp files
      show the app icon in Explorer and open on double-click.
- [x] **C8** — BlockShuffler.exe shows the owl icon in Explorer AND the running window/taskbar shows it (not generic).
- [x] **11.4** — Ctrl+Z immediately after load → nothing happens.
- [x] **11.6** — Missing media → warning lists paths; project loads with silent clips.
- [x] **A10/A11 (inspect)** — Saved .bsp in a text editor: int64s quoted strings; `media/...` forward slashes only.

## Group W5 — links

- [x] **4.1** — Link create → arc + label with block NAMES + % (no UUIDs).
- [x] **4.7 (MAN)** — Remove link → gone; one Ctrl+Z restores link AND its %.
- ONE NOTE OF FAILURE: If I delete the block that another block is linked to, the link doesn't disappear. It appears as 'unknown'

## Group W6 — export

- [x] **10.1** — Export chooser offers WAV, FLAC, BSF.
- [x] **10.2 (MAN)** — Exported WAV vs in-editor playback (Audacity) → pitch/speed/crossfades match.
- [FAIL - no option to extract, if I rename to /zip, it asks me to first 'copy files to this compressed folder'] **10.3 (MAN)** — .bsf → .zip: manifest.json + model.json + clips/*.flac; int64s quoted.

## Group W7 — cross-platform round-trips (needs files from the Mac)

- [x] **C1** — Copy a Mac-saved project folder (.bsp + media/) to Windows → opens: all blocks/clips/settings
      intact, clips RESOLVE and PLAY.
- [x] **C2** — Save a project on Windows, copy to the Mac → opens identically (reverse of C1).
- [x] **C3** — After C1/C2: marker positions numerically identical to the source machine (compare inspector values).
- [x] **C4** — Open both .bsp files in a text editor: audioFile entries all `media/...`, no `\` in either direction.
- [x] **C5** — Same project + settings, export WAV on both OSes at matched sample rate → compare
      (Audacity invert+mix → silence, or checksum); document any float-rounding delta.

## Sign-off row

- [Some failures] **12.2** — With W0–W7 done: "Windows behaves same as macOS for groups 1–11" — mark PASS in the plan
      with the CI run URL + this checklist; fill every remaining Windows-Release cell.

## Already closed (do not re-run)

- Windows CI build (Configure/Build, Standalone + VST3 artifacts): GREEN on b829f75 — run 29419333753.
- Mac column: complete except 13.4's manual re-verify (pending on BOTH platforms — fresh builds contain the fix).

## Post-slice resolutions (2026-07-16) — every FAIL above is closed

- **5.12 FAIL** (sliders hidden past ~12 blocks) → fixed e4291ea (content-driven inspector height, T49 backstop); re-verified.
- **12.3 FAIL** (logo distorted) → fixed 355edaa (cached highResamplingQuality rescale); re-verified crisp.
- **11.3 / C6 / C7 FAIL** (.bsp double-click / association) → fixed f8dba5d (app self-registers the .bsp association in HKCU on launch — no installer needed); re-verified.
- **W5 note** (deleted linked block leaves an "unknown" link) → fixed 23f5462 (removeBlock prunes links in the same undo entry, T48 guards it).
- **10.3 FAIL** (.bsf won't open in Explorer/7-Zip) → NOT a format bug: the .bsf is a valid standard ZIP (unzip -t clean). Explorer only opens archives literally named `*.zip`; rename .bsf→.zip (not "/zip") or use 7-Zip's right-click → Open archive.
- **C9 / W0** → MSVC harness now gated in CI on every push (run 29492810846 on f51e2ac: `STEP6 RESULT: ALL PASS`, T1–T49).
- **12.2 sign-off** → PASS; see VALIDATION_PLAN.md Windows column (fully filled 2026-07-16).
