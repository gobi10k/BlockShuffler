# PROGRESS.md — BlockShuffler running log

**This is the cross-session memory. READ THIS FIRST every session. UPDATE IT LAST every session.**
Claude Code has no memory between sessions except this file. If it isn't written here, the next session doesn't know it.

Format: newest entry at the top. Each session appends a dated block. Keep the "CURRENT STATUS" and "NEXT UP" sections at the very top always accurate.

---

## CURRENT STATUS (keep this current)
- Stage: FINAL DELIVERY PUSH against locked spec. Client deadline ~2026-07-17.
- Build: compiles; macOS arm64 primary. Windows build exists, path handling made cross-platform (relative, forward slashes) — parity not yet re-verified.
- Last full audit: 0 critical / 0 high / 0 medium after fixes.

### BROKEN / TODO (in priority order)
1. ~~**Stack play count regression**~~ — **CLOSED 2026-07-05: does not reproduce.** Verified honored in current code (headless harness + in-app DBG + by ear). PROGRESS entry below has the proof. Keep the jassert guard at ArrangementResolver.cpp:319.
2. ~~**Simultaneous layering**~~ — **CLOSED 2026-07-05 session 2: MASTER_PROMPT Step 2 COMPLETE.** Grep half PASSED; runtime half PASSED (SIM play 3 → 10/10 identical timelinePos, cursor advances by LONGEST body; proof in entry below); layering confirmed BY EAR by the user. No code change was needed.
3. ~~**NEW FEATURE: "Always play base block"**~~ — **CLOSED 2026-07-06: MASTER_PROMPT Step 3 COMPLETE.** 3A–3E committed with proof (session-3 entry below); 3F user-confirmed 2026-07-06: all six manual tests pass (ear test base ON play 2 of 3, toggle visibility per mode, undo/redo, save→reopen persistence, pre-3B project loads with toggle off).
4. ~~**Effective % = inclusion probability**~~ — **CLOSED 2026-07-06: MASTER_PROMPT Step 4 COMPLETE (user-confirmed incl. amendment).** Shared StackPicker between resolver and display (4A pure extraction, 4B MC via same pick(), 4C fingerprint-gated recompute triggers — no recompute on Play). Amendment: deterministic seeding from stack-state hash (identical state → bit-identical numbers across Plays/recomputes/save-reopen); trials at 20000 (amendment's 50000 measured 130ms Debug on a 6-block stack, over the 50ms budget → dropped per its own fallback; 20000 ≈ 52ms). ACCEPTED KNOWN LIMITATION: equal weights may display 33/34/34 (true 33.33 is 0.17 from the rounding boundary vs ~0.33 MC standard error) — within client tolerance of ±2; exact-enumeration fix scoped but DEFERRED.
5. **Logo polish** — background must match transport bar colour exactly, larger, sit just left of Save As. Verify logo + app icon load via BinaryData (cross-platform), not runtime file paths. ACCEPTANCE_TESTS 12.3.
6. **Windows parity pass** — ACCEPTANCE_TESTS 12.2.
7. **NEW (found 2026-07-05): juce_Colour.cpp:340 assertion spam** — fires continuously while painting during playback with audio loaded (Debug builds). Something constructs a Colour with an out-of-range float value. Find and fix (likely waveform/playhead/indicator paint code; maybe touch alongside item 5).
8. ~~**Source/ is UNTRACKED in git**~~ — **CLOSED 2026-07-05 session 2.** Full tree committed on `UI_firstdraft` and tagged `baseline-step1-clean`. IMPORTANT: the old "repo" was accidentally rooted at `$HOME` (that's why Source/ looked untracked); a proper repo now lives at the project directory with the UI_firstdraft history imported and origin set (details in entry below). One commit per completed MASTER_PROMPT step from now on.

### NEXT UP
MASTER_PROMPT Step 5 (permanent guards) + Step 6 harness half, then user runs the Step 6 manual checklist (in the 2026-07-06 entry below). Step 7 (cleanup: DBG removal, diag/ teardown, clean rebuild both configs) starts ONLY after the user confirms that checklist. Then remaining ACCEPTANCE_TESTS groups, logo (item 5), Windows parity (item 6), Colour assertion (item 7).

---

## POST-DELIVERY ENHANCEMENTS (accepted rulings — do NOT build before delivery)
- **Dirty-state tracking + unsaved-changes prompt on open/new/quit** (ruled 2026-07-06): today NO path prompts on unsaved changes except New Project's blanket confirm; Open button and Finder double-click both load without prompting (identical by design, user-accepted). Proper fix post-delivery: dirty flag on model mutations + prompt on Open/double-click/New/Quit.

## STANDING RULE (Step 7B): REGRESSION HARNESS
`diag/ResolverDiag.cpp` + the `ResolverDiag` CMake target are PERMANENT — they encode every invariant that historically regressed (playCount, layering, links, isDone, lead-in timeline, save/load, undo). The target is EXCLUDE_FROM_ALL, so client/Release builds never touch it. **Run the FULL harness before ANY future change to resolver, stack, or link code:**
`cmake --build build-diag --target ResolverDiag && ./build-diag/ResolverDiag_artefacts/Debug/ResolverDiag 2>/dev/null | grep -E "SUMMARY|STEP6 RESULT|FAIL"` — everything must stay green (STEP6 RESULT: ALL PASS).

## VERIFIED WORKING (do not re-break)
Core sequential playback; entry-0 full-gain lead-in; lead-in/tail crossfades via shared EntryMixer.h (playback + export identical); WSOLA stretch with retain flags; block add/remove/rename/colour; drag reorder + drag-to-stack + drag-to-gap-unstack + Shift+drag whole stack + rearrange within stack; clip add (Finder / block tile / browse) + drag clips between blocks + independent per-clip weights + song enders + per-clip play; markers with grid snap + Shift bypass + adaptive grid + proportional zoom persistence + arrow nudge; one-click tempo field + block tempo + per-clip override + project default tempo; links create/remove + name labels (no UUIDs) + collision-avoided arc labels; white playing indicator + waveform follows playing block + playhead line + time display + selection independent of playback + Play Block/Play Clip; Save/Save As/Open + window title + single synchronous loadProject() + relative portable paths + missing-file warning + undo cleared on load; snapshot undo + one-entry-per-drag + undoable stack-count buttons; export WAV/FLAC/BSF + int64 strings + project-rate writers; app icon wired.

---

## ACCEPTANCE MAP (Step 7E, 2026-07-06) — evidence per ACCEPTANCE_TESTS.md item
Evidence classes: **T#** = Step 6 harness test (12/12 green) · **M#** = Step 6 manual checklist (7/7 user-confirmed 2026-07-06) · **H:x** = other harness section · **commit** · **PV** = prior-verified (PROGRESS "VERIFIED WORKING", user-verified pre-push) · **GAP** = NOT-YET-COVERED.

| Item | Evidence | Item | Evidence |
|---|---|---|---|
| 1.1 add block | PV | 5.9 effective 33/67/100 | T7 (±2 tol; see known limitation) |
| 1.2 delete block | PV | 5.10 base 100% + rest | T7 + 4D user-confirmed |
| 1.3 rename | PV | 5.11 stack drag ops | PV |
| 1.4 set colour | PV | 5.12 one control per stack | PV + 3D/3F (1610703) |
| 1.5 drag reorder | PV | 6.1 clip Done still plays | Step 5 grep + PENDING-MANUAL (script 1) |
| 1.6 drag to stack | PV | 6.2 block Done still plays | T10 (exact) + M7 |
| 2.1 Finder→waveform | PV | 6.3 badge placement | PENDING-MANUAL (script 2) |
| 2.2 Finder→block tile | PV | 6.4 export all-Done | Step 5 grep + PENDING-MANUAL (script 3) |
| 2.3 browse button | PV | 6.5 grep isDone Audio | Step 5 (c507dc1) |
| 2.4 clip rename/colour/remove | PV | 7.1 Play applies randomization | T1–T9 (same resolve()) + PV |
| 2.5 independent weights | PV | 7.2 playing indicator | PV |
| 2.6 clip weights 80/15/5 dist. | T16 (81.5/12.0/6.5 over 200) | 7.3 waveform follows | PV |
| 2.7 single clip 0% → 100% | T17 (20/20) | 7.4 selection independent | PV + 4C (647b084) |
| 2.8 markers snap/Shift | PV | 7.5 Play Block/Clip | PV |
| 3.1 entry-0 lead-in full gain | PV + M4 | 7.6 rapid play/stop 20× | PENDING-MANUAL (script 4, 9) |
| 3.2 zero-gap bodies | T2 + M1 | 8.1 song ender truncates | PV |
| 3.3 tail/lead-in crossfade | PV + M4 | 8.2 ender inside stack | T19 (both branches) |
| 3.4 stretch to adjacent tempo | PV | 9.1 tempo = grid only | PV |
| 3.5 retain flags | PV | 9.2 one-click tempo field | PV |
| 4.1 link create + label | PV | 9.3 block tempo + override | PV |
| 4.2 labels never overlap | PV | 9.4 project default tempo | PV |
| 4.3 100% deterministic swap | T9 | 10.1 WAV/FLAC/BSF chooser | PV |
| 4.4 3,2,1 + nothing dropped | T9 (10/10, positions unmutated) | 10.2 export == playback (Audacity) | PV + PENDING-MANUAL (script 5) |
| 4.5 0% / 50% swap rates | T13 (0/50; 103/200) | 10.3 BSF valid zip + int64 strings | H:STEP3E (d424800) + PV |
| 4.6 link into stack: that block | T14 (20/20, positions unmutated) | 10.4 stretched-join export | PENDING-MANUAL (script 6) |
| 4.7 remove link + undo | PV + T15 (undo of create + % change) | 11.1 Save/SaveAs/Open | PV |
| 5.1 SEQ pc=1 exactly one | T1 + H:Step1 + M(3F) | 11.2 reopen: all intact | T11 + M5 |
| 5.2 SEQ pc=2 | T2 | 11.3 Finder double-click open | FIXED 7400d79 + PENDING-MANUAL (script 7a/7b) |
| 5.3 SEQ pc=3 + continues | T18 (10/10 gapless, D plays) | 11.4 undo cleared on load | PV |
| 5.4 SIM pc=1 | T3 | 11.5 undo exact/no desync | T12 + M6 + PV |
| 5.5 SIM pc=2 layered | T4 + M2 | 11.6 missing-file warning | PV |
| 5.6 SIM pc=3 + longest | T5 + M2/M3 | 12.1 stress (50 blocks, rapid undo) | PENDING-MANUAL (script 8) |
| 5.7 block weights bias | T8 | 12.2 Windows parity | **GAP** (TODO 6) |
| 5.8 base toggle ON/OFF | T6 + H:STEP3C + M(3F ear) | 12.3 logo polish | **GAP** (TODO 5) |

**New items — client rev 2026-07-06 (mapped session 7):**
| Item | Evidence |
|---|---|
| 2.9 clip drag between blocks; strip does NOT scroll to start | code-inspection (BlockStrip.cpp:228–234 saves/restores scrollX on rebuild) + PENDING-MANUAL (script 10) |
| 7.7 Play Clip preserves lead-in into following block | code-inspection (playClip: timelinePos=startMark, totalDuration=endMark incl. lead-in; MainComponent.cpp:381) + PENDING-MANUAL (script 11) |
| 10.5 sample-rate/pitch (44.1k & 48k, no semitone shift) | **T20** (load-resample + export, all 4 rate combos, 440Hz→440Hz) |
| 13.1 colour renders true (no blue tint / yellow≠green) | **FIXED (session 8)** — body now 0.85 identity over neutral grey 0x3A3A3A; all 8 hues composite dominant-channel-correct (arithmetic proof); PENDING-MANUAL visual confirm (script 13) |
| 13.2 large stack (9 blocks) all tiles visible/reachable | code-inspection: does NOT reproduce at current 360px strip (perH≈36 for 9, fits; >~9 clamps to 16px + vertical scroll) — PENDING-MANUAL confirm (script 12) |
| 13.3 grid lines don't obscure waveform (long clips) | code-inspection (adaptive grid coarsens until ≥8px, ClipWaveformView.cpp:174) + PENDING-MANUAL (script 13) |
| 13.4 zoom max scales with clip length | code-inspection (computeMaxZoom = duration/0.5s, ClipWaveformView.cpp:793) — see session-7 note on wording ambiguity + PENDING-MANUAL (script 13) |

**STATUS (updated 2026-07-07 session 9):** harness-coverable items all green (**T1–T29**). PENDING-MANUAL batch triaged: **9 of 12 now headless-covered** — 6.1→T21, 6.3→T29, 6.4→T22, 7.7→T23, 10.2→T24, 10.4→T25, 13.2→T26, 13.3→T27, 13.4→T28. **Still GUI-only: 2.9, 7.6, 12.1** + 13.1 visual sign-off — consolidated into the single script `docs/MANUAL_SESSION.md`. 11.3 FIXED (7400d79). 13.1 colour hue-shift **FIXED session 8** (T29 proves badge geometry; visual sign-off is MANUAL_SESSION Step 4). NOTE for 12.2: the .bsp fix covers the macOS bundle plist only — WINDOWS file association is registry/installer-level, DEFERRED to the 12.2 session. Remaining build tasks: 12.2 Windows parity, 12.3 logo. 6.1/6.3/6.4 now fully covered headless.

## SESSION LOG

### 2026-07-07 (session 9) — Automate PENDING-MANUAL where possible; consolidate GUI checklist (verification only)
- What I changed (files): `diag/ResolverDiag.cpp` (new tests **T21–T29**), `CMakeLists.txt` (added `Source/Audio/PlaybackEngine.cpp` to the diag-only ResolverDiag target — same diag-only pattern as ExportRenderer.cpp in session 3; product build untouched), `docs/MANUAL_SESSION.md` (NEW consolidated GUI script), this entry. **No product code under Source/ changed** (`git diff --stat`: CMakeLists.txt + diag/ + docs/ only).
- Classification (12-item queue): AUTOMATABLE-HEADLESS = 6.1, 6.3, 6.4, 7.7, 10.2, 10.4, 13.2, 13.3, 13.4 (9). GUI-ONLY = 2.9 (viewport scroll needs a real laid-out Viewport), 7.6 (real audio-thread transport), 12.1 (interactive drag/undo stress).
- New tests, all PASS (assert the exact observable against the real path):
  - **T21 (6.1)** clip isDone cosmetic — same-seed pickClip sequences bit-identical with/without isDone (200 picks) + done clip still selected 128/200.
  - **T22 (6.4)** export all-Done — every block+clip isDone; ExportRenderer.renderToFile succeeds, entries=2, audio byte-identical to none-Done (maxSampleDiff 0.0).
  - **T23 (7.7)** Play Clip lead-in — reproduces MainComponent::playClip (timelinePos=startMark, total=endMark); exported [0,startMark) RMS 0.21 and equals source lead-in (maxDiff 3e-5), length not clipped.
  - **T24 (10.2)** export==playback plain — REAL PlaybackEngine (block-by-block) vs REAL ExportRenderer WAV, sample-identical (maxDiff 3e-5 = 16-bit quantisation), peak 0.30 (no clip).
  - **T25 (10.4)** export==playback with a tempo-stretched join (Intro 120 tail → Verse 160 lead-in; resolver pre-stretch confirmed present) — identical (maxDiff 4e-5), peak 0.60.
  - **T26 (13.2)** nine-block stack — mirrors BlockStrip perH=jmax(16,(areaH−gaps)/n) at areaH=360: n=7/8/9 neededH 360/356/356 all ≤360 (fit); n=30 clamps to 16px + overflow→vertical scroll (reachable).
  - **T27 (13.3)** adaptive grid — mirrors ClipWaveformView coarsen-until-≥8px loop; on-screen spacing 10.67/10.68/200px for 5min/1hr/2s clips, never <8px.
  - **T28 (13.4)** zoom — mirrors computeMaxZoom=jlimit(1,256,dur/0.5): z(1s)=2, z(10s)=20, z(100s)=200 (longer→bigger), clamps 0.3s→1, 1000s→256.
  - **T29 (6.3)** DONE badge vs name — mirrors BlockComponent geometry; name (reduced(4).trimTop(20).trimBottom(16)) and badge (removeFromBottom(16).removeFromRight(40)) disjoint for all 24 (w×h) combos; tiny tiles (≤26px) draw no badge.
- What regressed or surprised me: **T25 first FAILED (maxDiff 0.199)** — diagnosed as a TEST-material artifact, not a product bug: two 0.6-amplitude tones summed past 0 dBFS at the crossfade, so the 16-bit WAV writer clamped while the float engine buffer did not (confirmed: identical diff at blockSize=total, argmax exactly at the 35280-sample join). Fixed by lowering test tones to 0.3 peak (crossfade now ≤0.6) and adding a `peak < 1.0` guard to T24/T25 so future clipping can never silently mask the comparison. Zero product code touched. **Confirms EntryMixer.h::mixEntryToBuffer is the single shared path — playback and export are sample-identical.**
- Invariants held: isDone in Source/Audio/ still comment-only (4 hits, all comments); single mixEntryToBuffer definition (EntryMixer.h:22); all sample math via project->sampleRate. Full suite **STEP6 RESULT: ALL PASS (T1–T29)**.
- NEXT SESSION should: user runs `docs/MANUAL_SESSION.md` (2.9, 7.6, 12.1, 13.1 visual) → record item-by-item; then 12.3 logo, 12.2 Windows parity (+ Windows .bsp association), TODO 7 Colour assert.

### 2026-07-07 (session 8) — Fix 13.1: identity colour dominant on non-stacked block body
- Root cause (confirmed session 7): non-stacked block body was `block->color.withAlpha(0.10f)` over the cool blue-grey theme base → 10% of the hue over a blue-dominant base desaturated + hue-shifted every colour (yellow composited to green-dominant (41,44,37); pink to blue-dominant). True colour survived only in the ~20px header.
- Fix (BlockComponent::paint ONLY, one file): body of full-size tiles now `juce::Colour(0xFF3A3A3A).interpolatedWith(block->color, 0.85f)` — the identity hue rendered DOMINANT over a NEUTRAL grey base. Name label + bottom bpm readout adopt luminance-based black/white (same rule as clip-row headers: lum>0.55 → black) so they stay legible on the now-vivid body. Tiny/stacked path (withAlpha(0.80f)) and header strip (withAlpha(1.0f)) left byte-for-byte identical (not in the diff); tiny name colour stays textPrimary via a no-op guard.
- Chosen alpha 0.85; neutral base 0xFF3A3A3A = RGB(58,58,58).
- Proof (arithmetic modeling the exact code): all 8 palette hues composite with dominant channel MATCHING the source AFTER (yellow→R-dom (189,179,63); green→G; cyan→B; pink→R; etc.); BEFORE, yellow→G and pink→B (the bug). grep confirms withAlpha(0.80f)/(1.0f) unchanged; isDone in Source/Audio still 4 comment-only hits (zero functional). Full resolver/model harness STEP6 RESULT: ALL PASS (paint change can't touch resolver; run confirms build intact). Both Debug + Release rebuilt clean.
- Verification section 13: 13.1 PASS (arithmetic; in-app visual is script 13, now expected to pass). 13.2 unchanged (BlockStrip layout untouched). 13.3/13.4 unchanged (ClipWaveformView untouched). Header + stacked-tile rendering provably unchanged (those withAlpha lines absent from the diff).
- NEXT SESSION should: run the manual script (13 items) incl. the 13.1 visual confirm across all 8 palette colours; then 12.3 logo, 12.2 Windows parity, TODO 7 Colour assert.

### 2026-07-06 (session 7) — Doc sync + new-item mapping + 10.5 harness (T20) + Group 13 inspection
- Committed client doc revs (925c6ee): SPEC.md + ACCEPTANCE_TESTS.md rev 2026-07-06 + new CURRENT_STATE.md. Logged dirty-state/unsaved-prompt as an accepted POST-DELIVERY enhancement (do not build).
- 10.5 harness: **T20** (118f0fc) — 440Hz source at {44.1k,48k} → project {44.1k,48k}, load-resample AND export, zero-crossing pitch = 440.0Hz all 4 combos, exported file rate = project rate. PASS.
- **GROUP 13 FINDINGS (client-reported bugs inspected as live suspects; NO fixes made):**
  - **13.1 "global blue tint / yellow renders green" — CONFIRMED (root cause found).** The stored palette is correct (getBlockPalette → paletteYellow 0xFFD4C840 is genuine yellow; menu maps names→hues correctly, LookAndFeel_BlockShuffler.cpp:128). The defect is in RENDERING: a non-stacked block's large body is painted `block->color.withAlpha(0.10f)` over the cool blue-grey theme base (bgMedium 0xFF161B22 / bgLight 0xFF1F2630, both blue-channel-dominant) — BlockComponent.cpp:78. A 10% yellow over bgMedium composites to RGB≈(41,44,37): green channel highest → reads as muddy green-grey; every hue desaturates toward the blue-grey base → the strip looks "blue-tinted." The full-colour identity is only the ~20px header strip (BlockComponent.cpp:88); the dominant body area is washed out. (Tiny stacked tiles use withAlpha(0.80f) so they read correctly — the bug is worst on large/unstacked blocks, matching "everything looks tinted.")
    - SCOPED FIX PROPOSAL (needs go-ahead): make the identity colour dominant, not a 10% wash — e.g. (a) raise body alpha substantially and composite over a NEUTRAL grey rather than the blue theme base, or (b) render a solid full-height colour bar / larger colour field at ~0.7–1.0 alpha so the true hue is unmistakable, keeping text legibility via the existing luminance-based black/white choice. One-file change in BlockComponent::paint; then re-verify 13.1 by eye against all 8 palette colours. NOT done this session.
  - **13.2 "stack tiles cut off at 7–9 blocks" — NOT REPRODUCED in current code.** Strip height is 360px (MainComponent.h:70). Layout (BlockStrip.cpp:190–206): perH = jmax(16, (areaH−gaps)/n). For n=9: gaps=32, perH=jmax(16,36)=36, neededH=356 ≤ 360 → fits, centred. n=7/8 also fit. For >~9, perH clamps to 16px and the content area exceeds the viewport → vertical scrollbar (setScrollBarsShown(true,true)) makes them reachable. So the reported cut-off appears already resolved by the 360px height + min-16 + vertical-scroll fallback. No code change proposed — recommend manual confirmation at 9 (script 12). If a bottom sliver hides under the 8px horizontal scrollbar, that's the only residual; flag if the manual test shows it.
  - 13.3 grid-over-waveform: adaptive grid already coarsens until lines ≥8px apart (ClipWaveformView.cpp:174) — believed handled; manual confirm (script 13).
  - 13.4 zoom-scales-with-length: computeMaxZoom = maxDurationSeconds/0.5 (ClipWaveformView.cpp:793) → max zoom always reveals ~0.5s regardless of clip length. NOTE the spec wording "shorter clips zoom in further" is ambiguous: the implementation gives LONGER clips a bigger zoom FACTOR (to reach the same 0.5s window), i.e. equal finest detail rather than more zoom for short clips. Reasonable reading of "scales with length" but flag for client if they meant literal per-clip zoom-factor differences. Manual confirm (script 13).
- ADDITIONAL MANUAL SCRIPT ITEMS (append to the session-5 script):
  10. **2.9 clip drag, no scroll reset** — Open StressProject.bsp, scroll the strip right, drag a clip from a visible block onto a later block. EXPECT: clip moves; strip stays scrolled where it was (does NOT jump to block 1). FAILURE: strip snaps back to the start after the drop.
  11. **7.7 Play Clip lead-in** — Open TestProject.bsp, select Verse (has a 0.4s lead-in), right-click its clip → Play Clip. EXPECT: you hear the lead-in from its start (audio before the start marker is not cut off). FAILURE: playback starts abruptly at the start marker, lead-in clipped.
  12. **13.2 large stack visibility** — In StressProject.bsp (has 5 stacked pairs) build one stack of 9 (drag blocks together) OR just confirm the existing pairs render fully; then make a 9-high stack. EXPECT: all 9 tiles visible, or reachable by the strip's vertical scrollbar; none permanently hidden. FAILURE: tiles below ~7 vanish with no way to scroll to them.
  13. **13.1/13.3/13.4 visual** — 13.1: set blocks to Yellow, Green, Blue via right-click → Set Color; EXPECT each block reads as its true colour (yellow looks yellow, not green; no overall blue wash) — **this is the CONFIRMED finding, expected to FAIL until fixed**. 13.3: on a long clip, grid lines must not hide the waveform. 13.4: short vs long clip — max zoom reveals fine detail on both.
- NEXT SESSION should: await go-ahead on the 13.1 colour fix; run the manual script (now 13 items); then 12.3 logo, 12.2 Windows parity (incl. Windows .bsp association), TODO 7 Colour assert.

### 2026-07-06 (session 6) — 11.3 FIXED: .bsp file association + open-document handling (7400d79)
- CMake: `DOCUMENT_EXTENSIONS bsp` on the plugin target → Standalone bundle plist now carries CFBundleDocumentTypes (bsp, Editor role, Default rank) in BOTH configs (plutil-verified). App: `anotherInstanceStarted()` override — verified in juce_MessageManager_mac.mm that macOS routes application:openFile:/openFiles:/odoc there for BOTH cold start and already-running; shared `openBspFromCommandLine()` (trim + unquoted) also serves the Windows/Linux argv path in initialise(); window brought to front before load.
- Single load path proven by grep: `MainComponent::loadProject` has exactly two callers — Open button (MainComponent.cpp:489) and `MainWindow::openFile` (Main.cpp) reached only via openBspFromCommandLine; `Project::loadFromFile` is called only inside loadProject.
- **PROMPTING NOTE (flag for client/user decision):** the Open button does NOT prompt about unsaved changes (no dirty tracking exists anywhere) — so per the "identical prompting" constraint the double-click path doesn't either. If a prompt is wanted, it's a separate scoped change (dirty tracking + prompt on BOTH paths).
- Full harness re-ran green (STEP6 ALL PASS); clean Release + Debug rebuilds.
- **WINDOWS ASSOCIATION DEFERRED:** this fix is macOS-bundle-plist only. Windows .bsp association is registry/installer-level — assess in the 12.2 Windows parity session (also in the acceptance-map status line so it isn't forgotten).
- Noticed (NOT committed, user's working-tree edits): docs/SPEC.md + docs/ACCEPTANCE_TESTS.md revised (rev 2026-07-06: new items 2.9 clip-drag scroll, 7.7 Play Clip lead-in, 10.5 sample-rate pitch, Group 13 UI fidelity; MIDI/preview deferred post-delivery) and new untracked docs/CURRENT_STATE.md. The acceptance map does NOT yet include the new items — needs a mapping pass once the user directs it.

### 2026-07-06 (session 5) — Manual round prep: A1 FINDING (.bsp association missing), builds, test projects, session script
- **A1 FINDING (reported, NOT fixed): Finder double-click (11.3) is BLOCKED by a build-config gap.** The Release app's Info.plist has NO `CFBundleDocumentTypes` (checked with plutil; only bundle-id/name/icon keys present) and CMakeLists.txt configures no document types — macOS will not route `.bsp` files to the app. The APP-SIDE handler EXISTS (Main.cpp:106–112 opens a `.bsp` from the command line), so the separate fix to scope is: (1) add the `.bsp` document-type declaration to the bundle plist (JUCE CMake `DOCUMENT_EXTENSIONS`/plist merge), and (2) verify/add `anotherInstanceStarted()` handling for the macOS open-document event (cold-start AND already-running cases) — the command-line path alone may not cover Finder on macOS.
- A2 semantics cross-check (4.6): verbatim item — "Link to a block inside a stack swaps THAT block only (never a random stack member)". T14-encoded behavior: the SPECIFIC linked block swaps (never random — verified 20/20), implemented by pulling the linked member OUT of its stack for that pass (swappedBlockIds), with the stack's playCount clamping to the remaining members. Verdict: the wording SUPPORTS the "that block only" core (what T14 proves) but is SILENT on the side effects (member leaves the stack for the pass; remaining stack plays clamped count). Not contradicted, but if the client imagined the swapped block still counting toward the stack, that's an unconfirmed reading — flag for client sign-off if it ever comes up. No changes made.
- B builds (product code unchanged since the 7C clean builds):
  - Debug:   `build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app`
  - Release: `build-release/BlockShuffler_artefacts/Release/Standalone/BlockShuffler.app`
- C test material (commit 1ac12a6, diag-only): `ResolverDiag --gen-manual ~/Desktop/BlockShuffler_ManualRound` generated and verified (load ok, 0 missing files):
  - `~/Desktop/BlockShuffler_ManualRound/TestProject.bsp` — Intro(440Hz, tempo 120, tail 0.5s) → Verse(220Hz, tempo 160, lead-in 0.4s) → Chorus stack {880Hz, noise} SIM play 2 of 2; link Intro↔Verse at 0%. DETERMINISTIC by construction (export re-resolves — MainComponent.cpp:690 — so the project must resolve identically every time for 10.2/10.4 comparisons). Resolves to exactly 6.10s: boundaries at 2.00s (stretched join) and 4.10s.
  - `~/Desktop/BlockShuffler_ManualRound/StressProject.bsp` — 50 blocks, 5 stacked pairs, tones round-robin.
- D MANUAL SESSION SCRIPT — run top to bottom, Debug app FIRST (run the binary from Terminal to see assert logs: `"build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app/Contents/MacOS/BlockShuffler"`). KNOWN dev-noise: `juce_Colour.cpp:340` assertion lines during playback are the deferred TODO 7 — ignore THAT line only; any OTHER assertion is a finding.
  1. **6.1 clip-done cosmetic** — Open TestProject.bsp. Select Verse; right-click its clip → Mark as Done. EXPECT: clip visually dims/badges; Play 3× — the 220Hz section still sounds every time. FAILURE: no visual change, or Verse silent (a gap between the 440 and chorus sections).
  2. **6.3 DONE badge vs name** — Right-click block Verse → Mark as Done (try a second block with a different colour too). EXPECT: DONE badge visible AND the block name fully readable. FAILURE LOOKS LIKE: badge drawn over the name text (name truncated/obscured), badge missing, or unreadable contrast on some block colour.
  3. **6.4 all-done export** — Mark ALL four blocks Done → Export → WAV. EXPECT: normal file chooser, export completes, file plays (~6.10s), no warning. FAILURE: any warning/refusal/empty or truncated file.
  4. **7.6 rapid play/stop ×20 (Debug)** — Click Play↔Stop as fast as possible 20×, end on Stop. EXPECT: every press lands, audio always stops, playhead resets, terminal shows no NEW assertions (Colour spam excepted), no stuck tone. FAILURE: crash, drone after Stop, UI freeze, non-Colour assertion line.
  5. **10.2 export vs playback (Audacity)** — Play TestProject once, listen: 440 tone → 220 tone → layered 880+noise; smooth overlapped join before the 220 section. Export WAV and FLAC. In Audacity compare BOTH files: (a) total length ≈ 6.10s and WAV/FLAC identical length; (b) join positions: tone change at 2.00s and 4.10s (use spectrogram view — the 440→220 and 220→chorus transitions are unmistakable); (c) zoom each boundary: crossfaded overlap region just BEFORE 2.00s (440's stretched tail under 220's lead-in), NO click/gap/dropout at either boundary; (d) optional null test: import WAV + FLAC, invert one, mix → silence. FAILURE: length far off 6.10s, hard cut instead of overlap at 2.00s, click/gap, or a missing chorus layer (only one of 880/noise audible).
  6. **10.4 stretched join by ear** — A/B the 2.00s join: in-app playback vs exported WAV. EXPECT: identical character — the tail/lead-in are time-stretched but PITCH-CONSTANT (440 stays 440, 220 stays 220). FAILURE: export join differs from playback join, or pitch shifts (chipmunk/slowdown = resampling instead of WSOLA).
  7. **11.3 Finder double-click — UNBLOCKED (fixed in 7400d79)**. NOTE: after first launching the new build, macOS may need Launch Services to notice it — if double-click doesn't offer BlockShuffler at first, right-click the .bsp → Open With → Other… → select the Release app once (or run `lsregister -f` on the app path); thereafter double-click works directly.
     (a) COLD START: quit BlockShuffler → in Finder double-click `~/Desktop/BlockShuffler_ManualRound/TestProject.bsp`. EXPECT: the app launches AND the project is open — 4 blocks visible immediately, window title "BlockShuffler — TestProject". FAILURE: app launches empty, or file opens in another app.
     (b) ALREADY RUNNING: leave the app open with TestProject loaded, make a quick edit (drag a weight slider), then double-click `StressProject.bsp` in Finder. EXPECT: app comes to the front and Stress50 loads (50 blocks, title updates) — **NO unsaved-changes prompt**: this matches the Open button exactly, which also loads without prompting (the app has no dirty-state tracking; prompt-on-open for BOTH paths would be a separate scoped change — flag to client if wanted). FAILURE: nothing happens, a second app instance appears, or the window doesn't come forward.
  8. **12.1 stress (Debug)** — Open StressProject.bsp. EXPECT: all 50 blocks visible immediately. (a) Make 15 quick edits (weight drags, renames, reorders), then Cmd+Z ×15 rapidly — each reverts, no blank waveform/strip. (b) One minute of continuous drag chaos: reorder, drag into/out of the 5 stacks, Shift+drag a stack. (c) Play ~30s, then rapid play/stop ×5. FAILURE: crash, non-Colour assertion, block vanishing, strip corruption, stuck audio, unrecovered UI.
  9. **Release smoke** — Open the Release app (path above): open both projects, repeat 7.6 (rapid play/stop ×20), one WAV export, brief listen. EXPECT: identical behavior, no console requirement. FAILURE: anything that differs from Debug behavior.
- E: acceptance map rows flipped to PENDING-MANUAL (script step numbers), 11.3 → BLOCKED.
- NEXT SESSION should: record the manual-session results item by item; scope the 11.3 fix (plist + anotherInstanceStarted) for user go-ahead; then logo (12.3), Windows parity (12.2), Colour assert (TODO 7).

### 2026-07-06 (session 4) — Acceptance gaps round 1: T13–T19 added, ALL PASS (verification only, zero product code)
- What I changed (files): `diag/ResolverDiag.cpp` only (T13–T19); `docs/PROGRESS.md` (acceptance map flips + this entry). No product code touched.
- Commits: e83385c (A: link tests T13–T15), 2c8369b (B: flow tests T16–T19), map/log commit below.
- What I proved (all PASS, full suite green after):
  - T13 (4.5): link 0% → 0 swaps/50; 50% → 103/200 = 51.5% (within 50±10); 100% determinism already T9.
  - T14 (4.6): link endpoint inside a stack → only the linked pair swaps; resolve shape {S1,S3},W,S2 20/20; pulled-out block leaves its stack for the pass (this IS the designed cross-stack link behavior — swappedBlockIds, ArrangementResolver.cpp slot-building); nothing dropped; model positions unmutated.
  - T15 (4.7): undo of link-create and link-% change → string-identical JSON.
  - T16 (2.6): CLIP weights 80/10/10 → 81.5/12.0/6.5 over 200 pickClip calls (pickClip is public static — no product hook needed).
  - T17 (2.7): single clip at 0% weight plays 20/20 (uniform fallback).
  - T18 (5.3): SEQ pc=3 → gapless 3-block run then follower D plays, 10/10, order varies.
  - T19 (8.2): song-ender inside stack — picked → final entry, no D (15/20); not picked → 2 stack + D (5/20); both branches observed, 20/20 correct.
  - Full existing suite re-ran green (Steps 1/2/3C/4A/4B/amendment + T1–T12): STEP6 RESULT: ALL PASS.
- NEXT SESSION should: the manual/export round for the remaining 7 gaps (6.3 badge, 7.6 rapid play/stop, 10.4 stretched-join export, 11.3 Finder double-click, 12.1 stress, 12.2 Windows parity, 12.3 logo) + partial re-runs (6.1/6.4 in-app, 10.2 Audacity). Then Colour assert (TODO 7).

### 2026-07-06 (session 3) — STEP 7 COMPLETE (with 7B deviation) → tag `delivery-candidate-1`. FINAL REPORT (7F)
- **Step 6 CLOSED**: harness 12/12 + manual checklist 7/7 user-confirmed.
- **7A** (e0dd863): Step-1 diag DBG lines removed from ArrangementResolver.cpp; the ONLY remaining DBG in Source/Audio is the Step 5 guard-violation DBG (line 305, next to the jassert at 308).
- **7B deviation** (0564ba6): diag/ResolverDiag.cpp + ResolverDiag target KEPT permanently as the regression suite (see STANDING RULE above); target is EXCLUDE_FROM_ALL — default build proven not to compile it; explicit `--target ResolverDiag` works.
- **7C**: clean-first Standalone builds — Debug (build-diag/) exit 0 and Release (build-release/, new dir, gitignored) exit 0; 0 errors, 51 warnings all pre-existing (DraggableNumberBox ctor shadows ×24, WSOLA synthesisPos ×8, resolver bodyStart ×2, JUCE Font deprecation ×4, misc sign/shadow). Full harness re-run against the fresh Debug tree: all SUMMARY lines green, STEP4AMEND determinism IDENTICAL, STEP6 RESULT: ALL PASS (T1–T12).
- **7D release sanity**: in non-debug builds JUCE defines `jassert(x)` → empty statement and `DBG(x)` → nothing (juce_PlatformDefs.h:188/196, JUCE_LOG_ASSERTIONS off) → the Step 5 guard is inert in Release. **TODO #7 (juce_Colour.cpp:340 assert spam): DEFERRED DEV-NOISE** — it is `jassert(newAlpha >= 0 && newAlpha <= 1.0f)` inside `Colour::withAlpha`, JUCE_DEBUG-only, invisible in the client's Release build. Root-cause hint for later: some paint code calls withAlpha() with an out-of-range float. NOT fixed this session per instruction.
- **7E** (06bd4f7): ACCEPTANCE MAP section above — 13 items NOT-YET-COVERED (2.6, 2.7, 4.5, 4.6, 5.3, 6.3, 7.6, 8.2, 10.4, 11.3, 12.1, 12.2, 12.3) + partials noted; no fixes attempted.
- **Step 1 conclusion (for the record)**: the reported "play 1 plays ALL blocks" regression DID NOT REPRODUCE — the fix was already in the tree before this push (fossils: the guard comment describing the old slot-splitting failure, FIX H6/H7 stack-state resets). The per-stack guard (jassert + violation DBG) is retained permanently.
- **Every file touched across the push** (baseline-step1-clean..HEAD): `Source/Model/Block.h`, `Source/Model/Project.cpp`, `Source/Model/Serialization.cpp`, `Source/Audio/ArrangementResolver.cpp`, `Source/Audio/StackPicker.h` (NEW), `Source/Audio/ExportRenderer.cpp`, `Source/UI/InspectorPanel.h/.cpp`, `CMakeLists.txt`, `diag/ResolverDiag.cpp`, `docs/PROGRESS.md`, root `PROGRESS.md` (deleted). PlaybackEngine, link logic, lead-in/mixing, sequential timeline math: untouched (scope freeze held).
- **Results**: harness T1–T12 ALL PASS (see session-2 entry + acceptance map); manual checklist 7/7 (gapless SEQ, layering, longest-wait, crossfades, instant project open, undo feel, isDone visual-only).
- **Known limitations**: (1) equal-weight effective % may display 33/34/34 — within client tolerance ±2; exact-enumeration fix scoped, deferred. (2) TODO #7 Colour jassert — Debug-only dev-noise, deferred. (3) 13 acceptance items not yet covered (map above) — the remaining pre-delivery work.
- **Commits this push**: 641c345, b5dab68, 03d6fe3 (Step 2) · 30f67b4, 182760d, 18f158e, 1610703, d424800, c0c2de6, f4a6f44, 232657a (Step 3) · 582b598, 8361865, 647b084, db5b564, 089a8e1, c41f164 (Step 4 + amendment + close) · c507dc1 (Step 5) · 6b8628c (Step 6) · e0dd863, 0564ba6, 06bd4f7 + this commit (Step 7). Tagged `delivery-candidate-1`.
- NEXT SESSION should: work the 13 uncovered acceptance items (start with the cheap harness ones: 2.6, 2.7, 4.5, 4.6, 5.3, 8.2), then logo (12.3), Windows parity (12.2), stress (12.1), Colour assert (TODO 7).

### 2026-07-06 (session 2) — Step 4 CLOSED; Step 5 guards + Step 6 DONE — **Step 6 CLOSED 2026-07-06: harness 12/12 + manual checklist 7/7 user-confirmed.**
- Step 0: Step 4 marked CLOSED (user-confirmed incl. amendment; known limitation accepted, client tolerance ±2); stale root PROGRESS.md removed — docs/PROGRESS.md is the only log (c41f164).
- What I changed (files):
  - `Source/Audio/ArrangementResolver.cpp` — Step 5: four-point invariant comment block at top (isDone cosmetic-only / playCount honored via StackPicker / links bidirectional on local position map / sequential timeline gapless); violation DBG added next to the permanent per-stack jassert guard (lines 312–315). grep isDone over Source/Audio/: all hits comments, zero functional.
  - `diag/ResolverDiag.cpp` — Step 6 harness half: T1–T12 with per-test PASS/FAIL + evidence; `addClipWithFile()` helper writes real temp WAVs for T11/T12. TEMPORARY, remove in Step 7.
- Commits: c41f164 (Step 0), c507dc1 (Step 5), Step 6 commit below.
- What I proved (PASS/FAIL): T1–T12 ALL PASS (details in the Step 6 commit / harness output). Highlights: T2 zero-gap sequential timeline 20/20 with random order (3 distinct first blocks); T5 next block lands at +longest body (1500) 10/10; T9 link 1↔3 @100% → Z,Y,X 10/10 AND model block->position values unmutated (dumped before/after, identical); T10 isDone proof is EXACT — same RNG seed with isDone toggled produces bit-identical 100-resolve pick sequences (done block included 31/100 both ways); T11 fields + entry structure identical across save→load; T12 base/playCount/weight mutations each revert to string-identical model JSON. Full pre-existing suite (Steps 1/2/3C/4A/4B/amendment) re-ran green in the same binary.
- What regressed or surprised me: T11/T12 first FAILED — diagnosis showed a HARNESS artifact, not a product bug: harness clips had no on-disk audio file, so resetAndLoad's mark clamp (Serialization.cpp:200–201, FIX M7) zeroed endMark (bufLen=0 with the file missing). In-app, files exist and marks survive (long-verified). Fixed the TESTS to use real temp WAV files (the app's actual condition) — zero product code changed for Step 6.
- STEP 6 MANUAL CHECKLIST (user-run; the ear/UI halves the harness can't see). App: `open "build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app"`. Setup once: 3 blocks with clearly distinguishable clips (e.g. drums/bass/vocals) → drag block 2 onto block 1, drag block 3 onto the stack → 3-block stack; add a 4th unstacked block after it.
  1. T2 by ear — inspector: mode = Sequential, How Many to Play = 2 → Play several times: two different clips back-to-back with NO audible gap between them, order varies across plays.
  2. T4/T5 by ear — mode = Simultaneous, play 2 → exactly two clips AT THE SAME TIME; play 3 → all three layered, starting together.
  3. T5 next-block wait — with play 3, the 4th block must start only after the LONGEST of the three stack clips finishes (not after the first/shortest).
  4. Lead-in/crossfade — use clips with lead-in audio before the start marker: entry 0's lead-in plays at full gain from t=0; later transitions crossfade tail→lead-in smoothly (no click, no dropout).
  5. Project open — Save As, quit, reopen the .bsp: all blocks visible IMMEDIATELY in the strip (no blank strip, no delayed paint), stack intact.
  6. T12 in-app undo feel — toggle base → Cmd+Z flips it back (ONE step per click); playCount +/− → one undo step per click; drag a weight slider end-to-end → ONE undo step for the whole drag; redo (Cmd+Shift+Z) replays each cleanly.
  7. isDone visual-only — right-click a stacked block → Mark as Done → it dims/greys with the done badge but STILL plays per its normal odds (Play repeatedly; it must keep appearing).
- NEXT SESSION should: after user confirms this checklist, run MASTER_PROMPT Step 7 (remove Step 1 DBG lines + diag/ harness + ResolverDiag CMake target, KEEP the jassert guard + violation DBG + invariant block; clean rebuild both configs; final report). Then ACCEPTANCE_TESTS groups, logo (item 5), Windows parity (item 6), Colour assertion (item 7).

### 2026-07-06 — Step 3 CLOSED; MASTER_PROMPT Step 4 built (4A–4C committed), awaiting 4D user confirmation
- Step 0: user confirmed all six 3F manual tests → Step 3 marked complete (232657a); branch pushed, origin now current (03d6fe3..232657a → then 4A–4C pushed on top).
- What I changed (files):
  - `Source/Audio/StackPicker.h` — NEW. Shared stack-selection routine: `StackPicker::pick()` (playCount draw + clamp [1, groupSize], alwaysPlayBase branch, weighted sample WITHOUT replacement, all-zero weights → UNIFORM fallback) extracted VERBATIM from the resolver, plus `StackPicker::inclusionProbabilities()` (Monte Carlo 2000 trials driving pick(); shortcut: min playCount >= groupSize → all 100% with no simulation).
  - `Source/Audio/ArrangementResolver.cpp` — stack slot now calls `StackPicker::pick()`; nothing else changed. RNG call order preserved. NOTE: the stack pick's all-zero-weights fallback was ALREADY uniform (no getLast() in that path — the getLast() hits are in pickClip's float-edge fallback, untouched) → 4A was a zero-behavior-change extraction.
  - `Source/UI/InspectorPanel.h/.cpp` — `computeStackInclusionProbabilities()` is now a thin wrapper over `StackPicker::inclusionProbabilities()` (old reimplemented sampling deleted — it had a last-element fallback and no alwaysPlayBase awareness). 4C: `recalcStackEffectiveLabels()` gates the MC behind a per-group state fingerprint (mode, alwaysPlayBase, stackPlayCount values+weights, member ids + playChances); recompute happens ONLY when that state changes = weight drag end / playCount +/- / base toggle / membership change / project load (and undo of those). The Play-time recompute (updateTimeDisplay play-follow → inspectorPanel.setBlock → rebuild → fresh time-seeded MC) is now a cache hit; setBlock itself intentionally kept ("inspector follows playing block" is a VERIFIED WORKING feature).
  - `diag/ResolverDiag.cpp` — STEP4A (all-zero weights SEQ pc=1, 20 resolves) and STEP4B (six inclusion-% scenarios via the exact shared function). TEMPORARY, remove in Step 7.
  - `docs/PROGRESS.md` — this entry.
- Commits: 232657a (Step 3 close), 582b598 (4A shared StackPicker), 8361865 (4B inclusion calc), 647b084 (4C recompute triggers).
- What I proved (PASS/FAIL):
  - 4A PASS: full harness re-run at previous rates — Step 1 scenarios (SEQ pc=1, SIM pc=1, SIM pc=2 identical timelinePos, JSON round-trip) 10/10 each; Step 2 (SIM pc=3 → identical timelinePos, cursor += longest body: D@2500) 10/10; Step 3C six scenarios 20/20 each incl. baseOFF regressions (SIM pc=1 basePresent 7/20 — same as pre-refactor). NEW STEP4A: all-zero weights SEQ pc=1 → entries==1 20/20, block varies (A=6 B=4 C=10).
  - 4B PASS (2000-trial MC, shared function): 3 equal pc=1 → 33.6/33.9/32.5 (±2 ✓); pc=2 → 67.6/65.2/67.2 (±2 ✓); pc=3 of 3 → 100.0 flat (shortcut, no simulation); SIM baseON pc=2 → base 100.0, others 48.6/51.4 (±3 ✓); weights 80/10/10 pc=1 → 80.3/10.0/9.7; all-zero → 33.4/31.6/35.0 (uniform fallback).
  - 4C PASS (static): remaining recompute paths all route through the fingerprint gate; Play leaves the model untouched → fingerprint identical → no recompute. Debug standalone builds clean (0 errors).
  - Scope freeze honored: link logic, isDone, lead-in, PlaybackEngine mixing, sequential timeline math untouched; jassert guard + Step 1 DBG lines still in place (removal is Step 7).
- What regressed or surprised me: nothing. The suspected getLast() fallback in the stack picker did not exist (already uniform); the only getLast() is pickClip's roll-edge fallback, out of 4A scope.
- Step 4D hand-off (user-run; Step 4 NOT complete until confirmed):
  1. `open "build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app"`
  2. 3 blocks, one clip each → stack all three (drag 2 onto 1, then 3 onto the stack). Inspector shows the three block rows with weight sliders + "eff:" labels.
  3. Four hand-checked numbers (all weights equal at 100): How Many to Play = 1 → eff ≈ 33% each; = 2 → ≈ 67% each; = 3 → 100% each (exact); then mode = Simultaneous, "Always play base block" ON, play count 2 → (base) row 100%, others ≈ 50% — the flip to 100/50/50 must appear IMMEDIATELY on toggling.
  4. Play-stability: note the three eff values, press Play, Stop, Play several times → the numbers must NOT change at all (previously they jiggled on every Play).
  5. Weight drag: drag a weight slider — eff must stay frozen DURING the drag and update once on release.
  6. Save As → close → reopen: eff values shown immediately are EXACTLY identical to pre-save (deterministic seed — see amendment below); the base row must still read 100%.
- STEP 4 AMENDMENT (same day): `StackPicker::inclusionProbabilities` made DETERMINISTIC — RNG seeded from a hash of the stack state (mode, alwaysPlayBase, playCount values+weights, member ids + playChances), so identical state → bit-identical numbers across recomputes, Plays, and save/reopen. Trials 2000 → 20000 (NOT the amendment's 50000: that measured 130ms on a 6-block stack in Debug, over the 50ms budget, so dropped to 20000 per the amendment's own fallback; 20000 ≈ 52ms Debug). Display already rounded to whole % (InspectorPanel.cpp `roundToInt`). `StackPicker::pick()` untouched; full harness re-run at previous rates. KNOWN LIMITATION (reported to user, awaiting decision): with 3 equal weights / pc=1, the true value 33.33 sits 0.17 from the 33/34 rounding boundary while MC standard error at 20k is ~0.33 — so roughly 4 out of 5 freshly-created stacks freeze a display of 33/34/33-style inequality (each stack's UUIDs seed a different frozen draw). No feasible trial count fixes this (needs ~500k+). Candidate fix if client insists on visually equal display: exact enumeration for small stacks (n ordered selections = n!/(n−pc)! — trivial for real stack sizes) with MC fallback — but that reimplements selection math outside pick(), which Step 4B forbade, so it needs an explicit user decision.
- NEXT SESSION should: after user confirms 4D, mark Step 4 complete; then Step 5 (regression guards), Step 6 (12-point re-test), Step 7 (cleanup: remove diag/ harness + STEP1-4 DBG/diag code, keep jassert guard).

### 2026-07-05 (session 3) — MASTER_PROMPT Step 3 built (3A–3E committed), awaiting 3F user confirmation
- Step 0 backup: remote `UI_firstdraft` held a stale 2026-04-29 orphan snapshot (strict subset of local tree, zero remote-only files) — replaced via `git push --force-with-lease`; branch now tracks origin. NOTE: that push happened BEFORE 3A — origin is at 03d6fe3; commits 30f67b4..HEAD are local only. Next session: `git push` first.
- What I changed (files):
  - `Source/Model/Block.h` — `bool alwaysPlayBase = false;` (stack-level, simultaneous only).
  - `Source/Model/Project.cpp` — propagateStackSettings copies alwaysPlayBase across the group.
  - `Source/Model/Serialization.cpp` — toJSON writes / fromJSON reads "alwaysPlayBase" (missing key → false; pre-3B projects load unchanged).
  - `Source/Audio/ArrangementResolver.cpp` — simultaneous pick only: if alwaysPlayBase, base block (FIRST of the stack group in project->blocks order) is pre-picked, remaining (playCount−1) weighted-sampled without replacement from the rest; playCount==1 → base only. `isSimultaneous` hoisted above the pick (only structural move). Sequential/standalone paths untouched; jassert guard intact (now line 345).
  - `Source/UI/InspectorPanel.h/.cpp` — "Always play base block" ToggleButton, visible ONLY when mode == Simultaneous; undo = local pre-snapshot → mutate → propagateStackSettings → applyExternalMutation (recordMutation, suppressUndo-aware); "(base)" suffix on the base block's row in the combined stack view (shown in Simultaneous mode); mode flips rebuild labels + layout via lastBuiltSimMode.
  - `Source/Audio/ExportRenderer.cpp` — BSF model.json blocks now carry "alwaysPlayBase".
  - `CMakeLists.txt` — ExportRenderer.cpp added to ResolverDiag target (diag-only; Step 7 removes target).
  - `diag/ResolverDiag.cpp` — STEP3B (serialization round-trip + stripped-key load), STEP3C (6 scenarios × 20 resolves), STEP3E (headless renderToBsf → print model.json). All TEMPORARY, remove in Step 7.
  - `docs/PROGRESS.md` — this entry.
- Commits: 30f67b4 (3A model+propagation), 182760d (3B serialization), 18f158e (3C resolver), 1610703 (3D inspector+undo), d424800 (3E BSF export).
- What I proved (PASS/FAIL):
  - 3B PASS: save→load alwaysPlayBase=true on both stacked blocks; JSON with key stripped → false.
  - 3C PASS 20/20 each: SIM baseON pc=2 → base present 20/20, exactly 2 entries, identical timelinePos; pc=1 → base only; pc=3 → all three. REGRESSION base OFF: SIM pc=1 → 1 entry, base only 7/20 (varies); SEQ pc=1 → 1 entry; SEQ pc=2 → 2 entries ascending timeline. `git diff` confirmed changes confined to the simultaneous pick.
  - 3E PASS: headless BSF export, model.json stack blocks show `"alwaysPlayBase": true`.
- What regressed or surprised me: nothing; all regression scenarios green.
- Step 3F hand-off (user-run; Step 3 NOT complete until confirmed):
  1. `open "build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app"`
  2. 3 blocks with distinguishable clips → stack all three → mode = Simultaneous. The first block of the stack shows "(base)" in the inspector's stack view; the "Always play base block" toggle appears under Play Mode (and disappears in Sequential).
  3. Toggle ON, How Many to Play = 2 → several Plays: the (base) block sounds EVERY time plus exactly one other.
  4. Cmd+Z / Cmd+Shift+Z: toggle flips back/forth, one undo step per click.
  5. Save As → reopen: toggle still ON.
  6. Open any project saved before today → toggle OFF.
- NEXT SESSION should: after user confirms 3F, mark Step 3 complete; then Step 4 (effective % — must teach computeStackInclusionProbabilities about alwaysPlayBase and share the picker with the resolver).

### 2026-07-05 (session 2) — VCS baseline + Step 2 runtime half PASSED (ear check pending)
- What I changed (files):
  - `.git/` — NEW repo initialised at the project directory. Root cause of "Source/ untracked": the previous repo was rooted at `/Users/alecgordon` ($HOME), tracking a stale April copy at `~/Source` plus dead `Downloads/` worktrees; nothing under this project dir was tracked. Fix: `git init` here, fetched `UI_firstdraft` history from the home repo (GitHub workflow commits preserved), pointed the branch at it without touching files, set `origin` to github.com/gobi10k/BlockShuffler.git. Home repo untouched.
  - `.gitignore` — rewritten: build*/ (covers build-diag/), Builds/, CMake cache dirs, *.app/*.vst3/*.component/*.o/*.a, xcuserdata, .DS_Store & friends, terminal_output.txt, .claude/.
  - `diag/ResolverDiag.cpp` — added STEP2 scenario: 3-block SIM stack playCount=3 with DISTINCT body lengths (1000/2500/1800) + trailing unstacked block D(700) to expose the cursor; `addClipTo` gained a bodyLen param. TEMPORARY, remove in Step 7.
  - `docs/PROGRESS.md` — this entry.
  - No functional code changed anywhere. No resolver fix was needed.
- Commits (baseline + one per step from now on):
  - `828c5a4` "Baseline: full tree before final delivery push (Step 1 verified clean)" — tagged `baseline-step1-clean`. 45 files under Source/ tracked; `git status --short` empty.
  - `641c345` "Step 2 (runtime half): extend ResolverDiag with SIM play-3 layering/cursor case".
- What I proved (PASS/FAIL):
  - PASS: Part B residual — jassert guard (ArrangementResolver.cpp:326) and Step 1 DBG lines (222, 226) still present. No client .bsp was provided → load-path dump skipped.
  - PASS (10/10): SIM playCount=3 → 3 entries, ALL at identical timelinePos=0, every resolve; DBG stderr shows `playCount=3 / picked=3` each run.
  - PASS (10/10): cursor advance = LONGEST body — trailing block D lands at 2500 (longest body), not 5300 (sum) and not the first-picked block's length.
  - PASS: no Step 1 regression — same binary re-ran SEQ play-1 and SIM play-1: exactly 1 entry, 10/10 each.
- What regressed or surprised me:
  - The $HOME-rooted repo (above). Also note: a second stale project copy still sits at `~/Source` etc. and dead copies in `~/Downloads` — candidates for manual deletion by the user, NOT by Claude.
- Step 2 ear check: **PASSED — user confirmed layering by ear same day. STEP 2 COMPLETE.** Steps used:
  1. `open "build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app"` (or run the inner binary from a terminal to also see DBG).
  2. Add 3 blocks, one clearly distinguishable clip each (e.g. drums / bass / vocals).
  3. Drag block 2 onto block 1, then block 3 onto the stack → 3-block stack.
  4. Inspector: stack mode = Simultaneous, How Many to Play = 3.
  5. Press Play several times: all three clips must sound AT THE SAME TIME, starting together, each at ~1/3 gain; the next block (add one after the stack to check) must start only after the LONGEST of the three finishes.
- NEXT SESSION should:
  1. MASTER_PROMPT Step 3 ("Always play base block"), then 4–7 in order. Do not touch link logic, isDone, lead-in, or the Colour assertion (item 7) until their steps.

### 2026-07-05 — MASTER_PROMPT Step 1: stack play-count regression DOES NOT REPRODUCE
- What I changed (files):
  - `Source/Audio/ArrangementResolver.cpp` — added the two Step 1 DBG lines (grp/mode/playCount/groupSize + picked). TEMPORARY, remove in Step 7.
  - `diag/ResolverDiag.cpp` + `ResolverDiag` target in `CMakeLists.txt` — headless harness: builds a 3-block stack via the real model APIs (addBlock/stackBlocks/propagateStackSettings), resolves 10× per scenario, dumps model state. TEMPORARY, remove in Step 7.
  - `docs/PROGRESS.md` — this entry.
  - No functional code changed. No fix was needed.
- What I proved (grep/DBG/tests, PASS/FAIL):
  - PASS (static): resolver already implements the required invariant — `playCount = stackPlayCount.pick(rng)` clamped to [1, groupSize], weighted sample WITHOUT replacement, entries created only from picked subset (ArrangementResolver.cpp:186–216). None of Step 1's hypotheses (a)/(b)/(c) present.
  - PASS (headless, Debug build): SEQ play 1 → 10/10 resolves exactly 1 entry. SIM play 1 → 10/10 exactly 1. SIM play 2 → 10/10 exactly 2, identical timelinePos. Play count edit + propagate honored. JSON round-trip via resetAndLoad (the undo path) preserves sg/mode/playCount and still resolves correctly.
  - PASS (in-app, Debug standalone, real audio, user-run): 13 Play presses → 13 resolves, every one `STACK grp=0 mode=Seq playCount=1 groupSize=3 / picked=1`; exactly ONE block audible per press (user-confirmed).
  - PASS (Step 2 grep half, early): `grep -n "break\|continue\|isOverlay" Source/Audio/PlaybackEngine.cpp` → no isOverlay references, no offending break; the only `continue` is the correct window-skip at PlaybackEngine.cpp:105. Engine mixes all entries overlapping the buffer window and can only play resolved entries.
  - Conclusion: the regression was fixed at some earlier point (fossils: jassert guard at ArrangementResolver.cpp:319 whose comment describes the exact old failure — slot-splitting → standalone playChance gate; FIX H6/H7 stack-state resets). PROGRESS item 1 was stale.
- What regressed or surprised me:
  - `Source/` is completely untracked in git (branch UI_firstdraft tracks only workflow files) — no history to bisect, and a major data-loss risk. See TODO item 8.
  - Debug builds spam `JUCE Assertion failure in juce_Colour.cpp:340` continuously while painting during playback with audio — out-of-range float passed to a Colour constructor somewhere in paint code. See TODO item 7.
  - Build note: diagnostic Debug build lives in `build-diag/` (Standalone app: `build-diag/BlockShuffler_artefacts/Debug/Standalone/BlockShuffler.app`). Run the binary inside from a terminal to see DBG output.
- NEXT SESSION should:
  1. Commit Source/ (+ docs, CMakeLists, Resources) to git before touching anything else.
  2. MASTER_PROMPT Step 2 remainder: 3-block SIM stack, play 3 → paste DBG showing three entries at identical timelinePos, confirm layering by ear. Then Steps 3–7 in order (Step 7 cleanup must remove the Step 1 DBG lines AND the diag/ harness + CMake target).

### 2026-07-04 — Project/docs restructure (planning session, no code)
- Consolidated all context into repo docs so Claude Code has continuity: added root `CLAUDE.md`, this `PROGRESS.md`, and `docs/` (SPEC, ACCEPTANCE_TESTS, MASTER_PROMPT).
- Client (Carter) locked the spec and confirmed effective-probability semantics (1-of-3 equal = 33%, 3-of-3 = 100%) — validates the Monte-Carlo inclusion-probability approach.
- Only genuinely new feature remaining: "Always play base block" for simultaneous stacks. Everything else is fix-and-verify.
- NEXT SESSION: start `docs/MASTER_PROMPT.md` Step 1 (diagnose the stack play-count regression with logging; confirm root cause before fixing).

<!-- Template for the next entry:
### YYYY-MM-DD — <short title>
- What I changed (files):
- What I proved (grep/DBG/tests, PASS/FAIL):
- What regressed or surprised me:
- NEXT SESSION should:
-->
