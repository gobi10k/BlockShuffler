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
2. **Simultaneous layering** — grep half of MASTER_PROMPT Step 2 already PASSED (no isOverlay/break offenders in PlaybackEngine; entries share timelinePos in harness at play 2). Remaining: 3-block SIM stack at play 3, confirm layering by ear.
3. **NEW FEATURE: "Always play base block"** (simultaneous only) — not yet built. Model + serialization + resolver + inspector toggle + BSF. MASTER_PROMPT Step 3.
4. **Effective % = inclusion probability** — verify against client numbers (33 / 67 / 100). Share the picker function between resolver and display. MASTER_PROMPT Step 4.
5. **Logo polish** — background must match transport bar colour exactly, larger, sit just left of Save As. Verify logo + app icon load via BinaryData (cross-platform), not runtime file paths. ACCEPTANCE_TESTS 12.3.
6. **Windows parity pass** — ACCEPTANCE_TESTS 12.2.
7. **NEW (found 2026-07-05): juce_Colour.cpp:340 assertion spam** — fires continuously while painting during playback with audio loaded (Debug builds). Something constructs a Colour with an out-of-range float value. Find and fix (likely waveform/playhead/indicator paint code; maybe touch alongside item 5).
8. **NEW (found 2026-07-05): Source/ is UNTRACKED in git** — only workflow files are committed on branch UI_firstdraft. `git ls-files Source` is empty. Commit the tree ASAP; two weeks from deadline with zero VCS coverage is the single biggest project risk.

### NEXT UP
Commit Source/ to git (item 8). Then resume `docs/MASTER_PROMPT.md` at Step 2's remaining ear-check, then Steps 3–7 in order. Then remaining ACCEPTANCE_TESTS groups, logo (item 5), Windows parity (item 6), Colour assertion (item 7).

---

## VERIFIED WORKING (do not re-break)
Core sequential playback; entry-0 full-gain lead-in; lead-in/tail crossfades via shared EntryMixer.h (playback + export identical); WSOLA stretch with retain flags; block add/remove/rename/colour; drag reorder + drag-to-stack + drag-to-gap-unstack + Shift+drag whole stack + rearrange within stack; clip add (Finder / block tile / browse) + drag clips between blocks + independent per-clip weights + song enders + per-clip play; markers with grid snap + Shift bypass + adaptive grid + proportional zoom persistence + arrow nudge; one-click tempo field + block tempo + per-clip override + project default tempo; links create/remove + name labels (no UUIDs) + collision-avoided arc labels; white playing indicator + waveform follows playing block + playhead line + time display + selection independent of playback + Play Block/Play Clip; Save/Save As/Open + window title + single synchronous loadProject() + relative portable paths + missing-file warning + undo cleared on load; snapshot undo + one-entry-per-drag + undoable stack-count buttons; export WAV/FLAC/BSF + int64 strings + project-rate writers; app icon wired.

---

## SESSION LOG

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
