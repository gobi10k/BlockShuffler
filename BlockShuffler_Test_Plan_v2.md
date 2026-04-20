# BlockShuffler — Acceptance Test Plan v2

**Based on:** BlockShuffler_Feature_Description.md (2026-03-22)
**Tester setup:** macOS, 3+ audio files of varying format/length/tempo, external player (Audacity + VLC) for export verification

---

## How to Use This Document

Each test has a unique ID (e.g. T1.1). Run them in order within each group.
Mark each: **PASS**, **FAIL** (describe what happened), or **SKIP** (with reason).

For probability tests, run **20+ iterations** and tally results.
For export tests, verify output in an **external application**, not just BlockShuffler.

---

## TEST GROUP 5: Lead-In / Tail Handling & Tempo Stretching

### T5.1 — Lead-in/tail crossfade (same tempo)
1. Create blocks A and B, each with one clip at 120 BPM
2. Set clip A's end marker ~2 beats before audio end (creating tail)
3. Set clip B's start marker ~2 beats after audio start (creating lead-in)
4. Play
5. **Expected:** Smooth crossfade at the join. Tail fades out, lead-in fades in. No gap, no pop.
- PASS

### T5.2 — End mark aligns with next start mark
1. Same setup as T5.1
2. Listen carefully to the transition
3. **Expected:** Bodies play back-to-back. The overlap occurs only in the tail/lead-in region.
- PASS

### T5.3 — Tempo stretching (different tempos)
1. Set clip A to 120 BPM, clip B to 90 BPM
2. Set tails and lead-ins as in T5.1
3. Play
4. **Expected:** The tail of clip A slows down toward 90 BPM. The lead-in of clip B speeds up toward 120 BPM. No pitch change — WSOLA preserves pitch.
- PASS

### T5.4 — Tempo stretch audible verification
1. Use two clips with very different tempos (e.g. 80 vs 160 BPM)
2. Listen to the crossfade region
3. **Expected:** Rhythmic content transitions smoothly. No chipmunk/slowed-down pitch artifacts. Slight artifacts at extreme ratios are acceptable but it should not sound like simple resampling.
- PASS

### T5.5 — Retain Lead-In Tempo
1. Check "Retain Lead-In Tempo" on clip B
2. Play (clip A at 120 BPM, clip B at 90 BPM)
3. **Expected:** Clip B's lead-in plays at its original 90 BPM speed (not stretched to 120). Clip A's tail is still stretched.
4. Cmd+Z → flag unchecks, stretching resumes
- PASS

### T5.6 — Retain Tail Tempo
1. Check "Retain Tail Tempo" on clip A
2. Play
3. **Expected:** Clip A's tail plays at original 120 BPM speed (not stretched to 90).
4. Cmd+Z → reverts
- PASS but deselects the clip. 

### T5.7 — Both retain flags checked
1. Check retain lead-in on clip B AND retain tail on clip A
2. Play
3. **Expected:** No stretching occurs in the crossfade region at all. Both play at original speed.
- PASS

### T5.8 — Overlapping blocks are NOT tempo-stretched
1. Create an overlapping block with a different tempo clip
2. Play
3. **Expected:** Overlapping clip plays at its original tempo, no stretch applied
- FAIL no playback

### T5.9 — Tempo stretching in export
1. Set up a two-block arrangement with different tempos and lead-in/tail overlap
2. Export as WAV
3. Open in Audacity
4. **Expected:** The crossfade region shows the stretched audio — matches what was heard in-editor
- FAIL the exported audio is pitched up.

---

## TEST GROUP 6: Block Stacking

### T6.1 — Create a stack
1. Right-click block A → Stack with → click block B
2. **Expected:** Both blocks appear vertically stacked in one horizontal slot. Badge shows stack group number.
-PASS

### T6.2 — Stack play count (single value)
1. Stack 4 blocks, set play count = 2
2. Play 10+ times
3. **Expected:** Exactly 2 blocks from the stack play each time
-PASS

### T6.3 — Stack play count (weighted values)
1. Add entries: count=1 weight=0.7, count=3 weight=0.3
2. Play 30+ times, tally how many blocks play
3. **Expected:** ~70% of the time 1 block plays, ~30% of the time 3 blocks play
- PASS

### T6.4 — Stack count +/- buttons
1. Click + on a count entry
2. **Expected:** Count increments
3. Click -
4. **Expected:** Count decrements (minimum 1)
5. Cmd+Z
6. **Expected:** Reverts
- PASS

### T6.5 — Stack count weight slider
1. Drag weight slider for a count entry
2. Release
3. Cmd+Z
4. **Expected:** Weight reverts
- PASS

### T6.6 — Add/remove stack count entries
1. Click +Entry → new count option appears
2. Click × on an entry → it's removed
3. Cmd+Z → entry reappears
4. Cmd+Z again → added entry disappears
- PASS

### T6.7 — Sequential mode
1. Set stack play mode = Sequential, play count = 2
2. Play
3. **Expected:** 2 chosen blocks play one after another (not layered)
- PASS

### T6.8 — Simultaneous mode
1. Set stack play mode = Simultaneous
2. Play
3. **Expected:** Chosen blocks play layered at the same timeline position. Audio is mixed.
- PASS

### T6.9 — Simultaneous mode toggle undo
1. Toggle to Simultaneous
2. Cmd+Z
3. **Expected:** Reverts to Sequential
- PASS

---

## TEST GROUP 7: Overlapping Blocks

### T7.1 — Set block as overlapping
1. Stack blocks A and B
2. Right-click B → Set as Overlapping
3. **Expected:** Block B gets a dashed border. It's excluded from normal stack scheduling.
- PASS

### T7.2 — Overlapping toggle undo
1. Cmd+Z
2. **Expected:** Block B returns to normal (solid border, included in scheduling)
- PASS

### T7.3 — Overlap probability at 100%
1. Set overlap probability to 100%
2. Play 10 times
3. **Expected:** Overlapping block's clip ALWAYS plays on top of the normal block
- PASS

### T7.4 — Overlap probability at 0%
1. Set to 0%
2. Play 10 times
3. **Expected:** Overlapping block NEVER plays
- PASS

### T7.5 — Overlap probability at 50%
1. Set to 50%
2. Play 20+ times
3. **Expected:** Plays roughly half the time
- PASS 

### T7.6 — Overlap probability undo
1. Drag slider from 50% to 80%
2. Cmd+Z
3. **Expected:** Reverts to 50%
- PASS but sometimes the sample disappears

### T7.7 — Overlapping block timing
1. Play
2. **Expected:** Overlapping clip starts at same time as the normal block's clip, not sequentially
- PASS

---

## TEST GROUP 8: Block Linking (Swap)

### T8.1 — Create a link
1. Right-click block A → Link to → click block B
2. **Expected:** Curved arc drawn between A and B in the block strip
- PASS

### T8.2 — Link swap at 100%
1. Set swap probability to 100%
2. Play 5 times
3. **Expected:** A and B always swap positions
- PASS

### T8.3 — Link swap at 0%
1. Set to 0%
2. Play 5 times
3. **Expected:** A and B never swap
- PASS

### T8.4 — Link swap at 50%
1. Set to 50%
2. Play 20+ times
3. **Expected:** Swap roughly half the time
- PASS

### T8.5 — Link probability undo
1. Drag swap probability slider
2. Cmd+Z
3. **Expected:** Reverts to previous value
- PASS

### T8.6 — Remove link (context menu)
1. Right-click linked block → Remove Link
2. **Expected:** Arc disappears. No more swapping.
- PASS

### T8.7 — Link overlay scrolling
1. Create many blocks, create a link between distant blocks
2. Scroll the block strip
3. **Expected:** Arc follows the blocks correctly. No visual glitches.
- PASS

---

## TEST GROUP 9: Song-Ending Clips

### T9.1 — Set clip as song ender
1. Right-click clip → Song Ender (check)
2. **Expected:** Visual indicator on clip row
- PASS

### T9.2 — Song ender truncates arrangement
1. Blocks: A, B(song-ender clip), C, D
2. Play
3. **Expected:** Playback stops after B's clip finishes (including tail). C and D don't play.
- PASS

### T9.3 — Song ender in stacked block
1. Put song-ender clip in a stacked block, give it 100% probability
2. Play
3. **Expected:** Arrangement truncates after that clip
- PASS

### T9.4 — Song ender undo
1. Toggle Song Ender off
2. Cmd+Z → toggles back on
3. Play → still truncates
- PASS works but deselects the clip. 
---

## TEST GROUP 10: In-Editor Playback

### T10.1 — Play button / Space bar
1. Press Play or Space
2. **Expected:** Arrangement resolves and audio plays
- PASS 

### T10.2 — Stop
1. Press Stop
2. **Expected:** Audio stops. Playhead rewinds to 0.
-PASS

### T10.3 — Rewind
1. Press Rewind while stopped
2. **Expected:** Playhead at 0
3. Press Play
4. **Expected:** New randomisation plays from start
- PASS

### T10.4 — Playhead moves
1. During playback, observe waveform view
2. **Expected:** Red vertical playhead line moves across waveform
- PASS

### T10.5 — Currently-playing block highlight
1. During playback, observe block strip
2. **Expected:** The playing block shows a thicker green top bar
- PASS

### T10.6 — Time display
1. During playback, check transport bar
2. **Expected:** MM:SS elapsed / MM:SS total, updating at ~30 fps
- PASS

### T10.7 — Unique arrangement each play
1. Play 5 times with multiple clips and weighted probabilities
2. **Expected:** Results vary between plays
- PASS

---

## TEST GROUP 11: Export

### T11.1 — Export WAV
1. Click Export → WAV → save
2. Open in Audacity
3. **Expected:** Valid WAV, audio matches in-editor playback including crossfades and tempo stretching
- PASS

### T11.2 — Export FLAC
1. Click Export → FLAC → save
2. Open in VLC
3. **Expected:** Valid FLAC, same audio content as WAV export, lossless
-PASS

### T11.3 — Export BSF Bundle
1. Click Export → BSF Bundle → save
2. Rename .bsf to .zip, extract
3. **Expected:** Contains manifest.json + clips/ folder with .flac files
-  PASS

### T11.4 — BSF manifest validation
1. Open manifest.json
2. Verify:
   - [ ] `version` is 1
   - [ ] `sampleRate` is correct
   - [ ] `bitDepth` is present
   - [ ] `totalDurationSamples` is a **string** (not number)
   - [ ] Each clip has `id`, `file`, `startSample`, `durationSamples` (string)
   - [ ] `arrangement` array lists clips with `clipId` and `startTime` (string)
   - PASS

### T11.5 — BSF clips are valid FLAC
1. Open individual clip_001.flac in Audacity
2. **Expected:** Valid FLAC, contains the rendered body audio of that clip
- PASS

### T11.6 — Export progress dialog
1. Export a project with many clips
2. **Expected:** Progress bar visible. UI remains interactive (can scroll, etc.)
- PASS

### T11.7 — Export includes tempo stretching
1. Set up blocks with different tempos and lead-in/tail overlap
2. Export WAV
3. Compare with in-editor playback
4. **Expected:** Stretched crossfades present in export file
- PASS

---

## TEST GROUP 12: Project Save / Load

### T12.1 — Save project (Cmd+S)
1. Create a complex project (blocks, clips, links, stacks, overlaps, various flags)
2. Cmd+S → save as test.bsp
3. **Expected:** File created, is valid JSON
- PASS

### T12.2 — Load project (Cmd+O)
1. Cmd+O → open test.bsp
2. **Expected:** All state restored
- PASS

### T12.3 — Full round-trip checklist
Verify each property survives save/load:
- [ ] Block names
- [ ] Block colours
- [ ] Block positions/order
- [ ] Block isDone flags
- [ ] Stack groups and stack play counts (all weighted entries)
- [ ] Stack play mode (Sequential/Simultaneous)
- [ ] Overlapping flag and overlap probability
- [ ] Links and swap probabilities
- [ ] Clip names and colours
- [ ] Clip probabilities
- [ ] Clip start/end marker positions
- [ ] Clip tempo values
- [ ] Clip retainLeadInTempo / retainTailTempo flags
- [ ] Clip isSongEnder flags
- [ ] Clip isDone flags
- [ ] Clip gridOffsetSamples
- test later

### T12.4 — Missing file warning
1. Save project referencing audio file X
2. Move/delete file X from disk
3. Cmd+O → open the project
4. **Expected:** Warning dialog appears listing the missing file path
5. Project opens anyway — missing clip shows "No audio loaded" in waveform
-PASS

### T12.5 — New project (Cmd+N)
1. Cmd+N
2. **Expected:** Fresh project with a single empty block. Previous project cleared.
- PASS

---

## TEST GROUP 13: Undo / Redo (Comprehensive)

Test every action from the feature description's undo table:

| Test | Action | Undo (Cmd+Z) restores? | Redo works? |
|------|--------|------------------------|-------------|
| T13.1 | Add block | PASS | PASS |
| T13.2 | Remove block (check clips restored) | PASS | PASS |
| T13.3 | Move/reorder block | PASS | PASS |
| T13.4 | Stack blocks | PASS | PASS |
| T13.5 | Add link | PASS | PASS |
| T13.6 | Remove link | PASS | PASS |
| T13.7 | Set link swap probability (slider) | PASS | PASS |
| T13.8 | Set block colour | PASS | PASS |
| T13.9 | Block rename | PASS | PASS |
| T13.10 | Toggle block "Mark as Done" | PASS | PASS |
| T13.11 | Toggle block "Set as Overlapping" | PASS | PASS |
| T13.12 | Clip probability slider drag | PASS | PASS |
| T13.13 | Block overlap probability slider drag | PASS | PASS |
| T13.14 | Toggle clip "Song Ender" | PASS | PASS |
| T13.15 | Toggle clip "Mark as Done" | PASS | PASS |
| T13.16 | Toggle "Retain Lead-In Tempo" | FAIL | FAIL |
| T13.17 | Toggle "Retain Tail Tempo" | PASS | PASS |
| T13.18 | Toggle "Simultaneous" stack mode | PASS | PASS |
| T13.19 | Clip tempo field (Enter key) | PASS | PASSS |
| T13.20 | Stack count +/- buttons | PASS | PASS |
| T13.21 | Stack count weight slider drag | PASS | PASS |
| T13.22 | Add stack count entry | PASS | PASS |
| T13.23 | Remove stack count entry | PASS | PASS |
ESC to cancel doesn't work!
---

## TEST GROUP 14: Sample Rate Conversion

### T14.1 — Import clip at different sample rate
1. Load a 96kHz WAV into a 48kHz project
2. **Expected:** Clip loads, waveform displays. Audio plays at correct pitch and speed (not double speed).
- PASS

### T14.2 — Marker positions after resample
1. Save the project with the resampled clip and custom start/end marker positions
2. Reload
3. **Expected:** Markers are in the same audible positions (scaled by projectSR/nativeSR)
- PASS

### T14.3 — Mixed sample rates in one project
1. Add clips: one at 44.1kHz, one at 48kHz, one at 96kHz
2. Play through all blocks
3. **Expected:** All clips play at correct pitch. No speed artifacts.
- PASS
---

## TEST GROUP 15: Keyboard Shortcuts

| Test | Shortcut | Expected |
|------|----------|----------|
| T15.1 | Space | Toggles Play/Stop | PASS
| T15.2 | Cmd+S | Saves (or prompts for location) | PASS
| T15.3 | Cmd+O | Opens file browser for .bsp | PASS
| T15.4 | Cmd+N | New project | PASS
| T15.5 | Cmd+Z | Undo | PASS
| T15.6 | Cmd+Shift+Z | Redo | PASS
| T15.7 | Cmd+Y | Redo (alternative) PASS
| T15.8 | ←/→ (clip selected) | Nudge gridOffsetSamples | PASS
| T15.9 | Delete/Backspace | Remove selected clip | PASS
| T15.10 | Esc | Cancel pending link/stack mode | PASS 
| T15.11 | Cmd+Scroll | Zoom waveform | PASS

---

## TEST GROUP 16: Edge Cases & Robustness

### T16.1 — Empty project playback
1. Fresh project, no clips, press Play
2. **Expected:** No crash. Nothing plays.
- PASS

### T16.2 — Block with no clips
1. Add an empty block between blocks with clips
2. Play
3. **Expected:** Empty block is skipped. Adjacent blocks' crossfade handles correctly.
- PASS

### T16.3 — All clips at 0% probability
1. Set all clips in a block to 0%
2. Play
3. **Expected:** Graceful handling. No crash. Block either skipped or one clip chosen as fallback.
- PASS

### T16.4 — Single block, single clip (simplest case)
1. One block, one clip
2. Play and export
3. **Expected:** Works correctly.
- PASS 

### T16.5 — Very long audio file (30+ min)
1. Load a long file, set markers, play and export
2. **Expected:** No crash, no truncation, no memory issues
- PASS

### T16.6 — Many blocks stress test (50+)
1. Create 50 blocks with clips
2. Play
3. **Expected:** Resolves without significant delay
- PASS

### T16.7 — Rapid Play/Stop cycling
1. Press Play/Stop rapidly 20 times
2. **Expected:** No crash, no audio glitch, no deadlock
- PASS

### T16.8 — Export empty arrangement
1. Mark all blocks as Done, try to export
2. **Expected:** Graceful handling — either warning or empty file, no crash
- PASS

### T16.9 — Multiple links forming a chain
1. Link A↔B, B↔C, C↔D
2. Set all swap probabilities to 100%
3. Play multiple times
4. **Expected:** Blocks swap. Order varies. No crash from circular or chain swaps.
- PASS

### T16.10 — Stack with only overlapping blocks (no normal blocks)
1. Stack 3 blocks, set ALL as overlapping
2. Play
3. **Expected:** Graceful handling — nothing to anchor to, so either silence or error, no crash
- PASS

---

## RESULTS SUMMARY

| Group | Total Tests | Pass | Fail | Skip |
|-------|------------|------|------|------|
| 1. Layout | 3 | | | |
| 2. Block Management | 10 | | | |
| 3. Clip Management | 14 | | | |
| 4. Waveform & Grid | 10 | | | |
| 5. Lead-In/Tail/Stretch | 9 | | | |
| 6. Stacking | 9 | | | |
| 7. Overlapping | 7 | | | |ßß
| 8. Linking | 7 | | | |
| 9. Song Ender | 4 | | | |
| 10. Playback | 7 | | | |
| 11. Export | 7 | | | |
| 12. Save/Load | 5 | | | |
| 13. Undo/Redo | 23 | | | |
| 14. Sample Rate | 3 | | | |
| 15. Keyboard | 11 | | | |
| 16. Edge Cases | 10 | | | |
| **TOTAL** | **139** | | | |
