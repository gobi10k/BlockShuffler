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
4. **Effective % = inclusion probability** — verify against client numbers (33 / 67 / 100). Share the picker function between resolver and display. MASTER_PROMPT Step 4.
5. **Logo polish** — background must match transport bar colour exactly, larger, sit just left of Save As. Verify logo + app icon load via BinaryData (cross-platform), not runtime file paths. ACCEPTANCE_TESTS 12.3.
6. **Windows parity pass** — ACCEPTANCE_TESTS 12.2.
7. **NEW (found 2026-07-05): juce_Colour.cpp:340 assertion spam** — fires continuously while painting during playback with audio loaded (Debug builds). Something constructs a Colour with an out-of-range float value. Find and fix (likely waveform/playhead/indicator paint code; maybe touch alongside item 5).
8. ~~**Source/ is UNTRACKED in git**~~ — **CLOSED 2026-07-05 session 2.** Full tree committed on `UI_firstdraft` and tagged `baseline-step1-clean`. IMPORTANT: the old "repo" was accidentally rooted at `$HOME` (that's why Source/ looked untracked); a proper repo now lives at the project directory with the UI_firstdraft history imported and origin set (details in entry below). One commit per completed MASTER_PROMPT step from now on.

### NEXT UP
MASTER_PROMPT Step 4 (effective % = inclusion probability; computeStackInclusionProbabilities does NOT yet know about alwaysPlayBase — that is Step 4 work). Then Steps 5–7. Then remaining ACCEPTANCE_TESTS groups, logo (item 5), Windows parity (item 6), Colour assertion (item 7).

---

## VERIFIED WORKING (do not re-break)
Core sequential playback; entry-0 full-gain lead-in; lead-in/tail crossfades via shared EntryMixer.h (playback + export identical); WSOLA stretch with retain flags; block add/remove/rename/colour; drag reorder + drag-to-stack + drag-to-gap-unstack + Shift+drag whole stack + rearrange within stack; clip add (Finder / block tile / browse) + drag clips between blocks + independent per-clip weights + song enders + per-clip play; markers with grid snap + Shift bypass + adaptive grid + proportional zoom persistence + arrow nudge; one-click tempo field + block tempo + per-clip override + project default tempo; links create/remove + name labels (no UUIDs) + collision-avoided arc labels; white playing indicator + waveform follows playing block + playhead line + time display + selection independent of playback + Play Block/Play Clip; Save/Save As/Open + window title + single synchronous loadProject() + relative portable paths + missing-file warning + undo cleared on load; snapshot undo + one-entry-per-drag + undoable stack-count buttons; export WAV/FLAC/BSF + int64 strings + project-rate writers; app icon wired.

---

## SESSION LOG

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
