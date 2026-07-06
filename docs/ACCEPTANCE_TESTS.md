# BlockShuffler — ACCEPTANCE_TESTS.md (Client Measure of Success, rev 2026-07-06)

Derived 1:1 from the client's final feature list (2026-07-03) and clarifications. ALL tests must PASS before delivery. Run top to bottom. Any FAIL blocks delivery. After any code change, re-run the affected group AND groups 4–5 (highest regression risk).

## Group 1 — Blocks
- [ ] 1.1 "+" button adds a block with palette colour
- [ ] 1.2 Right-click → Delete removes a block; Delete key removes selected block (when no clip selected)
- [ ] 1.3 Double-click or right-click renames; name commits on Enter/focus loss
- [ ] 1.4 Right-click → Set Color changes the block colour
- [ ] 1.5 Drag a block to a gap → reorders fluidly, first attempt, every time
- [ ] 1.6 Drag a block onto a block → stacks, first attempt, every time

## Group 2 — Clips
- [ ] 2.1 Drag audio from Finder/Explorer onto waveform → clip added to selected block
- [ ] 2.2 Drag audio onto a block tile → clip added to THAT block
- [ ] 2.3 "+ Add Clip" browse button works
- [ ] 2.4 Right-click clip → rename, colour, remove all work
- [ ] 2.5 Per-clip weight slider: changing one clip never changes another clip's value
- [ ] 2.6 Weights drive selection: 80/15/5 over 30 plays roughly matches
- [ ] 2.7 Single clip at 0% weight → shows 100% effective → always plays
- [ ] 2.8 Start/end markers draggable; snap to grid; Shift bypasses snap
- [ ] 2.9 Drag an existing clip from one block into a later block → clip moves; block strip does NOT scroll back to the start

## Group 3 — Lead-ins and tails (clip alignment)
- [ ] 3.1 First clip with start marker moved forward → lead-in audio audible at song start (full volume)
- [ ] 3.2 Block N end marker aligns with Block N+1 start marker; bodies play back-to-back, zero gap, zero overlap
- [ ] 3.3 Tail of clip N crossfades under lead-in of clip N+1 (fade out / fade in)
- [ ] 3.4 Different tempos: lead-in/tail time-stretch to adjacent tempo by default; pitch preserved
- [ ] 3.5 Retain Lead-In Tempo → lead-in plays original speed; Retain Tail Tempo → tail original speed; both flags together → no stretching at the join

## Group 4 — Links (HIGH REGRESSION AREA)
- [ ] 4.1 Right-click → Link to → click target creates a link with an arc + readable label (block names + %)
- [ ] 4.2 Multiple link labels never overlap
- [ ] 4.3 Link at 100%: the two SPECIFIC linked blocks swap positions EVERY play — deterministic, same order every time
- [ ] 4.4 Blocks 1 & 3 linked at 100% with block 2 between: order is 3, 2, 1 every play; block 2 never moves; nothing is dropped
- [ ] 4.5 Link at 0%: never swaps. Link at 50%: swaps about half of 20 plays
- [ ] 4.6 Link to a block inside a stack swaps THAT block only (never a random stack member)
- [ ] 4.7 Remove Link works; undo restores it

## Group 5 — Stacks (HIGH REGRESSION AREA — client's #1 complaint)
- [ ] 5.1 Stack of 3, SEQUENTIAL, How Many to Play = 1 → EXACTLY ONE block plays (repeat 10×: always exactly one, weighted-random which)
- [ ] 5.2 Stack of 3, SEQUENTIAL, play 2 → exactly two play, one after another, random order
- [ ] 5.3 Stack of 3, SEQUENTIAL, play 3 → all three play back-to-back, random order, then the song continues
- [ ] 5.4 Stack of 3, SIMULTANEOUS, play 1 → exactly ONE block plays
- [ ] 5.5 Stack of 3, SIMULTANEOUS, play 2 → exactly TWO blocks play AT THE SAME TIME (layered)
- [ ] 5.6 Stack of 3, SIMULTANEOUS, play 3 → all three layered; next block starts after the longest finishes
- [ ] 5.7 Block weight sliders bias which blocks are included (80/10/10, play 1 → the 80 block is picked most)
- [ ] 5.8 "Always play base block" (SIMULTANEOUS): toggle ON, play 2 of 3 → base ALWAYS plays + 1 weighted pick from the others; toggle OFF → all three compete
- [ ] 5.9 Effective display (per client): 3 equal blocks, play 1 → each shows 33%; play 3 of 3 → each shows 100%; play 2 of 3 equal → each shows ~67%
- [ ] 5.10 With base-block ON, base shows 100% effective; others reflect (playCount−1) picks among the rest
- [ ] 5.11 Shift+drag moves a whole stack (all tiles move together); drag single block to gap unstacks it without the block disappearing; last remaining block auto-unstacks
- [ ] 5.12 Stack settings (mode, How Many to Play) are one global control per stack; stacked-block chance sliders appear once in the combined stack view with effective values

## Group 6 — Done flags (cosmetic only)
- [ ] 6.1 Mark clip as Done → visual change only → clip STILL plays and is STILL selectable by weight
- [ ] 6.2 Mark block as Done → visual change only → block STILL plays
- [ ] 6.3 "DONE" badge does not cover the block name
- [ ] 6.4 Export with everything marked Done → exports normally, no warning
- [ ] 6.5 grep isDone in Source/Audio → zero functional hits

## Group 7 — In-editor playback
- [ ] 7.1 Play applies all randomization (weights, links, stacks, chance)
- [ ] 7.2 Playing indicator (white border) tracks the current block on ANY block colour
- [ ] 7.3 Waveform follows the playing block; playhead line moves; time display updates
- [ ] 7.4 User selection/inspector unaffected by playback; stop reverts waveform to the selected block
- [ ] 7.5 Play Block / Play Clip (right-click) work EVERY time
- [ ] 7.6 Rapid play/stop 20× → no crash, no stuck audio
- [ ] 7.7 Play Clip on a clip with a lead-in still leads into the following block (lead-in not cut off)

## Group 8 — Song enders
- [ ] 8.1 Song-ender clip plays → arrangement truncates after it (tail included); nothing after
- [ ] 8.2 Works when the song-ender is inside a stack

## Group 9 — Tempo & grid
- [ ] 9.1 Clip tempo changes the GRID spacing only — audio speed unchanged
- [ ] 9.2 One click selects the tempo field; type; Enter commits
- [ ] 9.3 Block tempo sets all clips in the block; later per-clip override changes only that clip; new clips inherit block tempo
- [ ] 9.4 Project default tempo applies to new clips when no block tempo set

## Group 10 — Export (mobile/web-readable)
- [ ] 10.1 Export chooser offers WAV, FLAC, BSF
- [ ] 10.2 WAV/FLAC: correct pitch, speed, and crossfades — identical to in-editor playback (Audacity check)
- [ ] 10.3 BSF: valid ZIP with manifest.json + model.json + clips/*.flac; big ints as strings
- [ ] 10.4 Export with tempo-stretched joins matches playback
- [ ] 10.5 Source clips of any sample rate (44.1 kHz and 48 kHz) play and export at correct pitch — no semitone shift

## Group 11 — Sessions & undo
- [ ] 11.1 Save / Save As / Open work; window title shows project name; Cmd+S / Cmd+Shift+S
- [ ] 11.2 Reopen a saved session → ALL blocks visible immediately, all settings intact (full round-trip checklist)
- [ ] 11.3 Open via Finder/Explorer double-click behaves identically to the Open button
- [ ] 11.4 After load, Cmd+Z does nothing (history cleared)
- [ ] 11.5 Undo restores marker positions exactly, on grid; undo never desyncs unrelated state; redo never blanks the preview
- [ ] 11.6 Missing audio file on load → warning listing paths; session still opens

## Group 12 — Stability & parity
- [ ] 12.1 15 rapid undos, drag-heavy session, 50-block project → no crash
- [ ] 12.2 Windows build: same behavior as macOS for groups 1–11; logo and app icon appear (BinaryData-embedded)
- [ ] 12.3 Logo: background matches transport bar exactly, sized ~2/3 bar height, sits just left of Save As

## Group 13 — UI fidelity (client-reported visual defects)
- [ ] 13.1 Clip/block colour renders true — no global blue tint; each palette colour displays correctly ("yellow" is yellow)
- [ ] 13.2 Large stack of 9 blocks → all tiles visible and reachable, none cut off out of view
- [ ] 13.3 Long clip → grid lines do not obscure the waveform preview
- [ ] 13.4 Zoom maximum scales with clip length (short clips zoom further than long clips)

## Sign-off
All boxes checked → build both platforms → send to client with this checklist attached.
