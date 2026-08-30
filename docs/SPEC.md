# BlockShuffler — SPEC.md (LOCKED — client-approved 2026-07-03, rev 2026-07-06)

This is the definitive feature list provided by the client. It supersedes all previous specs, feature descriptions, and conversation history. Features not on this list are OUT OF SCOPE. Do not add, restore, or "improve" beyond it.

## 1. Blocks
- Add, remove, rename, colour, and drag to move blocks fluidly.
- Blocks are added with the "+" button at the end of the block strip.

## 2. Clips
- Add/remove clips to blocks; rename and colour them; set start/end triggers (markers).
- Clips are addable by dragging audio in from the file explorer AND via a browse button.
- Each clip has a randomization value (weight) that determines how likely it is to play when its block starts.
- On playback, each clip's end point aligns with the next block's start point, so clips can have lead-ins and endings (tails) that extend into adjacent blocks.
- An existing clip can be dragged from one block into another.

## 3. Links
- Blocks can be linked (via right click) to other blocks.
- Each link has a randomization value deciding how likely the two blocks are to switch places in the song.
- CLARIFIED SEMANTICS: a link connects two SPECIFIC blocks. A triggered swap exchanges exactly those two blocks' positions, bidirectionally. 100% = deterministic, same result every play. No other block is ever affected or dropped.

## 4. Stacks
- Blocks can stack. A stack is either SEQUENTIAL or SIMULTANEOUS.
- SEQUENTIAL: plays the "How Many to Play" number of blocks, chosen at random (weighted by each block's chance slider) from the stack, one after another in random order.
- SIMULTANEOUS: plays the "How Many to Play" number of blocks at the same time (layered). Block weights represent the likelihood each block is included.
- SIMULTANEOUS ONLY — "Always play base block" option: when enabled, the base block of the stack always plays, and the remaining (How Many to Play − 1) blocks are picked from the rest of the stack.
- Effective probability display (per client, 2026-07-03): the INCLUSION probability. Three stacked blocks, equal weights, play 1 → each shows 33%. Play 3 of a 3-block stack → each shows 100%.

## 5. Done flags
- Users can mark clips, blocks, and sections as done.
- Done is a visual/organizational flag ONLY. It never affects playback, selection, or export.

## 6. In-editor playback
- Users can play their song within the editor with all randomization values applied.
- Individual blocks and clips can be played on demand; a played clip still leads into the following block (lead-in/tail behaviour preserved).

## 7. Song enders
- Any clip can be set as a clip that ends the song. When it plays, the arrangement truncates after it (including its tail).

## 8. Tempo & grid
- Any clip can be given a tempo, which adjusts the GRID (not the clip's speed) so start/end marks land on time.

## 9. Lead-in / tail tempo behavior
- By default, lead-in and tail audio adjust (time-stretch) to the tempo of the adjacent clip.
- Users can retain original tempo for the lead-in, the tail, or both, per clip.

## 10. Export
- The song exports in a format readable by a mobile/web media player (the existing .bsf bundle: ZIP with manifest.json + model.json + FLAC clips; plus flat WAV/FLAC options).

## 11. UI fidelity & quality (client "professional look/feel", 2026-04-24)
These are quality requirements, not new features. They exist because the client reported each as a defect.
- Clip/block colour renders true to the selected colour — no global tint (the reported "everything tinted blue / yellow shows as green" bug must not occur).
- A large stack (up to at least 9 blocks) keeps every tile visible and reachable — none cut off out of view.
- On long clips, grid lines must not obscure the waveform preview.
- Zoom maximum scales with clip length (shorter clips zoom in further than longer clips).
- App icon and logo present on both platforms (use the logo image, not the wordmark).

## EXPLICITLY REMOVED (client, 2026-07-03)
- Overlapping blocks (the right-click "Set as Overlapping" concept) — removed entirely.
- Any feature not listed above that existed in earlier drafts.

## DEFERRED — post-delivery, OUT OF SCOPE for the 2026-07-17 deadline
- MIDI input / acting as a MIDI controller for an external program.
- Next-block preview and on-stage image/screen display.
