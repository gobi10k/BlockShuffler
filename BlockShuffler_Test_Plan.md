# BlockShuffler — Acceptance Test Plan

**Based on:** Original feature spec (user requirements)
**Cross-referenced with:** HANDOVER.md (2026-03-19 build)
**Status key:** 🟢 Claimed implemented | 🟡 Partially implemented | 🔴 Not implemented per handover

---

## TEST GROUP 1: Block Management

### T1.1 — Add block via "+" button 🟢
1. Launch BlockShuffler
    - this works 
2. Look for a "+" button at the end of the block strip
3. Click it
4. **Expected:** A new block appears with a default name ("Block N") and a visible colour
        - this works/ no visible colour
5. Click "+" three more times
6. **Expected:** Four blocks total, each with a unique default name
    - four blocks are created, no unique names

### T1.2 — Remove block 🟢
1. Right-click any block
    - right click only works on specific area of the block and not the whole block. 
2. Look for "Delete Block" or equivalent in context menu
3. Click it
4. **Expected:** Block disappears. Remaining blocks reorder to fill the gap

### T1.3 — Rename block 🟢
1. Double-click a block's name label (or use right-click → Rename)
2. Type a new name (e.g. "Intro")
3. Press Enter or click away
4. **Expected:** Block now shows the new name
    - this works

### T1.4 — Color block 🟢
1. Right-click a block → Set Color (or equivalent)
2. Choose a different colour
3. **Expected:** Block border/tile updates to the new colour immediately
    - only the border changes colour

### T1.5 — Drag to reorder blocks 🟢
1. Create 4 blocks: A, B, C, D
2. Click and drag block C to the left of block A
3. Release
4. **Expected:** Order is now C, A, B, D. Positions update accordingly
5. Drag C back between B and D
6. **Expected:** Order is A, B, C, D again
    - this works but could be more accurate. 

### T1.6 — Mark block as done 🟢
1. Right-click a block → Mark as Done (or checkbox in inspector)
2. **Expected:** Block visually changes (dimmed, checkmark, or similar indicator)
    - this works
3. Toggle it off
4. **Expected:** Block returns to normal appearance
    - this works


---

## TEST GROUP 2: Clip Management

### T2.1 — Add clip by dragging audio from Finder 🟢
1. Open Finder alongside BlockShuffler
2. Select a .wav file
3. Drag it onto a block (or onto the waveform area with a block selected)
4. **Expected:** Clip appears in the selected block's clip list. Waveform displays.
    - this works

### T2.2 — Add clip by browse button 🟢
1. Select a block
2. Look for an "Add Clip" / browse button
3. Click it — file chooser should open
4. Select a .flac file
5. **Expected:** Clip loads. Waveform renders. Name defaults to filename.
    - this works

### T2.3 — Multiple audio formats 🟢
Repeat T2.1 or T2.2 with each format:
- [ ] .wav
- [ ] .aiff
- [ ] .flac
- [ ] .ogg
- [ ] .mp3
**Expected:** All load successfully and display waveforms
    - all work 

### T2.4 — Remove clip 🟢
1. Right-click a clip → Remove Clip (or select + Delete key)
2. **Expected:** Clip removed from block. Waveform view updates.
    - this works 
    
### T2.5 — Rename clip 🟢
1. Right-click clip → Rename (or double-click name)
2. Enter new name
3. **Expected:** Name updates in clip list and waveform header
    - right clicking on clips crashes the software

### T2.6 — Color clip 🟢
1. Right-click clip → Set Color
2. **Expected:** Clip's visual indicator updates to new colour
    - right clicking on clips crashes the software

### T2.7 — Mark clip as done 🟢
1. Right-click clip → Mark as Done
2. **Expected:** Visual change on clip (dim/checkmark)
    - right clicking on clips crashes the software

### T2.8 — Clip probability slider 🟢
1. Select a block with 2+ clips
2. In the inspector, find the probability slider for each clip
3. Set Clip A to 90%, Clip B to 10%
4. Play the song 10+ times
5. **Expected:** Clip A plays far more often than Clip B
    - this works

---

## TEST GROUP 3: Waveform Editor & Grid

### T3.1 — Waveform display 🟢
1. Load a clip into a block
2. **Expected:** Waveform renders against dark background. Clip name and tempo visible.
    - this works

### T3.2 — Start/end markers visible and draggable 🟢
1. Look for green (start) and red (end) vertical markers on waveform
2. Drag the start marker to the right
3. **Expected:** Marker moves. Region before start marker dims (lead-in visualisation).
4. Drag end marker to the left
5. **Expected:** Region after end marker dims (tail visualisation).
    - this works but could be improved 

### T3.3 — Grid snap on markers 🟢
1. Set clip tempo to 120 BPM
2. Drag start marker slowly
3. **Expected:** Marker snaps to beat grid positions (visible grid lines should align)
    - this works

### T3.4 — Shift+drag bypasses snap 🟢
1. Hold Shift and drag a marker
2. **Expected:** Marker moves freely without snapping
- this works

### T3.5 — Arrow key nudge 🟢
1. Select a clip
2. Press left/right arrow keys
3. **Expected:** Clip's grid offset nudges by 1 grid unit per press
    - this does nothing 

### T3.6 — Tempo field per clip 🟢
1. In inspector, find the tempo field for a clip
2. Change from 120 to 90 BPM
3. **Expected:** Grid lines on waveform recalculate. Grid spacing changes visibly. Audio does NOT change speed.
-  this does nothing 

### T3.7 — Zoom waveform 🟢
1. Cmd+scroll (or equivalent) on waveform
2. **Expected:** Waveform zooms in/out. Grid lines scale accordingly.
- this crashes the software

---

## TEST GROUP 4: Lead-In / Tail-End Handling

### T4.1 — Lead-in and tail crossfade 🟢
1. Create two blocks (A, B), each with one clip
2. Set clip A's end marker before the audio's actual end (creating a tail)
3. Set clip B's start marker after the audio's actual start (creating a lead-in)
4. Play the arrangement
5. **Expected:** The tail of clip A overlaps with the lead-in of clip B. Audio crossfades smoothly — no gap, no pop.
    - this doesn't work'

### T4.2 — End mark aligns with next start mark 🟢
1. Same setup as T4.1
2. **Expected:** Clip A's endMark sample position aligns in time with Clip B's startMark position. The "body" regions play back-to-back.
    - this doesn't work'

### T4.3 — Retain lead-in tempo checkbox 🔴 (stored but no effect)
1. In inspector, check "Retain Lead-In Tempo" for a clip
2. Set its tempo to something different from the previous clip
3. Play
4. **Expected per spec:** Lead-in plays at original tempo regardless of adjacent clip tempo
5. **Actual (per handover):** Flag is stored and displayed but TempoStretcher is a stub. This will NOT work yet.

### T4.4 — Retain tail tempo checkbox 🔴 (stored but no effect)
Same as T4.3 but for tail end.
**Actual (per handover):** Same — stub only. No effect.

---

## TEST GROUP 5: Block Stacking

### T5.1 — Stack two blocks 🟢
1. Right-click block A → Stack with → click block B (or drag B onto A)
2. **Expected:** Blocks A and B now appear vertically stacked in the same horizontal slot
    - this works

### T5.2 — Stack play count 🟢
1. Stack 4 blocks together
2. Set "Number of blocks to play" to 2
3. Play the song multiple times
4. **Expected:** Only 2 of the 4 blocks play each time the stack position is reached
    - this works 
    
### T5.3 — Weighted stack play count 🟡 (stored, needs UI verification)
1. In inspector, find the stack play count section
2. Add multiple values (e.g., play count = 2 at weight 0.7, play count = 3 at weight 0.3)
3. Play 20+ times
4. **Expected:** Count of 2 occurs ~70% of the time, count of 3 ~30%
5. **Verify:** The handover mentions undo isn't wired for stack count changes — check if the values actually persist and affect playback

### T5.4 — Sequential stack play mode 🟢
1. Stack 3 blocks, set play count = 2, mode = Sequential
2. Play multiple times
3. **Expected:** 2 randomly chosen blocks play one after another (not simultaneously)

### T5.5 — Simultaneous stack play mode 🟢
1. Same stack, change mode to Simultaneous
2. Play
3. **Expected:** 2 blocks' clips play layered on top of each other at the same time position

---

## TEST GROUP 6: Block Linking (Swap)

### T6.1 — Create a link 🟢
1. Right-click block A → "Link to..." → click block B
2. **Expected:** Visual arc appears between blocks A and B

### T6.2 — Link swap probability 🟢
1. Select a link (click the arc or select from inspector)
2. Set swap probability to 100%
3. Play multiple times
4. **Expected:** Blocks A and B always swap positions in the arrangement
5. Set to 0%
6. **Expected:** Blocks never swap

### T6.3 — Link visual overlay 🟢
1. Create 3 links between different blocks
2. **Expected:** Curved arcs visible between each pair. No overlapping/garbled rendering.

### T6.4 — Remove a link 🟢
1. Right-click a link or block → Remove Link
2. **Expected:** Arc disappears. Blocks no longer swap.

---

## TEST GROUP 7: Overlapping Blocks

### T7.1 — Set block as overlapping 🟢
1. Stack blocks A (normal) and B
2. Right-click block B → Set as Overlapping (or toggle in inspector)
3. **Expected:** Block B is visually marked differently (dashed border, overlay icon, etc.)

### T7.2 — Overlap probability 🟢
1. Set overlap probability for block B to 50%
2. Play 20+ times
3. **Expected:** Block B's clip plays on top of block A's clip roughly half the time
4. Set to 0% → never plays. Set to 100% → always plays.

### T7.3 — Overlapping block doesn't take its own time slot 🟢
1. Stack normal block A with overlapping block B
2. Play
3. **Expected:** Block B's audio starts at the same time as block A. It does NOT create an additional sequential segment.

---

## TEST GROUP 8: Song-Ending Clips

### T8.1 — Set clip as song ender 🟢
1. In inspector, check "Song Ender" for a clip in block C
2. Create blocks A, B, C, D, E
3. Play the arrangement
4. **Expected:** If block C's song-ender clip is chosen, playback stops after that clip's end mark (including tail). Blocks D and E do NOT play.

### T8.2 — Song ender with stacked blocks 🟢
1. Put the song-ender clip in a stacked block
2. Ensure it gets selected by giving it 100% probability
3. Play
4. **Expected:** Arrangement truncates after the song-ender clip regardless of remaining blocks

---

## TEST GROUP 9: In-Editor Playback

### T9.1 — Play button 🟢
1. Load clips into several blocks
2. Press Play (or Space)
3. **Expected:** Audio plays through speakers. Arrangement is randomised.

### T9.2 — Stop button 🟢
1. While playing, press Stop
2. **Expected:** Audio stops immediately

### T9.3 — Rewind 🟢
1. After stopping, press Rewind
2. Press Play
3. **Expected:** Playback starts from the beginning with a NEW randomisation

### T9.4 — Playhead indicator 🟢
1. During playback, observe the waveform view and block strip
2. **Expected:** A red vertical playhead moves across the waveform. The currently-playing block is highlighted (green bar per handover).

### T9.5 — Each play produces different arrangement 🟢
1. Play the song 5 times with multiple clips per block, some links, and a stack
2. Note which clips play each time
3. **Expected:** Results vary according to set probabilities. Not identical every time (unless all probabilities are 100%).

---

## TEST GROUP 10: Export

### T10.1 — Export as single FLAC file 🟢
1. Set up an arrangement with several blocks
2. Click Export → choose "FLAC" (or flat file option)
3. Save the file
4. **Expected:** Produces a valid .flac file. Open in VLC/Audacity — audio matches what was heard during in-editor playback. Lossless quality.

### T10.2 — Export as single WAV file 🟢
1. Same as above, choose WAV
2. **Expected:** Valid .wav file, correct audio content

### T10.3 — Export as .bsf bundle 🟢
1. Click Export → choose .bsf format
2. Save the file
3. Rename .bsf to .zip and extract
4. **Expected:** Contains `manifest.json` and `clips/` folder with numbered .flac files
5. Open `manifest.json`
6. **Expected:** Valid JSON matching the schema in HANDOVER.md §6. `arrangement` array lists clips in correct order. `totalDurationSamples` is a string (to avoid precision loss).

### T10.4 — Export progress dialog 🟢
1. Export a project with many/long clips
2. **Expected:** A progress bar dialog appears during export. App remains responsive.

---

## TEST GROUP 11: Project Save/Load

### T11.1 — Save project (.bsp) 🟢
1. Create a project with blocks, clips, links, stacks, and overlapping blocks
2. Cmd+S → choose save location
3. **Expected:** .bsp file created. It's valid JSON.

### T11.2 — Load project (.bsp) 🟢
1. Close and reopen BlockShuffler
2. Cmd+O → open the saved .bsp
3. **Expected:** All blocks, clips, links, stacks, names, colours, probabilities, and flags are restored exactly

### T11.3 — Round-trip integrity 🟢
1. Save → Load → compare all values
2. **Expected:** Every setting (clip probabilities, stack counts, overlap flags, link probabilities, tempo values, retain-tempo flags, done flags, song-ender flags) survives the round-trip

### T11.4 — Missing audio file on load 🟡
1. Save a project, then move/delete one of the referenced audio files
2. Load the project
3. **Expected per spec:** Warning should appear
4. **Actual (per handover):** Clip loads with empty buffer silently. **No warning shown.** This is a known gap.

---

## TEST GROUP 12: Undo/Redo

### T12.1 — Undo add block 🟢
1. Add a block
2. Cmd+Z
3. **Expected:** Block disappears

### T12.2 — Redo 🟢
1. After undo, Cmd+Shift+Z
2. **Expected:** Block reappears

### T12.3 — Undo block removal 🟢
1. Delete a block
2. Cmd+Z
3. **Expected:** Block returns with all its clips intact

### T12.4 — Undo for clip probability 🔴
1. Change a clip's probability from 50% to 90%
2. Cmd+Z
3. **Expected per spec:** Reverts to 50%
4. **Actual (per handover):** "Slider drag directly mutates clip->probability... drag-end undo hook has not been added." **Will NOT undo.**

### T12.5 — Undo for overlap probability 🔴
Same as T12.4 but for block overlap probability. **Known gap per handover.**

### T12.6 — Undo for stack count changes 🔴
**Known gap per handover.** Stack count +/- and weight changes are not undoable.

### T12.7 — Undo for context menu actions 🔴
Right-click actions (set colour, mark done, set overlapping). **Known gap per handover.**

---

## TEST GROUP 13: Edge Cases & Robustness

### T13.1 — Empty project playback
1. Launch fresh, press Play with no blocks
2. **Expected:** Nothing plays. No crash.

### T13.2 — Block with no clips
1. Create a block but add no clips
2. Play
3. **Expected:** That block is skipped silently. No crash.

### T13.3 — All clips at 0% probability
1. Set all clips in a block to 0% probability
2. Play
3. **Expected:** Graceful handling — either skip block or pick one anyway. No crash.

### T13.4 — Single block, single clip
1. One block, one clip, probability 100%
2. Play and export
3. **Expected:** Plays and exports correctly. Simplest possible case works.

### T13.5 — Very long audio file
1. Load a 30+ minute audio file as a clip
2. Set start/end markers
3. Play and export
4. **Expected:** Handles large buffers. No memory crash or truncation.

### T13.6 — Many blocks (stress test)
1. Create 50+ blocks with 3 clips each
2. Play
3. **Expected:** Resolves and plays without significant delay

### T13.7 — Rapid play/stop cycling
1. Press Play/Stop rapidly 20 times
2. **Expected:** No crash, no audio glitches, no deadlock

---

## SUMMARY OF KNOWN GAPS (from handover)

| # | Feature | Status |
|---|---------|--------|
| 1 | Tempo stretching (lead-in/tail tempo adjustment) | 🔴 Stub only |
| 2 | Clip probability undo | 🔴 Not wired |
| 3 | Block overlap probability undo | 🔴 Not wired |
| 4 | Stack count undo | 🔴 Not wired |
| 5 | Context menu action undo | 🔴 Not wired |
| 6 | Missing file warning on project load | 🔴 Silent fail |
| 7 | Zoom state persistence across block selection | 🟡 Resets |
| 8 | Mobile companion player | 🔴 Not in scope of this repo |

---

## EXECUTION NOTES

- Run tests with **at least 3 different audio files** (short percussive, long pad, spoken word) to catch format/length edge cases
- For probability tests (T2.8, T5.3, T6.2, T7.2), run **20+ iterations** minimum and tally results
- Export tests should verify files in an **external player** (Audacity, VLC) not just within BlockShuffler
- Save your test project as `test_full.bsp` after setting up all features for regression testing
