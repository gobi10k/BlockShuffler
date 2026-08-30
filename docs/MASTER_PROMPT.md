# MASTER_PROMPT.md — paste this into Claude Code as one prompt

FINAL DELIVERY PUSH. The client has locked the spec (see SPEC.md in the repo — add it from the project docs if not present) and set a 2-week deadline. This prompt fixes the current stack regression, implements the one new required feature, and locks the semantics with guards. Work through the steps in order. For every step, paste proof (grep output, function bodies, DBG output). Do not claim a step done without proof. Do not touch anything outside the listed scope.

CONTEXT — CLIENT-CONFIRMED SEMANTICS:
- SEQUENTIAL stack: play the "How Many to Play" number of blocks, chosen weighted-random from the stack, one after another in random order.
- SIMULTANEOUS stack: play the "How Many to Play" number of blocks AT THE SAME TIME (layered). Block weights = likelihood each block is included.
- NEW FEATURE — "Always play base block" (simultaneous stacks only): when ON, the base block always plays and the remaining (playCount − 1) blocks are weighted-picked from the rest.
- Effective % display = INCLUSION probability: 3 equal blocks, play 1 → 33% each; play 2 → ~67% each; play 3 of 3 → 100% each. With base-block ON: base shows 100%; others reflect (playCount − 1) picks among the rest.
- isDone is cosmetic only, everywhere, forever.

STEP 1 — DIAGNOSE THE STACK REGRESSION (play count ignored).
Current bug: a stack set to "play 1" plays ALL blocks, both modes.
Run and paste:
grep -n "stackPlayCount\|playCount\|pickStack\|Sequential\|Simultaneous\|picked" Source/Audio/ArrangementResolver.cpp
Then paste the COMPLETE stack-resolution section of resolve() (from stack-group identification to entry creation).
Add logging:
DBG("STACK grp=" + juce::String(stackGroup) + " mode=" + juce::String(mode == StackPlayMode::Sequential ? "Seq" : "Sim") + " playCount=" + juce::String(playCount) + " groupSize=" + juce::String(stackBlocksInGroup.size()));
DBG("picked=" + juce::String(picked.size()));
Build, create a 3-block stack, play count 1, press Play, paste output. Identify which of these is true: (a) playCount resolves wrong (not one value from stackPlayCount.pick(rng)); (b) the pick returns all blocks; (c) entry creation iterates the full group instead of the picked subset. Fix the root cause. The invariant: playCount = stackPlayCount.pick(rng), clamped to [1, groupSize]; weighted sample WITHOUT replacement of exactly playCount blocks using each block's playChance as weight; entries created ONLY from the picked subset.

STEP 2 — VERIFY/RESTORE SIMULTANEOUS LAYERING.
Sequential: for each picked block (shuffled), entry.timelinePos = cursor; cursor += bodyLen.
Simultaneous: every picked entry gets the SAME timelinePos; cursor += longest body.
PlaybackEngine must mix ALL entries overlapping the buffer window — no break after the first match, no leftover isOverlay skips:
grep -n "break\|continue\|isOverlay" Source/Audio/PlaybackEngine.cpp
Paste and fix any offender. Then with a 3-block simultaneous stack, play 3: paste DBG showing three entries at identical timelinePos, and confirm by ear that they layer.

STEP 3 — IMPLEMENT "ALWAYS PLAY BASE BLOCK" (new, simultaneous only).
Model (Block.h): add bool alwaysPlayBase = false; This is a STACK-level setting like stackPlayCount — include it in propagateStackSettings so all blocks in a group share it.
Serialization.cpp: read/write "alwaysPlayBase" in both toJSON and fromJSON (default false for old projects).
Resolver: define the base block as the FIRST block of the stack group in project->blocks order. In the simultaneous path:
if (alwaysPlayBase && mode == Simultaneous) { picked.add(base); then weighted-sample (playCount − 1) from the remaining blocks; }
else normal weighted sample of playCount from all.
Clamp: if playCount <= 0 → 1; if alwaysPlayBase and playCount == 1 → only the base plays.
Inspector: in the stack settings section, add a "Always play base block" ToggleButton, visible only when mode == Simultaneous. Undoable (local pre-snapshot → mutate → propagate → recordMutation). Label which block is the base in the combined stack view (e.g. "(base)" suffix on its row).
BSF model.json: include alwaysPlayBase on the stack/block serialization so the mobile player can honor it.

STEP 4 — EFFECTIVE % = INCLUSION PROBABILITY (client-confirmed numbers).
The combined stack view shows, per block, the probability it is INCLUDED in a play. Compute by Monte Carlo (2000 trials) that calls THE SAME selection routine the resolver uses (factor the picker into a shared function used by both resolver and the display — this guarantees they can never diverge). Handle: playCount >= groupSize → all 100%; alwaysPlayBase ON → base 100%, others simulated with (playCount − 1) picks from the rest; all-zero weights → uniform fallback (and the picker itself must use uniform fallback, not getLast()). Recompute on slider drag END and when playCount or the base toggle changes.
Hand-verify and paste results: 3 equal / play 1 → 33±2% each; 3 equal / play 2 → 67±2%; 3 of 3 → 100%; base ON, play 2 of 3 equal → base 100%, others ~50%.

STEP 5 — REGRESSION GUARDS (keep permanently).
In the resolver after each stack: jassert(entriesAddedForThisStack <= playCount); with a DBG on violation.
Top of ArrangementResolver.cpp invariant comment block: isDone never referenced here; playCount honored; links bidirectional. Verify: grep -c "isDone" Source/Audio/*.cpp Source/Audio/*.h → all zeros (functional).

STEP 6 — TARGETED RE-TEST (paste results as PASS/FAIL):
1. Stack 3, SEQ, play 1 → exactly one plays (10 runs)
2. Stack 3, SEQ, play 2 → exactly two, back-to-back, random order
3. Stack 3, SIM, play 1 → exactly one
4. Stack 3, SIM, play 2 → exactly two, layered
5. Stack 3, SIM, play 3 → all three layered; next block waits for the longest
6. Base toggle ON, SIM, play 2 of 3 → base always + 1 other
7. Effective %: the four hand-verified numbers from Step 4
8. Weights 80/10/10, play 1 → the 80 block picked most (20 runs)
9. Adjacent regression check: links 1↔3 at 100% with block 2 between → 3,2,1 every time, nothing dropped
10. isDone: mark a stacked block Done → still plays
11. Save → reopen → stack mode, play count, base toggle, weights all restored; blocks visible immediately
12. Undo: base toggle, play count +/−, weight drags each undo cleanly

STEP 7 — CLEANUP. Remove all DBG lines added in this session EXCEPT the jassert guard and its violation DBG. Clean rebuild both configs:
cmake --build build --target BlockShuffler_Standalone --clean-first
Report: what the root cause in Step 1 was, every file touched, and the 12 test results.
