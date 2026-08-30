# BlockShuffler — CURRENT_STATE.md (as of 2026-07-06)

## What is built and believed working
- Core sequential playback: blocks play in position order, bodies back-to-back, lead-in/tail crossfades via shared EntryMixer.h (playback + export identical).
- Entry-0 lead-in plays at full gain from timeline 0.
- WSOLA tempo stretching with retain lead-in/tail flags.
- Block management: add/remove/rename/colour, drag reorder (ComponentDragger), drag-onto-block to stack, drag-to-gap to unstack, Shift+drag moves whole stack, rearrange within stack.
- Clip management: drag from Finder, drag onto block tiles, browse button, drag clips between blocks, per-clip weights (independent), song enders, per-clip play (right-click).
- Waveform: markers with grid snap, Shift bypass, adaptive grid density, proportional zoom with persistence, arrow-key nudge.
- Tempo: one-click select field, block tempo, per-clip override, project default tempo.
- Links: create/remove via right click, name labels (no UUIDs), collision-avoided arc labels.
- Play tracker: white playing indicator, waveform follows playing block, playhead line, time display; selection independent of playback.
- Save/Save As/Open with window title; single canonical synchronous loadProject(); relative portable audio paths (forward slashes); missing-file warning; undo history cleared on load.
- Undo: snapshot-based, per-action local pre-snapshots, one entry per slider drag, stack-count buttons undoable.
- Export: WAV/FLAC/BSF chooser; BSF = manifest.json + model.json + FLAC clips; int64s as strings; project-rate writers.
- App icon wired (icns/ico); logo in transport bar (needs polish — see open items).
- Overlapping blocks REMOVED (client, 2026-07-03) — the right-click "Set as Overlapping" concept and its slider are gone; simultaneous stacks cover that use case. Any remaining overlapping-block code is dead and must stay removed.
- Audits passed: last full audit had 0 critical / 0 high / 0 medium after fixes.

## BROKEN RIGHT NOW (must fix — see MASTER_PROMPT)
1. Stack play count regressed: a stack set to "play 1" plays through ALL blocks. Both modes affected. (Tests 5.1, 5.4)
2. Simultaneous mode broken after overlapping-block removal (entries may not layer or count is ignored). (Tests 5.5, 5.6)
3. NEW required feature not yet built: "Always play base block" toggle for simultaneous stacks. (Tests 5.8, 5.10)
4. Effective probability must be verified against client semantics (inclusion probability: 1-of-3 equal → 33% each; 3-of-3 → 100%). Monte Carlo approach was implemented then possibly regressed alongside stack changes. (Test 5.9)
5. Logo polish: background colour must match transport bar exactly, larger, positioned next to Save As. Verify logo + icon load via BinaryData (cross-platform), not runtime file paths. (Test 12.3)

## UNVERIFIED — client-reported, needs a pass before sign-off
- Clip/block colour accuracy: client reported a global blue tint ("yellow" rendered green). Confirm the colour picker renders true palette colours. (Test 13.1) — NOT confirmed in code review yet.
- Large-stack visibility: client reported tiles cut off out of view at ~7–9 blocks in a stack. Confirm all tiles remain visible/reachable at 9. (Test 13.2) — NOT confirmed yet.
- Clip drag between blocks: feature works, but confirm the block strip does NOT scroll back to the start after the drop. (Test 2.9)
- Per-clip play preserves lead-in into the following block. (Test 7.7)
- Sample-rate/pitch: believed working, but keep as a standing test — 44.1 & 48 kHz sources must play/export with no semitone shift. (Test 10.5)
- Grid-lines-over-waveform (long clips) and zoom-scales-with-length are believed handled by adaptive grid + proportional zoom; verify against tests 13.3, 13.4.

## Regression history (areas requiring proof-of-fix in every prompt)
| Area | Times regressed | Guard |
|---|---|---|
| isDone affecting playback | 4 | grep Source/Audio for isDone must be 0 functional hits; invariant comment atop ArrangementResolver.cpp |
| Stack play count ignored | 3 | jassert(entriesAdded <= playCount) in resolver — must remain |
| Link swap one-directional / drops blocks | 3 | swap via std::swap on local copies; verify 3-block A↔C test |
| Lead-in cut off | 3 | entry 0: timelinePos = startMark, full-gain lead-in |
| Blocks invisible on session load | 3 | synchronous loadProject(), no sendChangeMessage reliance |
| Export pitch/speed wrong | 2 | single project->sampleRate through the whole pipeline |

## Environment
- macOS build primary (arm64). Windows build exists; cross-platform path handling fixed (relative, forward slashes). Platform parity to be re-verified at delivery.
- Build: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target BlockShuffler_Standalone (use --clean-first after resolver changes).

## Deadline
- Client deadline 2026-07-17 (agreed with client 2026-07-04). Feature freeze: only SPEC.md items.
