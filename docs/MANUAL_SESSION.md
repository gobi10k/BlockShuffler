# BlockShuffler — MANUAL_SESSION.md (code-verified GUI checklist)

**Rev 2026-07-07.** This is the single, current manual-acceptance script. It contains
**only** the items that genuinely need a human at the GUI. Everything in the
PENDING-MANUAL queue that could be checked from buffers / layout math / model state
was automated in the regression harness (`diag/ResolverDiag.cpp`, tests **T21–T29**,
`STEP6 RESULT: ALL PASS`) and is **not** repeated here.

Coverage of the 2026-07-06 PENDING-MANUAL queue (2.9, 6.1, 6.3, 6.4, 7.6, 7.7, 10.2,
10.4, 12.1, 13.2, 13.3, 13.4):

| Item | Now covered by | Manual here? |
|---|---|---|
| 6.1 clip Done still plays | **T21** (headless) | no |
| 6.3 DONE badge vs name | **T29** (headless, geometric) | optional glance only |
| 6.4 export all-Done | **T22** (headless) | no |
| 7.7 Play Clip lead-in | **T23** (headless) | no |
| 10.2 export == playback | **T24** (headless) | no |
| 10.4 stretched-join export == playback | **T25** (headless) | no |
| 13.2 nine-block stack visible | **T26** (headless, layout math) | optional glance only |
| 13.3 grid ≥ 8px on long clips | **T27** (headless, grid math) | optional glance only |
| 13.4 zoom scales with length | **T28** (headless, zoom math) | optional glance only |
| **2.9 clip drag — no scroll reset** | code-inspection | **YES — Step 2** |
| **7.6 rapid play/stop ×20** | (real audio thread) | **YES — Step 3** |
| **12.1 50-block stress + rapid undo** | (interactive drag/undo) | **YES — Step 1** |
| **13.1 colour renders true** | fixed session 8; T29 geometry only | **YES — Step 4 (visual sign-off)** |

**Remaining human line-items: 2.9, 7.6, 12.1, 13.1.** Run top to bottom — steps are
ordered to reuse each loaded project (no reload between grouped steps).

## Apps & material
- **Debug app (run from Terminal to see assert logs):**
  `"build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app/Contents/MacOS/BlockShuffler"`
- **Release app:** `build-release/BlockShuffler_artefacts/Release/Standalone/BlockShuffler.app`
- **Projects:** regenerate with
  `./build-diag/ResolverDiag_artefacts/Debug/ResolverDiag --gen-manual ~/Desktop/BlockShuffler_ManualRound`
  → `TestProject.bsp` (4 blocks, deterministic) and `StressProject.bsp` (50 blocks, 5 stacked pairs).
- **KNOWN dev-noise:** `juce_Colour.cpp:340` assertion lines during playback are the
  deferred TODO 7 — ignore **that line only**; any **other** assertion is a finding.

---

## GROUP A — StressProject.bsp loaded (Steps 1–3, no reload between them)

Launch the **Debug** app from Terminal, then Open `~/Desktop/BlockShuffler_ManualRound/StressProject.bsp`.

### Step 1 — 12.1 stress: 50 blocks, 15 rapid undos, drag-heavy, no crash
**Precondition:** StressProject.bsp just opened.
**Steps:**
1. On load, confirm **all 50 blocks are visible/reachable** in the strip (scroll right to the end; the 5 stacked pairs render stacked). - yes
2. Make **15 quick edits** (weight-slider drags, a rename or two, a couple of reorders), then press **Cmd+Z ×15** rapidly. - yes 
3. Do ~1 minute of **drag chaos**: reorder blocks, drag blocks into/out of the 5 stacks, **Shift+drag** a whole stack. - crash
4. **Play ~30s**, then **rapid Play/Stop ×5**. - pass

**Expected observable:** every undo reverts one step (no blank waveform/strip); no crash,
no non-`Colour` assertion in the Terminal, no block vanishing, no strip corruption, no stuck audio.
**Invariant guarded:** stability/parity under a 50-block project.
**Code proof —** StressProject is generated with exactly 50 blocks + 5 stacked pairs:
```
diag/ResolverDiag.cpp:184   for (int i = 1; i <= 50; ++i) {
diag/ResolverDiag.cpp:185       auto* b = p.addBlock("Block " + juce::String(i));
diag/ResolverDiag.cpp:191   for (int i = 10; i <= 50; i += 10)
diag/ResolverDiag.cpp:193       p.stackBlocks(...)  // → StressProject.bsp
```
**PASS / FAIL:** ______

### Step 1b — 12.1 UAF re-gate under AddressSanitizer (the real acceptance test)
The stack-drag crash was a use-after-free the headless suite structurally cannot catch
(T31 passes while the app crashed). Validate the fix on an **ASan Standalone**.

**Build (already built at `build-asan/…`; rebuild if needed):**
```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
  -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build-asan --target BlockShuffler_Standalone
```
**Run (inner binary, from Terminal, so ASan prints to stderr):**
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 \
  "build-asan/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app/Contents/MacOS/BlockShuffler"
```
**Steps:** Open `~/Desktop/BlockShuffler_ManualRound/StressProject.bsp`, then for ~1 min:
drag blocks **OUT of** the 5 stacks, **Shift+drag** whole stacks, and do rapid **reorders**.
**Expected (PASS):** no `AddressSanitizer` report, no `malloc: pointer being freed…` abort,
and **no burst/flood of `juce_Colour.cpp:340`** tied to the drags. Dropped blocks/stacks land
in the correct final position (deferred rebuild).
**KNOWN separate noise:** a *sporadic* `juce_Colour.cpp:340` still comes from the waveform grid
alpha (`ClipWaveformView.cpp:187`, `jmap` unclamped — TODO-7), independent of the drag. That is
NOT the UAF and NOT a flood; ignore isolated `:340` lines from waveform painting.
**PASS / FAIL:** ______

### Step 1c — stack drop-out-onto-block BEHAVIOUR (headless-green T32–T34; confirm by eye)
On the SAME ASan run, verify the stack-classification fix (a block dragged out of a stack and
dropped onto a different block must NOT pull a third block in):
1. In a stack of 2 (e.g. one of StressProject's pairs, or build S={A,B}), drag **A** out and drop
   it **onto a standalone block C**. EXPECT: a fresh 2-block stack **{A,C}**; **B becomes standalone**
   (last-remaining auto-unstacks); C is **not** swallowed into the old stack; no third tile appears.
   POSITION (user ruling 2026-07-10, headless: T36): the {A,C} stack sits at **C's slot** (the drop
   location); B stays at the old stack's slot; nothing swaps back to A's old index.
2. With two stacks S={A,B} and S2={C,D}, drag **A** onto **C**. EXPECT: **A joins S2** (so S2={A,C,D});
   **B standalone**; **D stays** in S2; no block lost. POSITION: S2 stays at its own slot.
3. Drag a **non-stacked** block onto a **stacked** one. EXPECT: it **joins** that stack, and the
   stack stays anchored at ITS slot (all drops now route through `restackBlockOnto`; the old
   standalone→`stackBlocks` dispatch anchored the stack at the DRAGGED block's old slot whenever
   the target was to the RIGHT — the 2026-07-13 in-app finding; headless: T36 case 3).
4. DIRECTION CHECK (the bug T36 used to miss): with standalone blocks, drop **X onto a block to
   its RIGHT**, then (fresh pair) **onto a block to its LEFT**. EXPECT both: the new stack sits at
   the TARGET's slot; the target never appears to jump back to X's old slot ("swap" illusion).
5. RIGHT-CLICK path: right-click X → **Stack with...** → click Y. EXPECT: same as dropping X onto
   Y — merged stack anchored at **Y's slot** (same `restackBlockOnto` routing since 2026-07-13).
6. `Cmd+Z` after each: **one** undo restores the exact prior stack membership.
**Expected (PASS):** every case matches; no block absorbed/dropped; single-step undo.
**PASS / FAIL:** ______

### Step 1d — rapid undo ×15 re-check (regression found + narrowed 2026-07-08)
Routing ALL strip rebuilds through the coalescing AsyncUpdater collapsed per-step undo
refresh (stale strip across a multi-undo window). Fixed by narrowing: undo/model changes
refresh per change via `changeListenerCallback` (`MessageManager::callAsync`, the original
wiring); the AsyncUpdater now serves ONLY the drop path (headless guard: **T35**, per-step
model exactness ×15 + redo ×15).
1. On the SAME ASan run, make **15 quick edits** (weight drags, renames, reorders, a stack
   drag or two), then press **Cmd+Z ×15 rapidly**.
**Expected (PASS):** each undo visibly reverts one step as it happens (strip/waveform track
per press, no frozen or blank strip until the end); final state = original; no ASan report.
**PASS / FAIL:** ______

### Step 2 — 2.9 clip drag between blocks does NOT scroll the strip back to start
**Precondition:** StressProject.bsp open; the strip scrolled well to the **right** so block 1 is off-screen.
**Steps:**
1. Scroll the block strip to the right until a far block (e.g. Block 40+) is visible and block 1 is out of view.
2. Select a visible block that has a clip; drag one of its **clips** onto a **different later block** in view.
3. Release the drop.

**Expected observable:** the clip **moves** to the target block, and the strip **stays scrolled
where it was** — it does **not** jump back to block 1 / x=0.
**Invariant guarded — 2.9 scroll/visibility.** Quote (the rebuild path saves and restores the
horizontal scroll after a model change so a far-right drop doesn't snap the view to x=0):
> "Preserve the horizontal scroll position across rebuilds so that dropping a clip onto a
> far-right block doesn't snap the view back to x=0."
**Code proof —** BlockStrip.cpp:
```
226   // Preserve the horizontal scroll position across rebuilds so that
227   // dropping a clip onto a far-right block doesn't snap the view back to x=0.
228   int savedScrollX = safe->viewport.getViewPositionX();
229   safe->needsRebuildAfterDrag = false;
230   safe->rebuildBlocks();
231   safe->resized();
232   safe->repaint();
233   if (savedScrollX > 0)
234       safe->viewport.setViewPosition(savedScrollX, 0);
```
**FAILURE looks like:** the strip snaps back to the start after the drop.
**PASS / FAIL:** ______

### Step 3 — 7.6 rapid Play/Stop ×20, no crash, no stuck audio
**Precondition:** StressProject.bsp open (or any loaded project with audio).
**Steps:**
1. Click **Play**, then **Stop**, as fast as possible, **20 times**, ending on **Stop**.

**Expected observable:** every press lands; audio always stops on Stop (no drone/stuck tone);
the playhead resets; the Terminal shows **no new** assertions (the `juce_Colour.cpp:340` spam excepted).
**Invariant guarded:** transport robustness — Stop always halts the engine.
**Code proof —** MainComponent toggles the engine, and `stop()` unconditionally clears the playing flag:
```
Source/MainComponent.cpp:415  void MainComponent::onPlayPressed() {
Source/MainComponent.cpp:416      if (engine.isPlaying()) { engine.stop(); ... }
Source/MainComponent.cpp:422      else { currentArrangement = resolver.resolve(*project, rng); engine.play(...); }

Source/Audio/PlaybackEngine.cpp:27  void PlaybackEngine::stop() {
Source/Audio/PlaybackEngine.cpp:28      playing.store(false);
```
**FAILURE looks like:** crash, drone after Stop, UI freeze, or a non-`Colour` assertion line.
**PASS / FAIL:** ______

---

## GROUP B — colour sign-off (Step 4)

Open `TestProject.bsp` (4 blocks) — or add 3 fresh blocks in a new project.

### Step 4 — 13.1 clip/block colour renders true (no blue tint; yellow is yellow)
**Precondition:** a project with at least 3 blocks visible in the strip.
**Steps:**
1. Right-click a block → **Set Color** → **Yellow**.
2. Right-click a second block → **Set Color** → **Green**.
3. Right-click a third block → **Set Color** → **Blue** (and, if a fourth exists, **Pink**).

**Expected observable:** each block body reads as its **true** hue — **yellow looks yellow**
(not muddy green), green is green, blue is blue, pink is pink; there is **no overall blue wash**
across the strip. The block **name stays legible** on every colour (dark text on light bodies,
light text on dark bodies).
**Invariant guarded — 13.1 colour.** Quote (session-8 fix: the block body is now the identity
hue rendered dominant over a **neutral** grey base, not a 10% wash over the blue theme base):
> "colour over the cool blue-grey theme base desaturated and hue-shifted every colour
> (bug 13.1: yellow read as green). Text colour follows the body luminance … so the name
> stays legible."
**Code proof —** BlockComponent.cpp (body = neutral grey interpolated 0.85 toward the block colour;
name colour by luminance):
```
78   static constexpr juce::uint32 neutralBodyBase = 0xFF3A3A3A;  // neutral grey, no hue
79   const juce::Colour bodyCol =
80       juce::Colour(neutralBodyBase).interpolatedWith(block->color, 0.85f);
84   const juce::Colour bodyTextCol = (bodyLum > 0.55f) ? juce::Colours::black
85                                                      : juce::Colours::white;
```
**FAILURE looks like:** yellow reads as green, an overall blue tint, or the name obscured/illegible on some colour.
**PASS / FAIL:** ______

---

## OPTIONAL — visual glance for the headless-covered UI items (non-blocking)
The app is already open, so a quick eyeball is cheap. These are **already proven** by the
harness; record only if something contradicts the test.
- **6.3 (T29):** mark a block **Done** — the DONE badge sits in the bottom-right and does not
  cover the block name.
- **13.2 (T26):** build a stack of **9** blocks — all 9 tiles are visible (they fit at the 360px
  strip height) or reachable via the strip's vertical scrollbar.
- **13.3 (T27):** on a **long** clip, grid lines are spaced ≥ 8px and do not obscure the waveform.
- **13.4 (T28):** compare a **short** vs **long** clip — max zoom reveals ~0.5s of fine detail on both.
**Glance PASS / (flag): ______**

---

## GROUP C — Windows-only follow-ups (12.2 / 12.3, require a Windows machine)
The macOS/Linux side is verified in-session (paths T30, open-on-launch handler, icon build).
These remaining checks need an actual **Windows** build + Explorer and cannot be run on macOS.
The MSVC build itself is validated by GitHub Actions (`.github/workflows/build-windows.yml`,
now triggered on `UI_firstdraft`) — check that run is green first.

### Step 5 — 12.3 app icon on Windows
1. Build/download the Windows `BlockShuffler.exe` (CI artifact "BlockShuffler-Standalone-Windows").
2. In Explorer, view the `.exe` at large-icon size.
**Expected:** the RiverMix owl icon (blue/cyan on black) shows on the `.exe` — JUCE embeds it
from `Resources/appicon.png` via `ICON_BIG`/`ICON_SMALL`; no distortion.
**PASS / FAIL:** ______

### Step 6 — 12.2 cross-platform .bsp round-trip
1. Copy a `.bsp` **saved on macOS** (e.g. `TestProject.bsp` + its `media/` folder) to Windows.
2. Open it in the Windows build via the in-app **Open** button.
**Expected:** all clips load (no "Missing Audio Files" warning) — forward-slash relative paths
resolve on Windows. Also save a project on Windows, copy back to macOS, open → clips resolve
(backslash paths normalise; proven headless by **T30**).
**PASS / FAIL:** ______

### Step 7 — 12.2 open-on-launch + double-click association (Windows)
1. From a terminal: `BlockShuffler.exe "C:\path\to\TestProject.bsp"` → the project opens on launch
   (argv path → canonical `loadProject`).
2. Register the `.bsp` association (installer, or the `.reg` in `docs/WINDOWS_PACKAGING.md`), then
   double-click a `.bsp` in Explorer.
**Expected:** double-click opens the project in BlockShuffler (a fresh instance loads it).
**Note:** no installer is built yet — until one runs the registry writes in
`docs/WINDOWS_PACKAGING.md`, only the command-line/argv path (step 7.1) works.
**PASS / FAIL:** ______

### Step 8 — 12.3 in-app logo sizing (macOS or Windows, cosmetic)
ACCEPTANCE 12.3 asks the transport-bar logo be "~2/3 bar height, just left of Save As". The
asset (RiverMix wordmark, `BinaryData::icon_png`) and position are correct; confirm the **size**
reads right (currently fills the transport content-area height). Flag if it looks too tall/short.
**PASS / FLAG:** ______

---

## Report back
Fill each **PASS / FAIL** above. Any FAIL → note the item, what you saw, and (if Debug) the
Terminal line.
- **macOS/Linux GUI-only (run locally):** 2.9, 7.6, 12.1, 13.1, + 12.3 logo-size eyeball (Step 8).
- **Windows-only (need a Windows box):** 12.3 exe icon (Step 5), 12.2 .bsp round-trip (Step 6),
  12.2 open-on-launch + double-click assoc (Step 7).
