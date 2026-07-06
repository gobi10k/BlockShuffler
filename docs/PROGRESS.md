# PROGRESS.md — BlockShuffler running log

**This is the cross-session memory. READ THIS FIRST every session. UPDATE IT LAST every session.**
Claude Code has no memory between sessions except this file. If it isn't written here, the next session doesn't know it.

Format: newest entry at the top. Each session appends a dated block. Keep the "CURRENT STATUS" and "NEXT UP" sections at the very top always accurate.

---

## CURRENT STATUS (keep this current)
- Stage: FINAL DELIVERY PUSH against locked spec. Client deadline ~2026-07-17.
- Build: compiles; macOS arm64 primary. Windows build exists, path handling made cross-platform (relative, forward slashes) — parity not yet re-verified.
- Last full audit: 0 critical / 0 high / 0 medium after fixes.

### BROKEN / TODO (in priority order)
1. ~~**Stack play count regression**~~ — **CLOSED 2026-07-05: does not reproduce.** Verified honored in current code (headless harness + in-app DBG + by ear). PROGRESS entry below has the proof. Keep the jassert guard at ArrangementResolver.cpp:319.
2. ~~**Simultaneous layering**~~ — **CLOSED 2026-07-05 session 2: MASTER_PROMPT Step 2 COMPLETE.** Grep half PASSED; runtime half PASSED (SIM play 3 → 10/10 identical timelinePos, cursor advances by LONGEST body; proof in entry below); layering confirmed BY EAR by the user. No code change was needed.
3. ~~**NEW FEATURE: "Always play base block"**~~ — **CLOSED 2026-07-06: MASTER_PROMPT Step 3 COMPLETE.** 3A–3E committed with proof (session-3 entry below); 3F user-confirmed 2026-07-06: all six manual tests pass (ear test base ON play 2 of 3, toggle visibility per mode, undo/redo, save→reopen persistence, pre-3B project loads with toggle off).
4. ~~**Effective % = inclusion probability**~~ — **CLOSED 2026-07-06: MASTER_PROMPT Step 4 COMPLETE (user-confirmed incl. amendment).** Shared StackPicker between resolver and display (4A pure extraction, 4B MC via same pick(), 4C fingerprint-gated recompute triggers — no recompute on Play). Amendment: deterministic seeding from stack-state hash (identical state → bit-identical numbers across Plays/recomputes/save-reopen); trials at 20000 (amendment's 50000 measured 130ms Debug on a 6-block stack, over the 50ms budget → dropped per its own fallback; 20000 ≈ 52ms). ACCEPTED KNOWN LIMITATION: equal weights may display 33/34/34 (true 33.33 is 0.17 from the rounding boundary vs ~0.33 MC standard error) — within client tolerance of ±2; exact-enumeration fix scoped but DEFERRED.
5. **Logo polish** — background must match transport bar colour exactly, larger, sit just left of Save As. Verify logo + app icon load via BinaryData (cross-platform), not runtime file paths. ACCEPTANCE_TESTS 12.3.
6. **Windows parity pass** — ACCEPTANCE_TESTS 12.2.
7. **NEW (found 2026-07-05): juce_Colour.cpp:340 assertion spam** — fires continuously while painting during playback with audio loaded (Debug builds). Something constructs a Colour with an out-of-range float value. Find and fix (likely waveform/playhead/indicator paint code; maybe touch alongside item 5).
8. ~~**Source/ is UNTRACKED in git**~~ — **CLOSED 2026-07-05 session 2.** Full tree committed on `UI_firstdraft` and tagged `baseline-step1-clean`. IMPORTANT: the old "repo" was accidentally rooted at `$HOME` (that's why Source/ looked untracked); a proper repo now lives at the project directory with the UI_firstdraft history imported and origin set (details in entry below). One commit per completed MASTER_PROMPT step from now on.

### NEXT UP
MASTER_PROMPT Step 5 (permanent guards) + Step 6 harness half, then user runs the Step 6 manual checklist (in the 2026-07-06 entry below). Step 7 (cleanup: DBG removal, diag/ teardown, clean rebuild both configs) starts ONLY after the user confirms that checklist. Then remaining ACCEPTANCE_TESTS groups, logo (item 5), Windows parity (item 6), Colour assertion (item 7).

---

## VERIFIED WORKING (do not re-break)
Core sequential playback; entry-0 full-gain lead-in; lead-in/tail crossfades via shared EntryMixer.h (playback + export identical); WSOLA stretch with retain flags; block add/remove/rename/colour; drag reorder + drag-to-stack + drag-to-gap-unstack + Shift+drag whole stack + rearrange within stack; clip add (Finder / block tile / browse) + drag clips between blocks + independent per-clip weights + song enders + per-clip play; markers with grid snap + Shift bypass + adaptive grid + proportional zoom persistence + arrow nudge; one-click tempo field + block tempo + per-clip override + project default tempo; links create/remove + name labels (no UUIDs) + collision-avoided arc labels; white playing indicator + waveform follows playing block + playhead line + time display + selection independent of playback + Play Block/Play Clip; Save/Save As/Open + window title + single synchronous loadProject() + relative portable paths + missing-file warning + undo cleared on load; snapshot undo + one-entry-per-drag + undoable stack-count buttons; export WAV/FLAC/BSF + int64 strings + project-rate writers; app icon wired.

---

## SESSION LOG

### 2026-07-06 (session 2) — Step 4 CLOSED; Step 5 guards + Step 6 DONE — **Step 6 CLOSED 2026-07-06: harness 12/12 + manual checklist 7/7 user-confirmed.**
- Step 0: Step 4 marked CLOSED (user-confirmed incl. amendment; known limitation accepted, client tolerance ±2); stale root PROGRESS.md removed — docs/PROGRESS.md is the only log (c41f164).
- What I changed (files):
  - `Source/Audio/ArrangementResolver.cpp` — Step 5: four-point invariant comment block at top (isDone cosmetic-only / playCount honored via StackPicker / links bidirectional on local position map / sequential timeline gapless); violation DBG added next to the permanent per-stack jassert guard (lines 312–315). grep isDone over Source/Audio/: all hits comments, zero functional.
  - `diag/ResolverDiag.cpp` — Step 6 harness half: T1–T12 with per-test PASS/FAIL + evidence; `addClipWithFile()` helper writes real temp WAVs for T11/T12. TEMPORARY, remove in Step 7.
- Commits: c41f164 (Step 0), c507dc1 (Step 5), Step 6 commit below.
- What I proved (PASS/FAIL): T1–T12 ALL PASS (details in the Step 6 commit / harness output). Highlights: T2 zero-gap sequential timeline 20/20 with random order (3 distinct first blocks); T5 next block lands at +longest body (1500) 10/10; T9 link 1↔3 @100% → Z,Y,X 10/10 AND model block->position values unmutated (dumped before/after, identical); T10 isDone proof is EXACT — same RNG seed with isDone toggled produces bit-identical 100-resolve pick sequences (done block included 31/100 both ways); T11 fields + entry structure identical across save→load; T12 base/playCount/weight mutations each revert to string-identical model JSON. Full pre-existing suite (Steps 1/2/3C/4A/4B/amendment) re-ran green in the same binary.
- What regressed or surprised me: T11/T12 first FAILED — diagnosis showed a HARNESS artifact, not a product bug: harness clips had no on-disk audio file, so resetAndLoad's mark clamp (Serialization.cpp:200–201, FIX M7) zeroed endMark (bufLen=0 with the file missing). In-app, files exist and marks survive (long-verified). Fixed the TESTS to use real temp WAV files (the app's actual condition) — zero product code changed for Step 6.
- STEP 6 MANUAL CHECKLIST (user-run; the ear/UI halves the harness can't see). App: `open "build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app"`. Setup once: 3 blocks with clearly distinguishable clips (e.g. drums/bass/vocals) → drag block 2 onto block 1, drag block 3 onto the stack → 3-block stack; add a 4th unstacked block after it.
  1. T2 by ear — inspector: mode = Sequential, How Many to Play = 2 → Play several times: two different clips back-to-back with NO audible gap between them, order varies across plays.
  2. T4/T5 by ear — mode = Simultaneous, play 2 → exactly two clips AT THE SAME TIME; play 3 → all three layered, starting together.
  3. T5 next-block wait — with play 3, the 4th block must start only after the LONGEST of the three stack clips finishes (not after the first/shortest).
  4. Lead-in/crossfade — use clips with lead-in audio before the start marker: entry 0's lead-in plays at full gain from t=0; later transitions crossfade tail→lead-in smoothly (no click, no dropout).
  5. Project open — Save As, quit, reopen the .bsp: all blocks visible IMMEDIATELY in the strip (no blank strip, no delayed paint), stack intact.
  6. T12 in-app undo feel — toggle base → Cmd+Z flips it back (ONE step per click); playCount +/− → one undo step per click; drag a weight slider end-to-end → ONE undo step for the whole drag; redo (Cmd+Shift+Z) replays each cleanly.
  7. isDone visual-only — right-click a stacked block → Mark as Done → it dims/greys with the done badge but STILL plays per its normal odds (Play repeatedly; it must keep appearing).
- NEXT SESSION should: after user confirms this checklist, run MASTER_PROMPT Step 7 (remove Step 1 DBG lines + diag/ harness + ResolverDiag CMake target, KEEP the jassert guard + violation DBG + invariant block; clean rebuild both configs; final report). Then ACCEPTANCE_TESTS groups, logo (item 5), Windows parity (item 6), Colour assertion (item 7).

### 2026-07-06 — Step 3 CLOSED; MASTER_PROMPT Step 4 built (4A–4C committed), awaiting 4D user confirmation
- Step 0: user confirmed all six 3F manual tests → Step 3 marked complete (232657a); branch pushed, origin now current (03d6fe3..232657a → then 4A–4C pushed on top).
- What I changed (files):
  - `Source/Audio/StackPicker.h` — NEW. Shared stack-selection routine: `StackPicker::pick()` (playCount draw + clamp [1, groupSize], alwaysPlayBase branch, weighted sample WITHOUT replacement, all-zero weights → UNIFORM fallback) extracted VERBATIM from the resolver, plus `StackPicker::inclusionProbabilities()` (Monte Carlo 2000 trials driving pick(); shortcut: min playCount >= groupSize → all 100% with no simulation).
  - `Source/Audio/ArrangementResolver.cpp` — stack slot now calls `StackPicker::pick()`; nothing else changed. RNG call order preserved. NOTE: the stack pick's all-zero-weights fallback was ALREADY uniform (no getLast() in that path — the getLast() hits are in pickClip's float-edge fallback, untouched) → 4A was a zero-behavior-change extraction.
  - `Source/UI/InspectorPanel.h/.cpp` — `computeStackInclusionProbabilities()` is now a thin wrapper over `StackPicker::inclusionProbabilities()` (old reimplemented sampling deleted — it had a last-element fallback and no alwaysPlayBase awareness). 4C: `recalcStackEffectiveLabels()` gates the MC behind a per-group state fingerprint (mode, alwaysPlayBase, stackPlayCount values+weights, member ids + playChances); recompute happens ONLY when that state changes = weight drag end / playCount +/- / base toggle / membership change / project load (and undo of those). The Play-time recompute (updateTimeDisplay play-follow → inspectorPanel.setBlock → rebuild → fresh time-seeded MC) is now a cache hit; setBlock itself intentionally kept ("inspector follows playing block" is a VERIFIED WORKING feature).
  - `diag/ResolverDiag.cpp` — STEP4A (all-zero weights SEQ pc=1, 20 resolves) and STEP4B (six inclusion-% scenarios via the exact shared function). TEMPORARY, remove in Step 7.
  - `docs/PROGRESS.md` — this entry.
- Commits: 232657a (Step 3 close), 582b598 (4A shared StackPicker), 8361865 (4B inclusion calc), 647b084 (4C recompute triggers).
- What I proved (PASS/FAIL):
  - 4A PASS: full harness re-run at previous rates — Step 1 scenarios (SEQ pc=1, SIM pc=1, SIM pc=2 identical timelinePos, JSON round-trip) 10/10 each; Step 2 (SIM pc=3 → identical timelinePos, cursor += longest body: D@2500) 10/10; Step 3C six scenarios 20/20 each incl. baseOFF regressions (SIM pc=1 basePresent 7/20 — same as pre-refactor). NEW STEP4A: all-zero weights SEQ pc=1 → entries==1 20/20, block varies (A=6 B=4 C=10).
  - 4B PASS (2000-trial MC, shared function): 3 equal pc=1 → 33.6/33.9/32.5 (±2 ✓); pc=2 → 67.6/65.2/67.2 (±2 ✓); pc=3 of 3 → 100.0 flat (shortcut, no simulation); SIM baseON pc=2 → base 100.0, others 48.6/51.4 (±3 ✓); weights 80/10/10 pc=1 → 80.3/10.0/9.7; all-zero → 33.4/31.6/35.0 (uniform fallback).
  - 4C PASS (static): remaining recompute paths all route through the fingerprint gate; Play leaves the model untouched → fingerprint identical → no recompute. Debug standalone builds clean (0 errors).
  - Scope freeze honored: link logic, isDone, lead-in, PlaybackEngine mixing, sequential timeline math untouched; jassert guard + Step 1 DBG lines still in place (removal is Step 7).
- What regressed or surprised me: nothing. The suspected getLast() fallback in the stack picker did not exist (already uniform); the only getLast() is pickClip's roll-edge fallback, out of 4A scope.
- Step 4D hand-off (user-run; Step 4 NOT complete until confirmed):
  1. `open "build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app"`
  2. 3 blocks, one clip each → stack all three (drag 2 onto 1, then 3 onto the stack). Inspector shows the three block rows with weight sliders + "eff:" labels.
  3. Four hand-checked numbers (all weights equal at 100): How Many to Play = 1 → eff ≈ 33% each; = 2 → ≈ 67% each; = 3 → 100% each (exact); then mode = Simultaneous, "Always play base block" ON, play count 2 → (base) row 100%, others ≈ 50% — the flip to 100/50/50 must appear IMMEDIATELY on toggling.
  4. Play-stability: note the three eff values, press Play, Stop, Play several times → the numbers must NOT change at all (previously they jiggled on every Play).
  5. Weight drag: drag a weight slider — eff must stay frozen DURING the drag and update once on release.
  6. Save As → close → reopen: eff values shown immediately are EXACTLY identical to pre-save (deterministic seed — see amendment below); the base row must still read 100%.
- STEP 4 AMENDMENT (same day): `StackPicker::inclusionProbabilities` made DETERMINISTIC — RNG seeded from a hash of the stack state (mode, alwaysPlayBase, playCount values+weights, member ids + playChances), so identical state → bit-identical numbers across recomputes, Plays, and save/reopen. Trials 2000 → 20000 (NOT the amendment's 50000: that measured 130ms on a 6-block stack in Debug, over the 50ms budget, so dropped to 20000 per the amendment's own fallback; 20000 ≈ 52ms Debug). Display already rounded to whole % (InspectorPanel.cpp `roundToInt`). `StackPicker::pick()` untouched; full harness re-run at previous rates. KNOWN LIMITATION (reported to user, awaiting decision): with 3 equal weights / pc=1, the true value 33.33 sits 0.17 from the 33/34 rounding boundary while MC standard error at 20k is ~0.33 — so roughly 4 out of 5 freshly-created stacks freeze a display of 33/34/33-style inequality (each stack's UUIDs seed a different frozen draw). No feasible trial count fixes this (needs ~500k+). Candidate fix if client insists on visually equal display: exact enumeration for small stacks (n ordered selections = n!/(n−pc)! — trivial for real stack sizes) with MC fallback — but that reimplements selection math outside pick(), which Step 4B forbade, so it needs an explicit user decision.
- NEXT SESSION should: after user confirms 4D, mark Step 4 complete; then Step 5 (regression guards), Step 6 (12-point re-test), Step 7 (cleanup: remove diag/ harness + STEP1-4 DBG/diag code, keep jassert guard).

### 2026-07-05 (session 3) — MASTER_PROMPT Step 3 built (3A–3E committed), awaiting 3F user confirmation
- Step 0 backup: remote `UI_firstdraft` held a stale 2026-04-29 orphan snapshot (strict subset of local tree, zero remote-only files) — replaced via `git push --force-with-lease`; branch now tracks origin. NOTE: that push happened BEFORE 3A — origin is at 03d6fe3; commits 30f67b4..HEAD are local only. Next session: `git push` first.
- What I changed (files):
  - `Source/Model/Block.h` — `bool alwaysPlayBase = false;` (stack-level, simultaneous only).
  - `Source/Model/Project.cpp` — propagateStackSettings copies alwaysPlayBase across the group.
  - `Source/Model/Serialization.cpp` — toJSON writes / fromJSON reads "alwaysPlayBase" (missing key → false; pre-3B projects load unchanged).
  - `Source/Audio/ArrangementResolver.cpp` — simultaneous pick only: if alwaysPlayBase, base block (FIRST of the stack group in project->blocks order) is pre-picked, remaining (playCount−1) weighted-sampled without replacement from the rest; playCount==1 → base only. `isSimultaneous` hoisted above the pick (only structural move). Sequential/standalone paths untouched; jassert guard intact (now line 345).
  - `Source/UI/InspectorPanel.h/.cpp` — "Always play base block" ToggleButton, visible ONLY when mode == Simultaneous; undo = local pre-snapshot → mutate → propagateStackSettings → applyExternalMutation (recordMutation, suppressUndo-aware); "(base)" suffix on the base block's row in the combined stack view (shown in Simultaneous mode); mode flips rebuild labels + layout via lastBuiltSimMode.
  - `Source/Audio/ExportRenderer.cpp` — BSF model.json blocks now carry "alwaysPlayBase".
  - `CMakeLists.txt` — ExportRenderer.cpp added to ResolverDiag target (diag-only; Step 7 removes target).
  - `diag/ResolverDiag.cpp` — STEP3B (serialization round-trip + stripped-key load), STEP3C (6 scenarios × 20 resolves), STEP3E (headless renderToBsf → print model.json). All TEMPORARY, remove in Step 7.
  - `docs/PROGRESS.md` — this entry.
- Commits: 30f67b4 (3A model+propagation), 182760d (3B serialization), 18f158e (3C resolver), 1610703 (3D inspector+undo), d424800 (3E BSF export).
- What I proved (PASS/FAIL):
  - 3B PASS: save→load alwaysPlayBase=true on both stacked blocks; JSON with key stripped → false.
  - 3C PASS 20/20 each: SIM baseON pc=2 → base present 20/20, exactly 2 entries, identical timelinePos; pc=1 → base only; pc=3 → all three. REGRESSION base OFF: SIM pc=1 → 1 entry, base only 7/20 (varies); SEQ pc=1 → 1 entry; SEQ pc=2 → 2 entries ascending timeline. `git diff` confirmed changes confined to the simultaneous pick.
  - 3E PASS: headless BSF export, model.json stack blocks show `"alwaysPlayBase": true`.
- What regressed or surprised me: nothing; all regression scenarios green.
- Step 3F hand-off (user-run; Step 3 NOT complete until confirmed):
  1. `open "build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app"`
  2. 3 blocks with distinguishable clips → stack all three → mode = Simultaneous. The first block of the stack shows "(base)" in the inspector's stack view; the "Always play base block" toggle appears under Play Mode (and disappears in Sequential).
  3. Toggle ON, How Many to Play = 2 → several Plays: the (base) block sounds EVERY time plus exactly one other.
  4. Cmd+Z / Cmd+Shift+Z: toggle flips back/forth, one undo step per click.
  5. Save As → reopen: toggle still ON.
  6. Open any project saved before today → toggle OFF.
- NEXT SESSION should: after user confirms 3F, mark Step 3 complete; then Step 4 (effective % — must teach computeStackInclusionProbabilities about alwaysPlayBase and share the picker with the resolver).

### 2026-07-05 (session 2) — VCS baseline + Step 2 runtime half PASSED (ear check pending)
- What I changed (files):
  - `.git/` — NEW repo initialised at the project directory. Root cause of "Source/ untracked": the previous repo was rooted at `/Users/alecgordon` ($HOME), tracking a stale April copy at `~/Source` plus dead `Downloads/` worktrees; nothing under this project dir was tracked. Fix: `git init` here, fetched `UI_firstdraft` history from the home repo (GitHub workflow commits preserved), pointed the branch at it without touching files, set `origin` to github.com/gobi10k/BlockShuffler.git. Home repo untouched.
  - `.gitignore` — rewritten: build*/ (covers build-diag/), Builds/, CMake cache dirs, *.app/*.vst3/*.component/*.o/*.a, xcuserdata, .DS_Store & friends, terminal_output.txt, .claude/.
  - `diag/ResolverDiag.cpp` — added STEP2 scenario: 3-block SIM stack playCount=3 with DISTINCT body lengths (1000/2500/1800) + trailing unstacked block D(700) to expose the cursor; `addClipTo` gained a bodyLen param. TEMPORARY, remove in Step 7.
  - `docs/PROGRESS.md` — this entry.
  - No functional code changed anywhere. No resolver fix was needed.
- Commits (baseline + one per step from now on):
  - `828c5a4` "Baseline: full tree before final delivery push (Step 1 verified clean)" — tagged `baseline-step1-clean`. 45 files under Source/ tracked; `git status --short` empty.
  - `641c345` "Step 2 (runtime half): extend ResolverDiag with SIM play-3 layering/cursor case".
- What I proved (PASS/FAIL):
  - PASS: Part B residual — jassert guard (ArrangementResolver.cpp:326) and Step 1 DBG lines (222, 226) still present. No client .bsp was provided → load-path dump skipped.
  - PASS (10/10): SIM playCount=3 → 3 entries, ALL at identical timelinePos=0, every resolve; DBG stderr shows `playCount=3 / picked=3` each run.
  - PASS (10/10): cursor advance = LONGEST body — trailing block D lands at 2500 (longest body), not 5300 (sum) and not the first-picked block's length.
  - PASS: no Step 1 regression — same binary re-ran SEQ play-1 and SIM play-1: exactly 1 entry, 10/10 each.
- What regressed or surprised me:
  - The $HOME-rooted repo (above). Also note: a second stale project copy still sits at `~/Source` etc. and dead copies in `~/Downloads` — candidates for manual deletion by the user, NOT by Claude.
- Step 2 ear check: **PASSED — user confirmed layering by ear same day. STEP 2 COMPLETE.** Steps used:
  1. `open "build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app"` (or run the inner binary from a terminal to also see DBG).
  2. Add 3 blocks, one clearly distinguishable clip each (e.g. drums / bass / vocals).
  3. Drag block 2 onto block 1, then block 3 onto the stack → 3-block stack.
  4. Inspector: stack mode = Simultaneous, How Many to Play = 3.
  5. Press Play several times: all three clips must sound AT THE SAME TIME, starting together, each at ~1/3 gain; the next block (add one after the stack to check) must start only after the LONGEST of the three finishes.
- NEXT SESSION should:
  1. MASTER_PROMPT Step 3 ("Always play base block"), then 4–7 in order. Do not touch link logic, isDone, lead-in, or the Colour assertion (item 7) until their steps.

### 2026-07-05 — MASTER_PROMPT Step 1: stack play-count regression DOES NOT REPRODUCE
- What I changed (files):
  - `Source/Audio/ArrangementResolver.cpp` — added the two Step 1 DBG lines (grp/mode/playCount/groupSize + picked). TEMPORARY, remove in Step 7.
  - `diag/ResolverDiag.cpp` + `ResolverDiag` target in `CMakeLists.txt` — headless harness: builds a 3-block stack via the real model APIs (addBlock/stackBlocks/propagateStackSettings), resolves 10× per scenario, dumps model state. TEMPORARY, remove in Step 7.
  - `docs/PROGRESS.md` — this entry.
  - No functional code changed. No fix was needed.
- What I proved (grep/DBG/tests, PASS/FAIL):
  - PASS (static): resolver already implements the required invariant — `playCount = stackPlayCount.pick(rng)` clamped to [1, groupSize], weighted sample WITHOUT replacement, entries created only from picked subset (ArrangementResolver.cpp:186–216). None of Step 1's hypotheses (a)/(b)/(c) present.
  - PASS (headless, Debug build): SEQ play 1 → 10/10 resolves exactly 1 entry. SIM play 1 → 10/10 exactly 1. SIM play 2 → 10/10 exactly 2, identical timelinePos. Play count edit + propagate honored. JSON round-trip via resetAndLoad (the undo path) preserves sg/mode/playCount and still resolves correctly.
  - PASS (in-app, Debug standalone, real audio, user-run): 13 Play presses → 13 resolves, every one `STACK grp=0 mode=Seq playCount=1 groupSize=3 / picked=1`; exactly ONE block audible per press (user-confirmed).
  - PASS (Step 2 grep half, early): `grep -n "break\|continue\|isOverlay" Source/Audio/PlaybackEngine.cpp` → no isOverlay references, no offending break; the only `continue` is the correct window-skip at PlaybackEngine.cpp:105. Engine mixes all entries overlapping the buffer window and can only play resolved entries.
  - Conclusion: the regression was fixed at some earlier point (fossils: jassert guard at ArrangementResolver.cpp:319 whose comment describes the exact old failure — slot-splitting → standalone playChance gate; FIX H6/H7 stack-state resets). PROGRESS item 1 was stale.
- What regressed or surprised me:
  - `Source/` is completely untracked in git (branch UI_firstdraft tracks only workflow files) — no history to bisect, and a major data-loss risk. See TODO item 8.
  - Debug builds spam `JUCE Assertion failure in juce_Colour.cpp:340` continuously while painting during playback with audio — out-of-range float passed to a Colour constructor somewhere in paint code. See TODO item 7.
  - Build note: diagnostic Debug build lives in `build-diag/` (Standalone app: `build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app`). Run the binary inside from a terminal to see DBG output.
- NEXT SESSION should:
  1. Commit Source/ (+ docs, CMakeLists, Resources) to git before touching anything else.
  2. MASTER_PROMPT Step 2 remainder: 3-block SIM stack, play 3 → paste DBG showing three entries at identical timelinePos, confirm layering by ear. Then Steps 3–7 in order (Step 7 cleanup must remove the Step 1 DBG lines AND the diag/ harness + CMake target).

### 2026-07-04 — Project/docs restructure (planning session, no code)
- Consolidated all context into repo docs so Claude Code has continuity: added root `CLAUDE.md`, this `PROGRESS.md`, and `docs/` (SPEC, ACCEPTANCE_TESTS, MASTER_PROMPT).
- Client (Carter) locked the spec and confirmed effective-probability semantics (1-of-3 equal = 33%, 3-of-3 = 100%) — validates the Monte-Carlo inclusion-probability approach.
- Only genuinely new feature remaining: "Always play base block" for simultaneous stacks. Everything else is fix-and-verify.
- NEXT SESSION: start `docs/MASTER_PROMPT.md` Step 1 (diagnose the stack play-count regression with logging; confirm root cause before fixing).

<!-- Template for the next entry:
### YYYY-MM-DD — <short title>
- What I changed (files):
- What I proved (grep/DBG/tests, PASS/FAIL):
- What regressed or surprised me:
- NEXT SESSION should:
-->
