# BlockShuffler — Developer Handover Document

**Date:** 2026-03-19
**Build:** Release, macOS (arm64), JUCE 8.0.7, C++17
**Project file:** `Builds/MacOSX/BlockShuffler.xcodeproj` (Projucer-generated)

---

## 1. What the app does

BlockShuffler is a standalone desktop audio tool for probabilistic song arrangement. Artists split a song into named **blocks** (Intro, Verse, Chorus, …), load one or more audio clips into each block, set probability weights on those clips, and let the app randomise the order and content at play-time or export-time.

Key concepts:

| Concept | Description |
|---|---|
| **Block** | A horizontal slot in the arrangement strip. Has a name, colour, and a set of clips. |
| **Clip** | An audio file loaded into a block with a probability weight. Has start/end markers (the "body") and a lead-in/tail region outside those markers. |
| **Stack** | Two or more blocks sharing the same `stackGroup` value. They are rendered vertically in the strip and the resolver picks N of them to play per resolution. |
| **Link** | A swap-probability relationship between two blocks. At resolution time the resolver may swap their positions. |
| **Overlapping block** | A block flagged `isOverlapping`. It doesn't occupy its own timeline slot; instead it layers on top of whichever non-overlapping block plays at that position, with its own `overlapProbability`. |

---

## 2. Build instructions

### Prerequisites
- Xcode 15+ on macOS
- JUCE 8.x at `~/JUCE` (used as the JUCE submodule/install for this project)
- The Projucer project at `Builds/MacOSX/BlockShuffler.xcodeproj` is already generated — you do **not** need to re-run Projucer unless you add source files

### Debug build
```bash
xcodebuild -project Builds/MacOSX/BlockShuffler.xcodeproj \
           -scheme "BlockShuffler - Standalone Plugin" \
           -configuration Debug build
# App: Builds/MacOSX/build/Debug/BlockShuffler.app
```

### Release build
```bash
xcodebuild -project Builds/MacOSX/BlockShuffler.xcodeproj \
           -scheme "BlockShuffler - Standalone Plugin" \
           -configuration Release build
# App: Builds/MacOSX/build/Release/BlockShuffler.app
```

### Adding new source files
New `.h/.cpp` files must be added through the **Projucer** (`BlockShuffler.jucer`) to be compiled. Open the `.jucer` file in Projucer, add the file, and re-export the Xcode project. Alternatively, include new headers from existing source files and put implementation in a file that is already in the project (acceptable for small additions).

---

## 3. Architecture overview

```
Source/
├── Main.cpp                    JUCEApplication entry point
├── PluginProcessor.h/.cpp      AudioProcessor + BlockShufflerEditor (hosts MainComponent, owns timer)
├── MainComponent.h/.cpp        Top-level Component: layout, file I/O, key handling, playback control
│
├── Model/
│   ├── Project.h/.cpp          Root data model; owns blocks + links; snapshot-based undo
│   ├── Block.h/.cpp            Block data + ChangeBroadcaster
│   ├── Clip.h/.cpp             Clip data + audio buffer loading
│   ├── BlockLink.h/.cpp        Swap-link between two blocks
│   └── Serialization.h/.cpp    JSON save/load (.bsp) and export manifest helpers
│
├── Audio/
│   ├── ArrangementResolver.h/.cpp   Resolves probabilities → concrete ResolvedArrangement
│   ├── PlaybackEngine.h/.cpp        AudioSource: mixes ResolvedArrangement in real time
│   ├── ExportRenderer.h/.cpp        Offline render: WAV/FLAC flat file or .bsf ZIP bundle
│   └── TempoStretcher.h/.cpp        STUB — not yet implemented (see §8)
│
├── UI/
│   ├── BlockStrip.h/.cpp            Horizontal scrollable block strip, slot-based stacked layout
│   ├── BlockComponent.h/.cpp        Single block tile (drag, context menu, playing indicator)
│   ├── BlockLinkOverlay.h/.cpp      Curved arc overlay showing links between blocks
│   ├── ClipWaveformView.h/.cpp      Scrollable list of clip rows with waveforms and markers
│   ├── InspectorPanel.h/.cpp        Right-side properties panel (all per-clip and per-block controls)
│   ├── TransportBar.h/.cpp          Play/Stop/Rewind/Export/Save/Open bar
│   ├── LookAndFeel_BlockShuffler.h/.cpp  Dark theme, colour constants, block palette
│   ├── ClipListPanel.h/.cpp         (Currently unused — clip list is embedded in ClipWaveformView)
│   └── BlockStackView.h/.cpp        (Currently unused — stack rendering is done inside BlockStrip)
│
└── Utils/
    ├── GridSnap.h              snapToGrid(), gridUnitSamples(), nudgeByGridUnits() — header-only
    ├── WeightedRandom.h        WeightedValue<T> template with pick() and sample() — header-only
    └── UuidGenerator.h         generateUuid() — header-only
```

### Data flow

```
User action
    │
    ▼
Model mutation (Project / Block / Clip)
    │  fires sendChangeMessage()
    ▼
MainComponent::changeListenerCallback()
    │  re-validates selectedBlock by ID
    │  calls refreshValues() or setBlock() on InspectorPanel
    ▼
UI re-renders
```

### Playback flow

```
[Space / Play button]
    │
    ▼
ArrangementResolver::resolve(project, rng)
    → produces ResolvedArrangement (entries with timelinePos + blockId)
    │
    ▼
PlaybackEngine::play(arrangement)  [copy stored in MainComponent::currentArrangement]
    │
    ▼  audio thread, every block:
PlaybackEngine::getNextAudioBlock()
    → mixEntryIntoBuffer() per entry (lead-in fade-in + body + tail fade-out)
    │
    ▼  message thread, 10 Hz timer:
MainComponent::updateTimeDisplay()
    → finds currently-playing block by walking currentArrangement entries
    → BlockStrip::setPlayingBlock(blockId) → green top bar on playing tile
```

---

## 4. Key implementation details

### 4.1 Snapshot-based undo/redo (`Project.cpp`)

Every model mutation takes a pre-snapshot, performs the change, then calls `recordMutation(preSnapshot)` which registers a `SnapshotAction` with `undoManager`. Undo/redo restores the full JSON snapshot via `resetAndLoad()`. A `suppressUndo` flag prevents recursive recording during restore.

**To add a new undoable mutation:** follow the pattern in `Project::addBlock()` — capture `toJSON()` before the change, do the change, call `recordMutation(pre)`.

### 4.2 Block strip slot-based layout (`BlockStrip::resized()`)

Blocks with the same `stackGroup` are grouped into a **slot**. Each slot occupies one horizontal position; blocks within the slot are divided vertically. `blockCentreXCache` (parallel array to `project->blocks`) stores the contentArea-relative X centre for each block, used by the link overlay and scroll-to-selected.

**Critical guard:** `resized()` starts with `if (blockComponents.size() != project->blocks.size()) rebuildBlocks()` — this prevents an `EXC_BAD_ACCESS` when the window lays out before the async rebuild fires.

### 4.3 Lead-in / tail crossfade (`PlaybackEngine::mixEntryIntoBuffer()`)

Each `ResolvedEntry` has its body at `[timelinePos, timelinePos + bodyLen)`. The engine also mixes:
- **Lead-in:** clip samples `[0, startMark)` mapped to `[timelinePos - startMark, timelinePos)` with gain ramping 0 → 1
- **Tail:** clip samples `[endMark, srcLen)` mapped to `[bodyEnd, bodyEnd + tailLen)` with gain ramping 1 → 0

Adjacent clips' tails and lead-ins naturally overlap and crossfade because they are both mixed into the output buffer at the same timeline positions.

`addFromWithRamp()` is used for sample-accurate per-block gain interpolation.

### 4.4 Grid snapping (`ClipWaveformView.cpp` + `GridSnap.h`)

Marker drag calls `snapToGrid(rawSample, clip.tempo, clip.nativeSampleRate)` from `GridSnap.h`. Hold **Shift** to bypass snap for fine positioning. Arrow-key nudge uses `nudgeByGridUnits()`.

### 4.5 Inspector link slider decoupling

Link probability sliders update `link->swapProbability` directly in `sliderValueChanged` (no `sendChangeMessage`). Undo is recorded once, asynchronously, in `sliderDragEnded` via `MessageManager::callAsync + SafePointer`. This prevents the HALC CoreAudio rate-limit flood that was triggered when undo was recorded on every drag sample.

### 4.6 `.bsf` bundle export (`ExportRenderer::renderToBsf()`)

Produces a ZIP file containing:
- `manifest.json` — clip catalogue + arrangement sequence
- `clips/clip_001.flac`, `clip_002.flac`, … — per-clip body audio (FLAC, lossless, store-level compression since FLAC is already compressed)

Workflow: deduplicate clips → render each body to a temp FLAC → build manifest JSON → `juce::ZipFile::Builder` → clean up temp dir.

### 4.7 Export progress dialog (`MainComponent.cpp`)

`ExportJob` extends `juce::ThreadWithProgressWindow`. Because JUCE 8 disables modal loops by default, `runThread()` is not available — use `launchThread()` instead. The job self-deletes in `threadComplete(cancelled)`. Progress is reported via `setProgress(double)` which updates the dialog bar from the background thread safely.

---

## 5. Known limitations / not yet implemented

| Feature | Location | Notes |
|---|---|---|
| **Tempo stretching** | `TempoStretcher.h/.cpp` | Stub only. `retainLeadInTempo` / `retainTailTempo` flags are stored and serialised but have no effect at playback. See §8 for implementation guide. |
| **Clip probability undo** | `InspectorPanel::sliderValueChanged` | Slider drag directly mutates `clip->probability`. A drag-end undo hook (matching the link slider pattern) has not been added. |
| **Block overlap undo** | `InspectorPanel::sliderValueChanged` | Same gap — `overlapSlider` mutates `block->overlapProbability` directly. |
| **Stack count undo** | `InspectorPanel` stack count rows | Inline count +/- and weight changes mutate the model directly without recording undo. |
| **Context menu mutations undo** | `BlockComponent::showContextMenu`, `ClipRowComponent::showContextMenu` | Right-click actions (color, done, overlapping, etc.) mutate model directly. |
| **Zoom state persistence** | `ClipWaveformView::zoomFactor` | Zoom resets to 1× when the selected block changes. |
| **Missing file recovery** | `Serialization::projectFromJSON` | If an audio file is missing on load, the clip is silently created with an empty buffer. No warning is shown. |
| **BSF mobile player** | — | The companion mobile player app is not part of this repo. The `.bsf` format spec is in `CLAUDE.md §10`. |

---

## 6. Data formats

### `.bsp` — Project file (JSON)

Save/load via `Project::saveToFile()` / `Project::loadFromFile()`. Audio paths are stored as absolute paths. When sharing projects between machines, audio files must be relocated manually (no embedded media in `.bsp`).

### `.bsf` — Export bundle (ZIP)

```
song.bsf (ZIP, rename to .zip to inspect)
├── manifest.json
└── clips/
    ├── clip_001.flac
    ├── clip_002.flac
    └── ...
```

`manifest.json` schema:
```json
{
  "version": 1,
  "title": "My Song",
  "sampleRate": 48000,
  "bitDepth": 24,
  "totalDurationSamples": "10584000",
  "clips": [
    { "id": "clip_001", "file": "clips/clip_001.flac", "startSample": 0, "durationSamples": "529200" }
  ],
  "arrangement": [
    { "clipId": "clip_001", "startTime": "0" },
    { "clipId": "clip_002", "startTime": "529200" }
  ]
}
```

Large integers (`totalDurationSamples`, `durationSamples`, `startTime`) are stored as strings to avoid double precision loss in JSON parsers.

---

## 7. Threading model

| Thread | Responsibilities | Constraints |
|---|---|---|
| **Message thread** | All UI, model mutations, undo/redo, file I/O | Standard JUCE message loop |
| **Audio thread** | `PlaybackEngine::getNextAudioBlock()` | No allocation, no locks except `CriticalSection`, no I/O |
| **Export thread** | `ExportJob::run()` — offline render | May allocate; accesses only its own copy of `ResolvedArrangement` |

`PlaybackEngine` guards its `arrangement` and `playheadSamples` with a `juce::CriticalSection`. The audio thread holds the lock briefly per block. `playheadSamples` is also `std::atomic<int64_t>` for lock-free reads from the message thread (used in `updateTimeDisplay`).

**Never** access `Project`, `Block`, or `Clip` from the audio thread — the engine receives a copy of `ResolvedArrangement` which contains only raw `Clip*` pointers to `audioBuffer` data that is never mutated after load.

---

## 8. Implementing tempo stretching (next steps)

This is the main remaining audio feature. `retainLeadInTempo` / `retainTailTempo` per clip are already stored, serialised, and displayed in the inspector — they just have no effect.

### Recommended approach (quality order)

1. **Rubber Band Library** (MIT) — best quality, time-stretch without pitch shift. Add as a submodule and integrate into `TempoStretcher`.
2. **JUCE `ResamplingAudioSource`** — simple rate-change (changes pitch, not true time-stretch). Good enough for rough drafts.

### Design

`TempoStretcher` should expose a static method:
```cpp
// Stretch srcBuffer[srcStart..srcEnd) to match targetLengthSamples.
// Returns a new AudioBuffer containing the stretched audio.
static juce::AudioBuffer<float> stretch(
    const juce::AudioBuffer<float>& src,
    int64_t srcStart, int64_t srcEnd,
    int64_t targetLengthSamples,
    double nativeSampleRate);
```

### Where to call it

In `PlaybackEngine::mixEntryIntoBuffer()`, when mixing the lead-in or tail:

```cpp
// Lead-in: if clip N's lead-in tempo differs from clip N-1's body tempo,
// stretch the lead-in samples to fit the gap.
// (Requires passing adjacent-entry tempo info into mixEntryIntoBuffer,
//  which currently only receives the single entry.)
```

The simplest integration point is in `ArrangementResolver::resolve()` — pre-stretch the lead-in/tail into new `AudioBuffer`s and store them in the `ResolvedEntry`. This keeps the audio thread allocation-free (buffers pre-computed at resolve time).

---

## 9. Keyboard shortcuts

| Key | Action |
|---|---|
| `Space` | Play / Pause |
| `Cmd+Z` | Undo |
| `Cmd+Shift+Z` / `Cmd+Y` | Redo |
| `Cmd+S` | Save project |
| `Cmd+O` | Open project |
| `Cmd+N` | New project |
| `Esc` | Cancel pending link/stack mode |
| `Delete` / `Backspace` | Remove selected clip (when waveform view has focus) |
| `← →` | Nudge selected clip by ±1 grid unit |
| `Cmd + scroll` | Zoom waveform in/out |
| `Shift + drag marker` | Free-drag marker (bypass grid snap) |

---

## 10. UI colour constants (`LookAndFeel_BlockShuffler.h`)

```cpp
bgDark         = 0xFF1E1E1E   // main window background
bgMedium       = 0xFF2D2D2D   // panel backgrounds
bgLight        = 0xFF3C3C3C   // waveform background, text box backgrounds
waveformFill   = 0xFFCCCCCC   // waveform drawn lines
gridLineColor  = 0xFF4A4A4A   // tempo grid lines
startMarkerCol = 0xFF00CC00   // green start marker / playing block indicator
endMarkerCol   = 0xFFCC0000   // red end marker
playheadCol    = 0xFFFF3333   // red playhead
textPrimary    = 0xFFE0E0E0   // main text
textSecondary  = 0xFF999999   // dimmed/secondary text
accentCol      = 0xFF5599FF   // selection highlight, link overlay arcs
```

---

## 11. Gotchas

- **Do not call `sendChangeMessage()` inside a drag callback** that fires at audio rate. The link slider implementation learned this the hard way (caused HALC CoreAudio rate-limit floods). Always defer undo/change notifications to `dragEnded` or similar.

- **`resized()` is called before `changeListenerCallback()`** on first layout. Always guard `resized()` with a sync check when it indexes into arrays that are rebuilt asynchronously.

- **`juce::ThreadWithProgressWindow::runThread()` requires `JUCE_MODAL_LOOPS_PERMITTED`** which is off by default in JUCE 8. Use `launchThread()` + `threadComplete()` callback instead.

- **Stack badge uses `stackGroup + 1`** (1-based display) but `stackGroup` is 0-based internally.

- **Project undo uses `suppressUndo` flag** — any `Project` method called during `resetAndLoad()` must respect this flag, otherwise undo of an undo will record a new undo action.

- **`WeightedValue<int>::isValid()`** returns false if `values` and `weights` are different sizes or empty. Always check before calling `pick()` or `sample()`.
