# BlockShuffler — CLAUDE.md (Claude Code Handoff)

## Project Overview

**BlockShuffler** is a standalone desktop audio application (JUCE/C++) for randomizing song arrangements. Artists split songs into named **blocks** (Intro, Verse, Chorus, etc.), each containing one or more **clips** (audio files). Each clip has a probability weight determining how likely it is to play when that block is reached. Blocks can be stacked, linked, overlapped, and reordered — all with weighted randomization. The final arrangement exports losslessly for playback on a companion mobile player.

**Reference image:** See `/reference/ui-mockup.png` for the original visual concept. The UI has three zones: a waveform editor (top), a block arrangement strip (bottom), and a properties/inspector panel (right).

---

## Technology Stack

| Component | Technology | Rationale |
|-----------|-----------|-----------|
| Framework | JUCE 7+ (C++17) | Cross-platform audio, VST3 optional, mature waveform/transport |
| Audio I/O | `juce::AudioFormatManager`, `juce::AudioTransportSource` | Native support for WAV, AIFF, FLAC, OGG |
| Waveform | `juce::AudioThumbnail` | Efficient waveform rendering with zoom |
| Drag & drop | `juce::DragAndDropContainer` / `juce::FileDragAndDropTarget` | OS-native file drag-in, internal block reorder |
| Serialization | JSON via `juce::var` / `juce::JSON` | Project save/load, export manifest |
| Export format | Custom `.bsf` bundle (ZIP containing FLAC clips + JSON manifest) | Lossless, portable, parseable by mobile player |
| Build | CMake with JUCE's CMake API | Standard JUCE build pattern |

### Build Commands
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target BlockShuffler_Standalone
# Optional VST3:
cmake --build build --target BlockShuffler_VST3
```

### Dependencies
- JUCE 7.x (added as git submodule at `/libs/JUCE`)
- C++17 compiler (Clang 14+, GCC 12+, MSVC 2022+)
- libFLAC (bundled with JUCE)

---

## Architecture

### Data Model (all in `Source/Model/`)

The data model is the single source of truth. The UI observes it via `juce::ChangeBroadcaster` / `juce::ChangeListener`. All mutations go through model methods that fire change notifications.

```
Project
├── name: String
├── sampleRate: double (project-wide, typically 44100 or 48000)
├── blocks: OwnedArray<Block>          // ordered left-to-right
├── links: OwnedArray<BlockLink>       // swap-links between blocks
└── exportSettings: ExportSettings

Block
├── id: Uuid
├── name: String                       // "Intro", "Verse 1", "Chorus", etc.
├── color: Colour
├── clips: OwnedArray<Clip>            // possible clips for this block
├── position: int                      // horizontal slot index in arrangement
├── stackGroup: int                    // -1 = no stack; same value = stacked together
├── stackPlayCount: WeightedValue<int> // how many from stack to play (randomizable)
├── stackPlayMode: enum { Sequential, Simultaneous }
├── isOverlapping: bool                // true = plays ON TOP of a non-overlapping block
├── overlapProbability: float          // 0.0–1.0, chance this overlapping block plays
├── isDone: bool                       // "mark as done" flag
└── tempo: double                      // BPM for grid snapping (per-block, affects grid only)

Clip
├── id: Uuid
├── name: String
├── color: Colour
├── audioFile: File                    // source audio path
├── audioBuffer: AudioBuffer<float>    // loaded PCM (owned, not shared)
├── sampleRate: double                 // native sample rate of this clip
├── startMark: int64                   // sample position of the "start" trigger
├── endMark: int64                     // sample position of the "end" trigger
├── probability: float                 // 0.0–1.0, weight for random selection
├── tempo: double                      // BPM for this clip's grid
├── retainLeadInTempo: bool            // if true, lead-in keeps original tempo
├── retainTailTempo: bool              // if true, tail keeps original tempo
├── isSongEnder: bool                  // if true, song stops after this clip
├── isDone: bool
└── gridOffsetSamples: int64           // manual nudge left/right to align to grid

BlockLink
├── blockA: Uuid
├── blockB: Uuid
└── swapProbability: float             // chance these two blocks swap positions

WeightedValue<T>   (template / struct)
├── values: Array<T>                   // e.g. [2, 3, 4]
└── weights: Array<float>             // e.g. [0.5, 0.3, 0.2]

ExportSettings
├── outputFormat: enum { FLAC, WAV }
├── sampleRate: double
└── bitDepth: int
```

### Key Architectural Rules

1. **Model owns all state.** UI components never store canonical data — they read from the model and call model methods to mutate.
2. **Undo/Redo via `juce::UndoManager`.** Every model mutation is an `UndoableAction`. Wire this early — retrofitting is painful.
3. **Audio buffers are loaded once** into `Clip::audioBuffer` on import, then referenced. Never re-read from disk during playback.
4. **Thread safety:** The audio thread reads the model (via a lock-free snapshot or `juce::ReadWriteLock`). UI thread writes. Use `juce::AbstractFifo` or a copy-on-write arrangement snapshot for the audio callback.
5. **Randomization is resolved at play-time** (or export-time), not baked into the model. The model stores probabilities; the `ArrangementResolver` produces a concrete `ResolvedArrangement` each time.

---

## Source File Layout

```
Source/
├── Main.cpp                           // JUCEApplication entry
├── MainComponent.h/.cpp               // Top-level component, DragAndDropContainer
│
├── Model/
│   ├── Project.h/.cpp
│   ├── Block.h/.cpp
│   ├── Clip.h/.cpp
│   ├── BlockLink.h/.cpp
│   └── Serialization.h/.cpp           // toJSON / fromJSON for save/load/export
│
├── Audio/
│   ├── ArrangementResolver.h/.cpp     // Resolves probabilities → concrete arrangement
│   ├── PlaybackEngine.h/.cpp          // AudioSource that plays a ResolvedArrangement
│   ├── TempoStretcher.h/.cpp          // Time-stretch for lead-in/tail tempo retention
│   └── ExportRenderer.h/.cpp          // Offline render to FLAC/WAV
│
├── UI/
│   ├── BlockStrip.h/.cpp              // Horizontal scrollable block arrangement
│   ├── BlockComponent.h/.cpp          // Single block tile (color, name, drag handle)
│   ├── BlockStackView.h/.cpp          // Vertical stack display for stacked blocks
│   ├── ClipListPanel.h/.cpp           // List of clips in selected block
│   ├── ClipWaveformView.h/.cpp        // Waveform with start/end markers, grid
│   ├── InspectorPanel.h/.cpp          // Right-side properties (probability, tempo, flags)
│   ├── TransportBar.h/.cpp            // Play/Stop/Export controls
│   ├── BlockLinkOverlay.h/.cpp        // Visual arcs showing links between blocks
│   └── LookAndFeel_BlockShuffler.h/.cpp // Custom dark theme matching mockup
│
└── Utils/
    ├── WeightedRandom.h               // Template: pick from weighted values
    ├── GridSnap.h                      // Snap sample position to tempo grid
    └── UuidGenerator.h
```

---

## Feature Implementation Guide

### 1. Block Management

**Add block:** `+` button at end of BlockStrip. Creates a new Block with default name "Block N", random pastel color, position = end.

**Remove block:** Right-click context menu → "Delete Block". Confirm if block has clips.

**Rename:** Double-click block name label → inline `juce::TextEditor`.

**Color:** Right-click → "Set Color" → `juce::ColourSelector` popup.

**Drag to reorder:** `BlockComponent` implements `juce::DragAndDropTarget`. On drop, update `Block::position` values for all affected blocks. Animate with `juce::ComponentAnimator`.

**Mark as done:** Checkbox or right-click toggle. Visually dim the block (reduce alpha) and show checkmark overlay.

### 2. Clip Management

**Add clip (drag from OS):** `ClipListPanel` implements `juce::FileDragAndDropTarget`. Accept WAV, AIFF, FLAC, OGG. On drop:
1. Copy file to project media folder
2. Load into `AudioBuffer` via `AudioFormatManager`
3. Create `Clip` object, auto-set startMark = 0, endMark = buffer length
4. Add to selected block's clip list

**Add clip (browse):** Button opens `juce::FileChooser` with audio format filter.

**Remove clip:** Right-click → "Remove Clip" or Delete key when selected.

**Probability slider:** In InspectorPanel. Range 0–100 (displayed as %). Stored as 0.0–1.0 float. When a block plays, clips are chosen via weighted random selection using these probabilities.

### 3. Waveform Editor & Grid

**Display:** `ClipWaveformView` uses `juce::AudioThumbnail` for efficient rendering. Dark background, light waveform (match mockup aesthetic).

**Start/End markers:** Draggable vertical lines (green for start, red for end — visible in mockup). Snap to grid when within threshold.

**Grid calculation:**
```cpp
double samplesPerBeat = (sampleRate * 60.0) / tempo;
double samplesPerGridLine = samplesPerBeat / gridSubdivision; // 1, 1/2, 1/4, etc.
int64 snappedPosition = juce::roundToInt(position / samplesPerGridLine) * samplesPerGridLine;
```

**Grid nudge (left/right arrow keys):** Move clip's `gridOffsetSamples` by ±1 grid unit. This shifts the clip relative to the grid without moving the markers.

**Tempo field:** Number box in inspector. Changing tempo recalculates grid lines but does NOT resample the audio.

### 4. Lead-In / Tail-End Handling

A clip's audio can extend before `startMark` (lead-in) and after `endMark` (tail). During playback:

- The **lead-in** of the current clip overlaps with the tail of the previous clip
- The **tail** of the current clip overlaps with the lead-in of the next clip
- Alignment: the `endMark` of clip N aligns with the `startMark` of clip N+1

**Tempo adjustment for lead-in/tail:**
- By default, lead-in and tail audio are time-stretched to match the tempo of the adjacent block's clip
- If `retainLeadInTempo` is true, the lead-in plays at original speed (may cause timing drift — that's the artist's choice)
- If `retainTailTempo` is true, the tail plays at original speed
- Use a simple WSOLA or phase-vocoder stretch (or JUCE's `TimeSliceThread` with `ResamplingAudioSource` for basic rate changes)

### 5. Block Stacking

When multiple blocks share the same `stackGroup` value:
- They render vertically stacked in the BlockStrip (see mockup — blocks pile up)
- `stackPlayCount` determines how many from the stack actually play
- `stackPlayMode` decides sequential (one after another in random order) or simultaneous (layered)

**Resolution logic (in ArrangementResolver):**
```
For each stack group:
  1. Resolve stackPlayCount (if weighted, pick randomly)
  2. Collect all non-overlapping blocks in the stack
  3. Pick `stackPlayCount` of them via weighted random (using block probability)
  4. If Sequential: shuffle picked blocks, insert into arrangement in sequence
  5. If Simultaneous: layer them (mix audio at same time position)
  6. For each picked block, check overlapping blocks in same stack:
     - Roll each overlapping block's overlapProbability
     - Winners get layered on top of the non-overlapping block they're assigned to
```

### 6. Block Linking (Swap)

Right-click block → "Link to..." → click target block. Creates a `BlockLink` with editable `swapProbability`.

**Visual:** `BlockLinkOverlay` draws curved arcs between linked blocks (like patch cables).

**Resolution:** Before arranging, `ArrangementResolver` iterates all links. For each link, roll against `swapProbability`. If hit, swap blockA and blockB positions in the arrangement. Process links in random order to avoid bias.

### 7. Overlapping Blocks

An overlapping block (flagged via `isOverlapping = true`) doesn't occupy its own time slot. Instead, it layers on top of whatever non-overlapping block plays at that position.

- Only meaningful when stacked with at least one non-overlapping block
- Has its own `overlapProbability` — each time the position plays, roll to see if the overlay triggers
- The overlapping clip's audio starts at the same time as the underlying clip's `startMark`

### 8. Song-Ending Clips

If a clip has `isSongEnder = true`, playback stops after that clip's `endMark` (including its tail, if any). This means the arrangement resolver should check: after resolving the full block sequence, if any selected clip is a song ender, truncate the arrangement at that point.

### 9. In-Editor Playback

`PlaybackEngine` extends `juce::AudioSource`:
1. On Play: `ArrangementResolver` produces a `ResolvedArrangement` (ordered list of clips with timing, volume, overlap info)
2. `PlaybackEngine` reads from this resolved arrangement, mixing overlapping clips, handling lead-in/tail crossfades
3. Transport controls: Play, Stop, rewind to start
4. Playhead indicator (red vertical line in mockup) moves across waveform and block strip

**Thread safety pattern:**
```cpp
// UI thread:
auto newArrangement = resolver.resolve(project);
playbackEngine.swapArrangement(std::move(newArrangement)); // lock-free swap

// Audio thread:
void getNextAudioBlock(AudioSourceChannelInfo& info) {
    auto* arr = currentArrangement.load(); // atomic pointer
    // read from arr, fill buffer
}
```

### 10. Export

**Export flow:**
1. Resolve arrangement (same as playback)
2. Render offline via `ExportRenderer`: iterate arrangement, write mixed audio to buffer
3. Encode to FLAC (lossless)
4. Package as `.bsf` (BlockShuffler Format):

```
song.bsf (ZIP archive)
├── manifest.json          // arrangement metadata, clip references, block order
├── clips/
│   ├── clip_001.flac      // individual rendered clips (pre-mixed where overlapping)
│   ├── clip_002.flac
│   └── ...
└── arrangement.json       // resolved arrangement: ordered clip sequence with timing
```

**manifest.json schema:**
```json
{
  "version": 1,
  "title": "My Song",
  "sampleRate": 48000,
  "bitDepth": 24,
  "totalDurationSamples": 10584000,
  "clips": [
    {
      "id": "clip_001",
      "file": "clips/clip_001.flac",
      "startSample": 0,
      "durationSamples": 529200
    }
  ],
  "arrangement": [
    { "clipId": "clip_001", "startTime": 0 },
    { "clipId": "clip_002", "startTime": 529200 }
  ]
}
```

The mobile player reads `manifest.json`, loads clips, and plays them in sequence. This format is intentionally simple so any platform (iOS/Android/web) can parse it.

**Alternative "flat" export:** Also offer a single-file FLAC/WAV export (fully rendered, no randomization — just the current resolved arrangement baked down).

---

## UI Design Spec

### Layout (match mockup)

```
┌─────────────────────────────────────────────────────────┬──────────────┐
│                   WAVEFORM EDITOR                        │  INSPECTOR   │
│  ┌──────────────────────────────────────────────────┐   │              │
│  │ Clip: "Acoustic"    Tempo: 170                    │   │ Probability  │
│  │ [===waveform with start/end markers============]  │   │   [30] ☑     │
│  ├──────────────────────────────────────────────────┤   │   [10] ☑     │
│  │ Clip: "Heavy"       Tempo: 120                    │   │   [ 1] ☐     │
│  │ [===waveform=============================]        │   │              │
│  ├──────────────────────────────────────────────────┤   │ Mark as done │
│  │ Clip: "Bossa Nova"  Tempo: 120                    │   │              │
│  │ [===waveform=============================]        │   │              │
│  └──────────────────────────────────────────────────┘   │              │
├─────────────────────────────────────────────────────────┤              │
│                    BLOCK STRIP                           │              │
│  ┌─────┐┌──────┐┌────────┐┌───────┐┌──────┐┌─────┐    │              │
│  │Intro││Verse1││Pre-chor││Chorus ││Jazz 1││ ... │ [+]│              │
│  └─────┘└──────┘└────────┘└───────┘│Break2│└─────┘    │              │
│                                     │Solo 5│           │              │
│                                     └──────┘           │              │
│  ◄━━━━━━━━━━━ scrollbar ━━━━━━━━━━━━━━━━━━━━━━━━━━━►  │              │
└─────────────────────────────────────────────────────────┴──────────────┘
│                      TRANSPORT BAR                                     │
│  [◄◄] [▶ Play] [■ Stop] [↗ Export]           00:00 / 04:32           │
└────────────────────────────────────────────────────────────────────────┘
```

### Color Palette (dark theme)

```cpp
// In LookAndFeel_BlockShuffler
const Colour bgDark       { 0xFF1E1E1E };  // main background
const Colour bgMedium     { 0xFF2D2D2D };  // panel backgrounds
const Colour bgLight      { 0xFF3C3C3C };  // waveform background
const Colour waveformColor{ 0xFFCCCCCC };  // waveform fill
const Colour gridLine     { 0xFF4A4A4A };  // tempo grid lines
const Colour startMarker  { 0xFF00CC00 };  // green start marker
const Colour endMarker    { 0xFFCC0000 };  // red end marker
const Colour playhead     { 0xFFFF3333 };  // red playhead
const Colour textPrimary  { 0xFFE0E0E0 };  // main text
const Colour textSecondary{ 0xFF999999 };  // dimmed text
const Colour accent       { 0xFF5599FF };  // selection, highlights
```

### Block Colors

Blocks have user-assignable colors. Default palette should be vibrant against dark background:
```
Red (#CC4444), Orange (#CC8844), Yellow (#CCAA44), Green (#44CC44),
Cyan (#44CCCC), Blue (#4488CC), Purple (#8844CC), Pink (#CC44AA)
```

Block tiles show: colored border (2px), name text centered, stack count badge (top-right if stacked).

### Context Menus

**Right-click Block:**
- Rename
- Set Color →
- Link to... → (then click target)
- Stack with... → (then click target, or drag onto)
- Set as Overlapping
- Mark as Done
- Delete Block

**Right-click Clip:**
- Rename
- Set Color →
- Set as Song Ender
- Mark as Done  
- Remove Clip

---

## Randomization Engine (`ArrangementResolver`)

### Resolution Algorithm

```
function resolve(project) → ResolvedArrangement:
    // 1. Copy block list
    blocks = deepCopy(project.blocks)
    
    // 2. Process links (swap)
    shuffledLinks = shuffle(project.links)
    for link in shuffledLinks:
        if random() < link.swapProbability:
            swap(blocks[link.blockA].position, blocks[link.blockB].position)
    
    // 3. Sort by position
    sort(blocks, by: position)
    
    // 4. Resolve stacks
    arrangement = []
    for group in groupByStackGroup(blocks):
        if group is single block (no stack):
            clip = weightedRandomPick(group[0].clips, by: probability)
            arrangement.append(clip)
        else:
            // Separate overlapping vs non-overlapping
            normal = group.filter(not isOverlapping)
            overlapping = group.filter(isOverlapping)
            
            // Pick how many normal blocks to play
            count = weightedRandomPick(group[0].stackPlayCount)
            picked = weightedRandomSample(normal, count)
            
            if group[0].stackPlayMode == Sequential:
                shuffle(picked)
                for block in picked:
                    clip = weightedRandomPick(block.clips)
                    // Check overlapping blocks
                    overlays = []
                    for ob in overlapping:
                        if random() < ob.overlapProbability:
                            overlays.append(weightedRandomPick(ob.clips))
                    arrangement.append(clip, overlays)
            else: // Simultaneous
                simultaneous = []
                for block in picked:
                    simultaneous.append(weightedRandomPick(block.clips))
                arrangement.append(simultaneous as layer)
    
    // 5. Handle song enders
    for i, entry in arrangement:
        if entry.clip.isSongEnder:
            arrangement = arrangement[0..i]  // truncate
            break
    
    // 6. Calculate timing (align end marks to next start marks)
    // 7. Apply tempo stretching for lead-in/tail where needed
    
    return arrangement
```

### Weighted Random Selection

```cpp
// In WeightedRandom.h
template<typename T>
T weightedPick(const Array<T>& items, const Array<float>& weights, Random& rng) {
    float totalWeight = 0.0f;
    for (auto w : weights) totalWeight += w;
    
    float roll = rng.nextFloat() * totalWeight;
    float cumulative = 0.0f;
    for (int i = 0; i < items.size(); ++i) {
        cumulative += weights[i];
        if (roll <= cumulative)
            return items[i];
    }
    return items.getLast(); // fallback
}
```

---

## Project Save/Load

**Format:** `.bsp` (BlockShuffler Project) — JSON file.

```json
{
  "version": 1,
  "name": "My Song",
  "sampleRate": 48000,
  "blocks": [
    {
      "id": "uuid-1",
      "name": "Intro",
      "color": "#CC4444",
      "position": 0,
      "stackGroup": -1,
      "stackPlayCount": { "values": [1], "weights": [1.0] },
      "stackPlayMode": "sequential",
      "isOverlapping": false,
      "overlapProbability": 0.0,
      "isDone": false,
      "tempo": 120.0,
      "clips": [
        {
          "id": "uuid-c1",
          "name": "Acoustic",
          "color": "#FFFFFF",
          "audioFile": "media/acoustic_intro.wav",
          "startMark": 0,
          "endMark": 529200,
          "probability": 0.7,
          "tempo": 170.0,
          "retainLeadInTempo": false,
          "retainTailTempo": false,
          "isSongEnder": false,
          "isDone": false,
          "gridOffsetSamples": 0
        }
      ]
    }
  ],
  "links": [
    { "blockA": "uuid-1", "blockB": "uuid-2", "swapProbability": 0.3 }
  ]
}
```

Audio files are stored relative to the project file in a `media/` subfolder. On save, new audio files are copied there.

---

## Implementation Phases

### Phase 1: Foundation (Milestone 1)
- [ ] CMake project setup with JUCE
- [ ] Data model classes (Project, Block, Clip, BlockLink)
- [ ] JSON serialization/deserialization
- [ ] Basic MainComponent with three-panel layout
- [ ] LookAndFeel_BlockShuffler (dark theme)
- [ ] Project save/load to `.bsp`

### Phase 2: Block Strip (Milestone 2)
- [ ] BlockStrip component with horizontal scrolling
- [ ] BlockComponent tiles with color borders, names
- [ ] Add block (+) button
- [ ] Drag to reorder blocks
- [ ] Right-click context menu (rename, color, delete)
- [ ] Block selection (click to select, highlight)

### Phase 3: Clip Management (Milestone 3)
- [ ] ClipListPanel showing clips in selected block
- [ ] Drag-in audio files from OS file explorer
- [ ] Browse button with FileChooser
- [ ] Audio loading via AudioFormatManager (WAV, AIFF, FLAC, OGG)
- [ ] Clip removal, rename, color

### Phase 4: Waveform Editor (Milestone 4)
- [ ] ClipWaveformView with AudioThumbnail
- [ ] Tempo-based grid overlay
- [ ] Draggable start/end markers with grid snapping
- [ ] Grid nudge (arrow keys)
- [ ] Tempo number box per clip
- [ ] Lead-in / tail visualization (dimmed regions before start, after end)

### Phase 5: Inspector Panel (Milestone 5)
- [ ] Probability sliders for clips
- [ ] Block probability / overlap probability
- [ ] Stack play count (with weighted value editor)
- [ ] Stack play mode toggle (sequential / simultaneous)
- [ ] Song ender checkbox
- [ ] Retain lead-in/tail tempo checkboxes
- [ ] Mark as done checkboxes

### Phase 6: Stacking & Linking (Milestone 6)
- [ ] Block stacking (drag block onto another, or right-click → Stack with)
- [ ] BlockStackView (vertical stack rendering)
- [ ] Stack play count weighted value editor
- [ ] Block linking (right-click → Link to)
- [ ] BlockLinkOverlay (visual arcs)
- [ ] Link swap probability editor

### Phase 7: Playback Engine (Milestone 7)
- [ ] ArrangementResolver (full resolution algorithm)
- [ ] WeightedRandom utility
- [ ] PlaybackEngine (AudioSource reading resolved arrangement)
- [ ] Crossfade/overlap handling for lead-in/tail
- [ ] Basic tempo stretching (ResamplingAudioSource for rate changes)
- [ ] Transport controls (Play, Stop, Rewind)
- [ ] Playhead visualization

### Phase 8: Export (Milestone 8)
- [ ] ExportRenderer (offline render)
- [ ] FLAC encoding
- [ ] `.bsf` package creation (ZIP with manifest + clips)
- [ ] Single-file FLAC/WAV export option
- [ ] Export progress dialog

### Phase 9: Polish (Milestone 9)
- [ ] Undo/Redo for all model mutations
- [ ] Keyboard shortcuts (Delete, Ctrl+Z, Ctrl+S, Space for play, etc.)
- [ ] Zoom in/out on waveform
- [ ] Scroll sync between waveform and block strip
- [ ] Tooltips
- [ ] Error handling (invalid audio files, missing files on project load)
- [ ] Overlapping block visual distinction (dashed border, overlay icon)

---

## Critical Implementation Notes

### Grid Snapping Math
```cpp
// GridSnap.h
inline int64 snapToGrid(int64 samplePos, double tempo, double sampleRate, int subdivision = 1) {
    double samplesPerBeat = (sampleRate * 60.0) / tempo;
    double gridUnit = samplesPerBeat / static_cast<double>(subdivision);
    return static_cast<int64>(std::round(static_cast<double>(samplePos) / gridUnit) * gridUnit);
}
```

### Tempo Stretching for Lead-In / Tail
When clip N's tail overlaps clip N+1's lead-in, and they have different tempos:
- By default, stretch the tail of clip N to match clip N+1's tempo
- By default, stretch the lead-in of clip N+1 to match clip N's tempo
- If `retainTailTempo` on clip N, skip stretching its tail
- If `retainLeadInTempo` on clip N+1, skip stretching its lead-in

For MVP, use `juce::ResamplingAudioSource` for simple rate-change stretching. For higher quality, integrate a WSOLA library or Rubber Band (MIT license).

### Overlap Alignment
When aligning blocks during playback:
```
Block A (Clip selected: "Verse_heavy.wav")
|----lead-in----|=====START==========END=====|---tail---|

Block B (Clip selected: "Chorus_acoustic.wav")  
                 |--lead-in--|=====START==========END=====|---tail---|

Alignment: Block A's END aligns with Block B's START
The overlap region = Block A's tail + Block B's lead-in (mixed/crossfaded)
```

### Audio Thread Safety
- The audio callback (`getNextAudioBlock`) must NEVER allocate memory, lock mutexes, or do I/O
- Use `std::atomic<ResolvedArrangement*>` for the current arrangement
- UI thread creates new arrangement, swaps pointer atomically
- Old arrangement is freed on UI thread after a safe delay (or use a lock-free GC queue)

### Mobile Export Format (.bsf) Compatibility
The `.bsf` format is a ZIP file (rename to .zip to inspect). The mobile player needs to:
1. Unzip to temp directory
2. Parse `manifest.json`
3. Load FLAC clips in order
4. Play sequentially per arrangement array, handling `startTime` offsets for simultaneous clips

Keep the manifest schema simple and stable — version it so future changes don't break old players.

---

## Testing Strategy

1. **Unit tests** for ArrangementResolver: verify weighted random distribution, link swaps, stack resolution
2. **Unit tests** for GridSnap: verify snapping math at various tempos/subdivisions
3. **Integration test**: load a project, resolve, verify arrangement is valid (no gaps, no overlaps where unintended)
4. **Audio test**: render a known arrangement, compare output buffer to expected (within float tolerance)
5. **Manual QA**: drag-and-drop, context menus, save/load round-trip

---

## Coding Standards

- JUCE naming conventions: `camelCase` for variables/methods, `PascalCase` for classes
- Use `juce::String` over `std::string` for UI-facing strings
- Use `juce::OwnedArray` for owned collections, `juce::Array` for value types
- Prefer `juce::ScopedPointer` / `std::unique_ptr` over raw pointers
- Header-only for small utilities (WeightedRandom, GridSnap)
- All UI components inherit from `juce::Component` and `juce::ChangeListener` where they need model updates
- Comment complex audio math and thread-safety boundaries

---

## Known Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Tempo stretching quality | Start with ResamplingAudioSource (simple pitch shift). Upgrade to Rubber Band later if quality insufficient |
| Audio thread starvation with many simultaneous clips | Pre-mix overlapping clips during resolution, not in real-time. Limit simultaneous voices to 8 |
| Large project files (many audio clips) | Store audio externally in media/ folder, reference by path. Consider memory-mapped files for large buffers |
| Complex undo/redo | Implement UndoableAction for every model method from Phase 1. Don't defer this |
| Mobile player format changes | Version the manifest.json schema. Never remove fields, only add |
