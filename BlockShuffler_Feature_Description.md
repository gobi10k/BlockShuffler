# BlockShuffler — Complete Feature Description

**Build date:** 2026-03-22
**Last updated:** 2026-03-22
**Status:** Fully compilable standalone desktop application (macOS)

---

## Overview

BlockShuffler is a standalone desktop audio application that lets artists arrange songs from interchangeable audio blocks. Each block holds one or more audio clips; the app randomly picks which clips play at runtime based on configurable probability weights. Arrangements are non-destructive — you configure the probabilities and structure once, then every playback or export produces a unique, deterministic-but-randomised version of the song.

---

## Application Layout

The window is divided into four zones:

| Zone | Location | Purpose |
|------|----------|---------|
| Waveform Editor | Top-left (main area) | Shows all clips in the selected block; drag markers and edit clip settings |
| Inspector Panel | Right sidebar | Edit clip and block properties: probability, tempo, flags, links, stack controls |
| Block Strip | Bottom | Horizontal scrollable list of all blocks in arrangement order |
| Transport Bar | Very bottom | Play / Stop / Rewind / Export / Save / Open controls and time display |

---

## Block Management

### Adding Blocks
- Click the **+** button at the right end of the block strip.
- Each new block is auto-named **"Block 1", "Block 2", "Block 3"**, etc., incrementing based on how many blocks already exist.
- Each new block receives a distinct color from an 8-color palette (Red → Orange → Yellow → Green → Cyan → Blue → Purple → Pink), cycling if more than 8 blocks are created.
- Block color is visible as a prominent **8 px colored top bar** plus a **subtle 12% tint** over the entire block tile background, making the color clearly distinguishable at a glance.

### Removing Blocks
- Right-click **anywhere** on a block tile (including the name area) → **Delete Block**.
- This removes the block and renumbers positions.

### Renaming Blocks
- Double-click a block tile to edit the name inline, or right-click → **Rename**.
- Press Enter or click away to confirm.
- Rename is **undoable**: the snapshot is captured when the editor opens and recorded when the editor closes, but only if the name actually changed.

### Setting Block Color
- Right-click → **Set Color** → choose from 8 named preset colors.
- Color change is immediately visible on the tile and is **undoable** (Cmd+Z).

### Reordering Blocks
- Click and drag a block left or right in the strip.
- Drop in the desired position to reorder.
- The operation is undoable.

### Marking a Block as Done
- Right-click → **Mark as Done** (toggleable), or use the toggle in the Inspector Panel.
- Done blocks are dimmed (45% alpha) and show "DONE" text at the bottom.
- Done blocks are excluded from randomised playback and export.
- Toggling is undoable.

---

## Clip Management

### Adding Clips
- **Drag audio files** from Finder onto the Waveform Editor area or directly onto a block tile.
- Or click **"+ Add Clip"** to open a file browser.
- Supported formats: **.wav, .aiff, .aif, .flac, .ogg, .mp3**
- The clip name defaults to the file name without extension.
- startMark is set to 0 and endMark to the full file length on import.

### Right-Click Clip Menu
Right-clicking **anywhere on a clip row** (including on the name label) opens the context menu. Options:
- **Rename** — enters inline text editing
- **Set Color** → pick from the 8-color preset palette
- **Song Ender** (toggle) — marks the clip as one that ends the song
- **Mark as Done** (toggle) — excludes this clip from random selection
- **Remove Clip** — deletes the clip from the block

The context menu callback is protected with a `SafePointer` to prevent crashes if the UI is rebuilt while the menu is open.

### Removing Clips
- Right-click → **Remove Clip**, or select a clip and press Delete/Backspace.

### Clip Probability Slider
- In the Inspector Panel, the **Probability (%)** slider (0–100) controls how likely each clip is to be chosen when its block plays.
- Multiple clips in a block compete by weighted probability.
- Example: Clip A at 90%, Clip B at 10% → A plays ~9× more often.
- Dragging the slider is **undoable**: the undo snapshot is captured when the drag starts and recorded when the drag ends.

---

## Waveform Editor

### Display
- Each clip in the selected block has its own waveform row (dark background, light waveform trace).
- A colored header bar shows the clip name and probability percentage.
- Vertical grid lines reflect the clip's configured tempo (BPM).

### Start / End Markers
- **Green triangle + line** = Start marker (defines the "body" start of the clip).
- **Red triangle + line** = End marker (defines the "body" end of the clip).
- Regions before the start marker and after the end marker are dimmed, representing the **lead-in** and **tail** audio respectively.
- Drag markers left/right to adjust. Markers snap to the tempo grid by default.
- Hold **Shift** while dragging to bypass grid snap and move freely.

### Grid Snap
- Grid lines are calculated from the clip's **Tempo (BPM)** setting.
- Grid formula: `samplesPerBeat = (sampleRate × 60) / tempo`
- Markers snap to the nearest grid line when released within snap distance.

### Arrow Key Nudge
- After selecting a clip, use **left/right arrow keys** to nudge the clip's `gridOffsetSamples` by ±1 beat grid unit.
- This shifts when the clip begins in the timeline relative to the grid, without moving the start/end markers within the audio.
- Keyboard focus is automatically grabbed when a clip is selected, so arrow keys work immediately.

### Tempo Field
- Set the clip's BPM in the Inspector Panel's **Tempo (BPM)** text box.
- Changing tempo recalculates and redraws the grid lines in the waveform.
- Commit with Enter or by clicking away.
- Tempo changes are **undoable**.

### Zoom
- **Cmd + scroll wheel** over the waveform area zooms in or out (1× to 32×).
- Horizontal scrollbar appears when content exceeds the view width.
- Zoom level persists when switching between blocks.
- Implemented via a `ZoomableViewport` subclass that intercepts Cmd+scroll before the default viewport scroll handler, eliminating a previous crash caused by re-entrant event routing.

---

## Lead-In / Tail-End Handling

### What They Are
- **Lead-in**: The audio BEFORE the start marker. Plays before the block's "body."
- **Tail**: The audio AFTER the end marker. Plays after the block's "body."

### Crossfade Behavior
- During playback, clip A's tail overlaps with clip B's lead-in.
- The tail fades out (gain 1.0 → 0.0) and the lead-in fades in (gain 0.0 → 1.0).
- The body of each clip plays at full gain.
- Alignment: clip A's endMark aligns in time with clip B's startMark — bodies play back-to-back with tails and lead-ins overlapping around the join.
- This crossfade is rendered correctly in both live playback and all export formats (WAV, FLAC, BSF bundle).

### Tempo Stretching
- By default, when two adjacent clips have different tempos, the **tail** of the earlier clip is time-stretched to match the **next** clip's tempo, and the **lead-in** of the later clip is time-stretched to match the **previous** clip's tempo.
- This creates smooth rhythmic transitions: the crossfade region grooves at the tempo of whichever body it is moving toward.
- Stretching is implemented using **WSOLA (Waveform Similarity Overlap-Add)** — a pitch-preserving time-stretch algorithm. Speed changes without pitch shifting, so transitions sound natural even at extreme ratios.
- WSOLA parameters: 2048-sample Hanning-windowed frames, 512-sample analysis hop, synthesis hop scaled by the stretch ratio, ±256-sample normalized cross-correlation search for the best analysis position per frame.
- Stretched buffers are **pre-computed offline** during arrangement resolution (`ArrangementResolver::resolve()` on the UI thread) and stored as `shared_ptr<AudioBuffer>` on each `ResolvedEntry`. The audio callback and export renderer do a simple 1:1 mix of pre-computed buffers — no real-time allocation.
- The stretch ratio is: `ratio = thisClip.tempo / adjacentClip.tempo`. Ratios > 1 produce longer (slower) output; ratios < 1 produce shorter (faster) output.
- Overlapping-block layers and clips within a simultaneous stack slot are never stretched (they do not drive tempo transitions).

### Retain Lead-In / Retain Tail Tempo
- Inspector Panel checkboxes **"Retain Lead-In Tempo"** and **"Retain Tail Tempo"** opt a clip out of tempo stretching.
- When **Retain Lead-In Tempo** is checked, the lead-in plays at the clip's original recorded speed regardless of the previous clip's tempo.
- When **Retain Tail Tempo** is checked, the tail plays at the clip's original recorded speed regardless of the next clip's tempo.
- Both flags are stored in the project and round-trip through save/load.
- Toggling either flag is **undoable**.

---

## Block Stacking

### Creating a Stack
- Right-click block A → **Stack with…** → click block B.
- Both blocks appear vertically stacked in the same horizontal slot in the block strip.
- Stacked blocks share a `stackGroup` ID (shown as a badge in the top-right corner of each tile).

### Stack Play Count
- In the Inspector Panel (visible when a stacked block is selected), the **Stack Play Count** editor shows weighted count options.
- Example: play count = 1 at weight 0.7, play count = 2 at weight 0.3 → plays 1 block 70% of the time, 2 blocks 30% of the time.
- Use **+Entry** to add more count options, **−** / **+** buttons to adjust the count, weight sliders to set relative probabilities, and **×** to remove an entry.
- All stack count mutations (+/−, weight slider drag, add entry, remove entry) are **undoable**.

### Stack Play Mode
- **Sequential** (default): chosen blocks play one after another in random order.
- **Simultaneous**: chosen blocks play layered on top of each other at the same timeline position.

### Overlapping Blocks
- Right-click a stacked block → **Set as Overlapping** (or toggle in Inspector).
- Overlapping blocks are displayed with a **dashed border** and excluded from normal stack slot scheduling.
- They play "on top of" whichever normal block plays in that slot.
- **Overlap Probability** (0–100%) in the Inspector sets the chance the overlapping block triggers on each playback.
- Setting to 0% means never, 100% means always.
- Overlap probability changes are **undoable**.

---

## Block Linking (Swap)

### Creating a Link
- Right-click block A → **Link to…** → click block B.
- A curved arc is drawn between the two blocks in the block strip.

### Swap Probability
- Shown in the Inspector Panel under **LINKS** when a linked block is selected.
- Set to 100% → the blocks always swap positions in the arrangement.
- Set to 0% → they never swap.
- Intermediate values give a weighted chance to swap.
- Changes are **undoable** (undo snapshot captured at drag start, recorded at drag end).

### Removing a Link
- Right-click a linked block → **Remove Link** (available in block context menu if a link exists), or manage via inspector.

### Visual Overlay
- The `BlockLinkOverlay` draws curved arcs above the block strip, updating as blocks are scrolled or rearranged.

---

## Song-Ending Clips

- In the Inspector Panel, check **"Song Ender"** on any clip.
- When the arrangement resolver picks a song-ender clip for playback or export, the arrangement is **truncated** at that clip's end mark (including its tail).
- Blocks that would have come after the song-ender are skipped.
- Works with stacked blocks: if the song-ender clip is in a stacked block and is selected by probability, it still ends the song.
- "Song Ender" toggle is **undoable**.

---

## In-Editor Playback

### Transport Controls
- **Play (Space / ▶)**: Resolves the arrangement (randomises block/clip selection using current weights) and begins audio playback through the system audio device.
- **Stop (■)**: Immediately stops playback. The playhead rewinds to the start.
- **Rewind**: Moves the playhead back to 0 without stopping (if already stopped) or re-resolves for a new arrangement.
- **Space bar** toggles play/stop.

### Audio Device
- Audio is routed through JUCE's `AudioDeviceManager` using the default system output (stereo).
- The engine sample rate is set by `prepareToPlay` when the device opens.
- The `PlaybackEngineSource` adapter bridges the custom `PlaybackEngine` to JUCE's `AudioSourcePlayer`.

### Playhead
- A **red vertical line** moves across the waveform view during playback.
- The currently-playing block's tile shows a **thicker green top bar** instead of its normal color bar.
- Time display (MM:SS elapsed / MM:SS total) is updated at 30 fps in the transport bar.

### Each Playback is Unique
- On every Play press, `ArrangementResolver::resolve()` runs a fresh weighted-random pass, so clip/block choices vary each time.

---

## Export

### How to Export
1. Click **Export** (↗) in the transport bar.
2. A dialog asks for the export format: **BSF Bundle**, **FLAC**, or **WAV**.
3. A file-save dialog opens.
4. A progress bar dialog appears during rendering.

### Export Formats

| Format | Description |
|--------|-------------|
| **WAV** | Single-file flat export of the resolved arrangement, fully mixed to stereo. Includes lead-in/tail crossfades and tempo stretching. |
| **FLAC** | Same as WAV but lossless FLAC encoding. |
| **BSF Bundle** | ZIP archive containing individual FLAC clips + `manifest.json`. Designed for playback on a mobile companion player. |

### BSF Bundle Contents
```
song.bsf  (rename to .zip to inspect)
├── manifest.json          — resolved arrangement: clip list + playback sequence
├── model.json             — full probabilistic model (for mobile re-randomisation)
└── clips/
    ├── clip_001.flac      — each resolved clip rendered individually
    ├── clip_002.flac
    └── ...
```
The manifest uses JSON integer strings for large sample values to avoid floating-point precision loss.

### manifest.json
Contains the concrete resolved arrangement: `totalDurationSamples`, `sampleRate`, `bitDepth`, the clip catalogue with `startSample`/`durationSamples`, and the `arrangement` array giving each clip's `startTime`. This is backward-compatible and sufficient for a simple sequential player.

### model.json
Contains the full probabilistic model so a mobile player can **re-randomise on-device** without needing the desktop app. Schema:
```json
{
  "version": 1,
  "name": "My Song",
  "sampleRate": 48000,
  "blocks": [
    {
      "id": "...",
      "name": "Verse",
      "color": "#CC4444",
      "position": 0,
      "stackGroup": -1,
      "stackPlayCount": { "values": [1, 2], "weights": [0.7, 0.3] },
      "stackPlayMode": "sequential",
      "isOverlapping": false,
      "overlapProbability": 0.0,
      "isDone": false,
      "clips": [
        {
          "id": "...",
          "name": "Acoustic",
          "embeddedFile": "clips/clip_001.flac",
          "probability": 1.0,
          "tempo": 120.0,
          "retainLeadInTempo": false,
          "retainTailTempo": false,
          "isSongEnder": false,
          "isDone": false,
          "startMark": "0",
          "endMark": "529200"
        }
      ]
    }
  ],
  "links": [
    { "blockA": "...", "blockB": "...", "swapProbability": 0.3 }
  ]
}
```
`embeddedFile` is only present for clips that were included in this particular resolved arrangement. Clips in other blocks that didn't appear in this resolution have no `embeddedFile` key. The mobile player should use manifest.json for immediate playback and model.json for re-randomisation.

### Export is Non-Blocking
The export job runs on a background thread. The UI remains interactive during rendering.

---

## Project Save / Load

### Save
- **Cmd+S**: Save to current file, or prompt for location if new.
- Project files use the **.bsp** extension (BlockShuffler Project), stored as JSON.

### Load
- **Cmd+O**: Open a `.bsp` file.
- All blocks, clips, names, colors, probabilities, tempo values, stack groups, links, flags (isDone, isSongEnder, retainLeadInTempo, retainTailTempo) are fully restored.
- Audio files are stored as absolute paths in the `.bsp` file.

### Missing File Warning
- If any audio file referenced in a `.bsp` cannot be found on disk, a **warning dialog** lists every missing file path before the project opens.
- Missing clips load silently (empty buffer) and are shown in the waveform as "No audio loaded."

### New Project
- **Cmd+N**: Creates a fresh project with a single empty block.

---

## Undo / Redo

Undo history uses snapshot-based `juce::UndoManager`. Every supported mutation records a full project JSON snapshot before and after the change.

| Action | Undoable? |
|--------|-----------|
| Add block | ✅ |
| Remove block (restores all clips) | ✅ |
| Move/reorder block | ✅ |
| Stack blocks | ✅ |
| Add/remove link | ✅ |
| Set link swap probability | ✅ |
| Set block color | ✅ |
| Block rename (name label edit) | ✅ |
| Toggle block "Mark as Done" | ✅ |
| Toggle block "Set as Overlapping" | ✅ |
| Clip probability slider drag | ✅ |
| Block overlap probability slider drag | ✅ |
| Toggle clip "Song Ender" | ✅ |
| Toggle clip "Mark as Done" | ✅ |
| Toggle "Retain Lead-In Tempo" | ✅ |
| Toggle "Retain Tail Tempo" | ✅ |
| Toggle "Simultaneous" stack mode | ✅ |
| Toggle "Mark Block as Done" | ✅ |
| Clip tempo field (Enter key) | ✅ |
| Stack count +/– buttons | ✅ |
| Stack count weight slider drag | ✅ |
| Add stack count entry | ✅ |
| Remove stack count entry | ✅ |

**Keyboard shortcuts:**
- `Cmd+Z` — Undo
- `Cmd+Shift+Z` / `Cmd+Y` — Redo

---

## Randomisation Engine

The `ArrangementResolver::resolve()` method produces a `ResolvedArrangement` — a concrete ordered list of clips with timeline positions — from the probabilistic model:

1. **Link swaps**: Links are processed in random order. Each link rolls against its `swapProbability`; if it fires, the two blocks swap positions in the arrangement.
2. **Sort by position**: Blocks are sorted by their (possibly swapped) position index.
3. **Group into slots**: Blocks sharing a `stackGroup` occupy the same time slot.
4. **Resolve each slot**:
   - Single non-stacked block: pick one clip by weighted random.
   - Stacked blocks: pick how many to play (from `stackPlayCount` weighted values), then pick that many blocks by weighted random sampling; run them sequentially or simultaneously per `stackPlayMode`. Apply overlapping block dice rolls.
5. **Song ender check**: Truncate the arrangement after any song-ender clip.
6. **Timeline cursor**: Advance by each body length (`endMark – startMark`). Tail of the last entry extends `totalDurationSamples` accounting for tempo stretch, so the audio engine doesn't cut off early.
7. **Tempo stretch ratios**: After building the entry list, a post-processing pass walks adjacent primary (non-overlapping) entries and assigns `leadInStretchRatio` and `tailStretchRatio` to each entry based on neighbouring clip tempos and the `retainLeadInTempo`/`retainTailTempo` flags. Entries in simultaneous stack slots are skipped.

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Space | Play / Stop |
| Cmd+S | Save project |
| Cmd+O | Open project |
| Cmd+N | New project |
| Cmd+Z | Undo |
| Cmd+Shift+Z | Redo |
| Cmd+Y | Redo |
| ← / → (clip selected) | Nudge clip `gridOffsetSamples` by ±1 grid unit |
| Delete / Backspace (clip selected) | Remove selected clip |
| Esc | Cancel pending link/stack mode |
| Cmd+Scroll | Zoom waveform in/out (1× – 32×) |

---

## Sample-Rate Conversion

Clips may be recorded at different native sample rates (44.1 kHz, 48 kHz, 96 kHz, etc.). BlockShuffler handles this transparently:

- When a clip is imported, `Clip::loadFromFile` checks whether the file's native sample rate differs from the project sample rate.
- If they differ by more than 0.5 Hz, the PCM buffer is resampled using JUCE's `LagrangeInterpolator` before being stored in `Clip::audioBuffer`.
- All further processing (playback, export, WSOLA stretch) operates on the resampled buffer at the project sample rate — no runtime conversion required.
- The clip's original `nativeSampleRate` is stored in the `.bsp` project file. On reload, start/end marker positions (originally in native-SR samples) are scaled by `projectSR / nativeSR` so they remain correctly aligned after resampling.

---

## Known Limitations

| Item | Notes |
|------|-------|
| Mobile companion player | Out of scope for this repository. The `.bsf` export format is designed for a separate player app. |
