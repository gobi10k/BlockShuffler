# VALIDATION_PLAN.md — Master delivery-validation matrix

Derived 1:1 from `docs/ACCEPTANCE_TESTS.md` (rev 2026-07-06). Built on HEAD `e75618b` (2026-07-13).
Mac harness slice executed 2026-07-14 on HEAD `25d1e2a`: `STEP6 RESULT: ALL PASS` (T1–T46). Manual Mac rows tracked in `docs/MAC_SLICE_CHECKLIST.md`.
**Mac slice COMPLETE 2026-07-15** — every Group 1–13 row + Sections A/B PASS in the Mac-Release column (11.1 post-fix 15b0b58, 12.3 post-fix fee8947). Remaining: 12.2 + Section C + Windows-Release column (Windows slice); BEYOND-SPEC pending Carter ratification.
**Rule: every row is a concrete observable. Any FAIL blocks delivery. Fill the result columns during execution — Mac-Release and Windows-Release builds only (no Debug/ASan).**

Method legend: **T#** = ResolverDiag harness test (headless; run per PROGRESS.md STANDING RULE) · **MAN** = manual GUI step · **XP** = cross-platform check (Section C) · **grep** = source inspection. Where a row lists T# + MAN, BOTH must pass.

Harness invocation — macOS:
`cmake --build build-diag --target ResolverDiag && ./build-diag/ResolverDiag_artefacts/Debug/ResolverDiag 2>/dev/null | grep -E "SUMMARY|STEP6 RESULT|FAIL"`
Windows (MSVC, multi-config — run in "x64 Native Tools" or any shell with CMake):
`cmake -B build-diag && cmake --build build-diag --config Debug --target ResolverDiag && build-diag\ResolverDiag_artefacts\Debug\ResolverDiag.exe`
Expected on BOTH platforms: `STEP6 RESULT: ALL PASS` (T1–T46).

## Group 1 — Blocks
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 1.1 | "+" adds a block | MAN | New tile appears at strip end, palette colour, default name | PASS (MAN, 2026-07-14) | |
| 1.2 | Delete via right-click and Delete key | MAN | Right-click→Delete removes; Delete key removes selected block only when no clip is selected | PASS (MAN, 2026-07-14) | |
| 1.3 | Rename commits on Enter/focus loss | MAN | Double-click AND right-click rename; new name persists after Enter and after clicking elsewhere | PASS (MAN, 2026-07-14) | |
| 1.4 | Set Color | MAN | Tile adopts chosen colour immediately (header + body) | PASS (MAN, 2026-07-14) | |
| 1.5 | Drag to gap reorders, first attempt, every time | T43 + MAN ×10 | 10 consecutive gap-drops land where released, zero snap-backs | PASS (T43 + MAN, 2026-07-14) | |
| 1.6 | Drag onto a block stacks, first attempt, every time | T32–T34, T36, T37 + MAN ×10 | Stack forms on first attempt; merged stack sits at the DROP TARGET's slot; ONE undo entry reverts it (see BEYOND-SPEC #1) | PASS (T32–T34/T36/T37 + MAN, 2026-07-14) | |

## Group 2 — Clips
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 2.1 | OS-drag audio onto waveform | MAN | Clip added to SELECTED block; waveform renders | PASS (MAN, 2026-07-14) | |
| 2.2 | OS-drag audio onto a block tile | MAN | Clip added to THAT tile's block, not the selected one | PASS (MAN, 2026-07-14) | |
| 2.3 | "+ Add Clip" browse | MAN | Chooser filters audio formats; picked file becomes a clip | PASS (MAN, 2026-07-14) | |
| 2.4 | Clip rename / colour / remove | MAN | All three context-menu items function | PASS (MAN, 2026-07-14) | |
| 2.5 | Weight slider independence | MAN | Dragging one clip's slider leaves every other clip's value numerically unchanged | PASS (MAN, 2026-07-14) | |
| 2.6 | 80/15/5 weights drive selection | T16 | 200 resolves ≈ 81.5/12.0/6.5 (within T16 tolerance) | PASS (T16, 2026-07-14) | |
| 2.7 | Clip at 0% weight: 0% effective, never selected (Carter correction 2026-07-15) | T17 (inverted) + T17b | 0% shows 0.0% effective and never plays; a block with ALL clips at 0% is SKIPPED entirely (silent) — semantics flagged for Carter | PASS (T17+T17b, 2026-07-15) | |
| 2.8 | Marker snap + Shift bypass | T44 + MAN | Drag lands ON a grid line; Shift-drag lands off-grid at pointer | PASS (T44 + MAN, 2026-07-14) | |
| 2.9 | Clip drag between blocks; strip does NOT scroll to start | MAN | After drop into a later block, strip scroll position unchanged | PASS (MAN, 2026-07-14) | |

## Group 3 — Lead-ins and tails
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 3.1 | Lead-in audible at song start, full volume | T38 | Audio before the start marker is heard from t=0 at full gain | PASS (T38, 2026-07-14) | |
| 3.2 | Zero gap, zero overlap between bodies | T2 | Resolved timeline: entry N+1 body starts exactly at entry N body end | PASS (T2, 2026-07-14) | |
| 3.3 | Tail crossfades under next lead-in | T38 | Fade-out under fade-in at the join; no click/gap | PASS (T38, 2026-07-14) | |
| 3.4 | Cross-tempo joins stretch, pitch preserved | T39 | Lead-in/tail time-stretched to adjacent tempo; no semitone shift | PASS (T39, 2026-07-14) | |
| 3.5 | Retain flags disable stretching | T40 | Flagged lead-in/tail plays original speed; both flags → raw join | PASS (T40, 2026-07-14) | |

## Group 4 — Links (HIGH REGRESSION)
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 4.1 | Link create: arc + readable label | MAN | Arc drawn between blocks; label = block NAMES + % (no UUIDs) | PASS (MAN, 2026-07-14) | |
| 4.2 | Multiple link labels never overlap | T46 | Labels visibly separated (collision-avoided) | PASS (T46, 2026-07-14) | |
| 4.3 | 100% link swaps EVERY play, deterministic | T9 | 10/10 resolves swapped, same resulting order each time | PASS (T9, 2026-07-14) | |
| 4.4 | 1&3 linked 100%, 2 between → 3,2,1 | T9 | Order 3,2,1 every play; block 2 never moves; nothing dropped; model positions unmutated | PASS (T9, 2026-07-14) | |
| 4.5 | 0% never / 50% ≈ half | T13 | 0/200 swaps at 0%; ~103/200 at 50% | PASS (T13, 2026-07-14) | |
| 4.6 | Link into stack swaps THAT block only; link to the stack's BASE swaps the WHOLE stack (Carter correction 2026-07-15) | T14 + T14b | Non-base: 20/20 only the linked member swaps, stack-mates untouched (T14). Base: 20/20 W takes the stack's slot, intact stack takes W's slot, members still shuffle within it, 0% control never swaps (T14b) | PASS (T14+T14b, 2026-07-15) | |
| 4.7 | Remove link; undo restores | T15 + MAN | Link gone after remove; one undo restores link AND its % | PASS (T15 + MAN, 2026-07-14) | |

## Group 5 — Stacks (HIGH REGRESSION — client's #1 complaint)
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 5.1 | SEQ, play 1 of 3 → exactly one | T1 + MAN ×10 | Every resolve plays exactly ONE stack member; member varies (weighted) | PASS (T1 + MAN, 2026-07-14) | |
| 5.2 | SEQ, play 2 → exactly two, in sequence | T2 | Two members back-to-back, random order | PASS (T2, 2026-07-14) | |
| 5.3 | SEQ, play 3 → all three, then song continues | T18 | 10/10 gapless; the block AFTER the stack still plays | PASS (T18, 2026-07-14) | |
| 5.4 | SIM, play 1 → exactly one | T3 | One member at the slot's timeline position | PASS (T3, 2026-07-14) | |
| 5.5 | SIM, play 2 → two layered | T4 + MAN (ear) | Two members share the SAME timelinePos | PASS (T4 + MAN, 2026-07-14) | |
| 5.6 | SIM, play 3 → all layered; next after longest | T5 | Next block starts after the LONGEST member's body | PASS (T5, 2026-07-14) | |
| 5.7 | Block weights bias inclusion | T8 | 80/10/10, play 1: the 80-block picked most over trials | PASS (T8, 2026-07-14) | |
| 5.8 | Always-play-base ON/OFF | T6 + MAN (ear) | ON: base always + 1 weighted other; OFF: all compete | PASS (T6 + MAN, 2026-07-14) | |
| 5.9 | Effective %: 33/67/100 | T7 | Equal 3: play1→33 each (±2), play2→~67, play3→100 | PASS (T7, 2026-07-14) | |
| 5.10 | Base ON: base 100%, others reflect remaining picks | T7 | Base shows 100; others = (playCount−1) among rest | PASS (T7, 2026-07-14) | |
| 5.11 | Shift+drag whole stack; gap-drop unstacks; last auto-unstacks | MAN | All tiles move together; unstacked block lands at gap and remains visible; solo remainder auto-unstacks with settings reset | PASS (MAN, 2026-07-14) | |
| 5.12 | One global control per stack | MAN | Mode + How-Many appear once for the stack; per-block chance sliders once in combined view with effective values | PASS (MAN, 2026-07-14) | |

## Group 6 — Done flags (cosmetic only)
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 6.1 | Clip Done still plays + selectable by weight | T21 | Done clip appears in resolves per its weight | PASS (T21, 2026-07-14) | |
| 6.2 | Block Done still plays | T10 + MAN | Done block plays; ONLY visual dim/badge changes | PASS (T10 + MAN, 2026-07-14) | |
| 6.3 | DONE badge does not cover the name | T29 + MAN | Badge rect disjoint from name rect at all tile sizes | PASS (T29 + MAN, 2026-07-14) | |
| 6.4 | Export with everything Done → normal | T22 | Export completes, no warning, audio identical to not-Done | PASS (T22, 2026-07-14) | |
| 6.5 | grep isDone in Source/Audio → zero functional hits | grep | `grep -rn isDone Source/Audio/` → no hits outside comments | PASS (grep clean, 2026-07-14) | |

## Group 7 — In-editor playback
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 7.1 | Play applies all randomization | T1–T9 (same resolve()) + MAN | Weights, links, stacks, chance all observed across plays | PASS (T1–T9 + MAN, 2026-07-14) | |
| 7.2 | Playing indicator on ANY colour | MAN | White border + dark inner line visible on light AND dark blocks | PASS (MAN, 2026-07-14) | |
| 7.3 | Waveform follows; playhead moves; time updates | MAN | All three tracked during a full play-through | PASS (MAN, 2026-07-14) | |
| 7.4 | Selection unaffected by playback | MAN | Inspector/selection unchanged while playing; stop reverts waveform to selected block | PASS (MAN, 2026-07-14) | |
| 7.5 | Play Block / Play Clip work EVERY time | MAN ×10 | 10/10 launches from context menu | PASS (MAN, 2026-07-14) | |
| 7.6 | Rapid play/stop 20× | MAN | No crash, no stuck/hanging audio | PASS (MAN, 2026-07-14) | |
| 7.7 | Play Clip preserves lead-in into next block | T23 | Lead-in samples present ahead of the following block's body | PASS (T23, 2026-07-14) | |

## Group 8 — Song enders
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 8.1 | Song ender truncates (tail included) | T41 | Nothing plays after the ender's tail | PASS (T41, 2026-07-14) | |
| 8.2 | Ender inside a stack | T19 | Truncation works in both stack branches | PASS (T19, 2026-07-14) | |

## Group 9 — Tempo & grid
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 9.1 | Tempo changes GRID only | T42 + MAN | Grid spacing changes; audio speed/pitch identical | PASS (T42 + MAN, 2026-07-14) | |
| 9.2 | One-click tempo field | MAN | Single click focuses; type; Enter commits | PASS (MAN, 2026-07-14) | |
| 9.3 | Block tempo + per-clip override | MAN | Block tempo sets all; override changes only that clip; new clips inherit | PASS (MAN, 2026-07-14) | |
| 9.4 | Project default tempo | MAN | New clip in tempo-less block gets project default | PASS (MAN, 2026-07-14) | |

## Group 10 — Export
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 10.1 | Chooser offers WAV, FLAC, BSF | MAN | All three formats selectable | PASS (MAN, 2026-07-14) | |
| 10.2 | WAV/FLAC identical to playback | T24 + MAN (Audacity) | Rendered file matches in-editor playback (pitch/speed/crossfades) | PASS (T24 + MAN, 2026-07-14) | |
| 10.3 | BSF valid ZIP; big ints as strings | H:STEP3E + MAN | Rename .bsf→.zip: manifest.json + model.json + clips/*.flac; int64 fields are JSON strings | PASS (harness STEP3E + MAN, 2026-07-14) | |
| 10.4 | Stretched-join export matches playback | T25 | Export buffer == playback buffer at the stretched join (float tolerance) | PASS (T25, 2026-07-14) | |
| 10.5 | 44.1k & 48k sources: no semitone shift | T20 | 440 Hz stays 440 Hz through load-resample + export, all 4 rate combos | PASS (T20, 2026-07-14) | |

## Group 11 — Sessions & undo
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 11.1 | Save / Save As / Open; title; shortcuts | MAN | All work; window title shows project name; Cmd/Ctrl+S, +Shift+S | PASS (MAN, 2026-07-15 - title mojibake fixed 15b0b58) | |
| 11.2 | Reopen: ALL blocks visible immediately, settings intact | T11 + MAN | Full round-trip checklist; strip populated the moment the window opens | PASS (T11 + MAN, 2026-07-14) | |
| 11.3 | Double-click open == Open button | MAN | Finder/Explorer double-click loads identically (single canonical loadProject) | PASS (MAN, 2026-07-14) | |
| 11.4 | Undo history cleared on load | MAN | Cmd/Ctrl+Z after load does nothing | PASS (MAN, 2026-07-14) | |
| 11.5 | Undo exact; no desync; redo never blanks | T12 + MAN | Markers restore on-grid exactly; unrelated state untouched; redo preview intact | PASS (T12 + MAN, 2026-07-14) | |
| 11.6 | Missing file warning; session opens | MAN | Warning lists missing paths; project still loads (silent clips) | PASS (MAN, 2026-07-14) | |

## Group 12 — Stability & parity
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 12.1 | Stress: 50 blocks, drag-heavy, 15 rapid undos | MAN | No crash (Release build; ASan-clean on Mac Debug 2026-07-13) | PASS (MAN, 2026-07-14; ASan-clean) | |
| 12.2 | Windows behaves same as macOS, groups 1–11 | XP + T30 + CI | Windows CI green on HEAD; T1–T37 ALL PASS on MSVC; groups 1–11 spot-run on the Windows box | T30 PASS 2026-07-14 (Mac harness); remainder = Windows slice | |
| 12.3 | Logo: bar-matched bg, ~2/3 bar height, left of Save As | MAN (visual) | All three properties observed on both platforms. Measured 2026-07-13 (from TransportBar geometry): drawn logo 44 px / bar 56 px = **0.79** vs client "~2/3" ≈ 0.67 — confirm target with client | PASS (MAN, 2026-07-15 - logo 2/3 fix fee8947; drawn 37px / bar 56px = 0.66) | |

## Group 13 — UI fidelity
| id | description | method | expected observable | Mac-Release | Windows-Release |
|---|---|---|---|---|---|
| 13.1 | Colours render true (yellow is yellow) | T45 + MAN (visual) | All 8 palette hues visually correct; no blue tint | PASS (T45 + MAN, 2026-07-14) | |
| 13.2 | 9-block stack: all tiles reachable | T26 + MAN | Every tile visible or reachable via stack scroll; none lost | PASS (T26 + MAN, 2026-07-14) | |
| 13.3 | Grid lines don't obscure long-clip waveform | T27 | Adaptive grid keeps ≥8px spacing at any zoom | PASS (T27, 2026-07-14) | |
| 13.4 | Zoom-in limit constant with clip length: deepest window ~0.5s on long clips too (Carter correction 2026-07-15) | T28 (reframed) + MAN | dur/maxZoom = 0.5 +/- eps at 2/30/300/3000s; tiny-clip clamp intact; 65536 safety cap (T28 PASS 2026-07-15). MAN: zoom a >5min clip to the beat on the FRESH build | PASS (MAN 2026-07-15 - cursor-anchored zoom, smooth scroll at deep zoom, reaches the beat on a long clip; a416bd2 clip-region paint + T47, 1147a19 centre anchor, c6d91b2 cursor anchor; harness T28 PASS) | |

## Section A — INVARIANTS (project rules; each must hold on BOTH platforms)
| # | invariant | how to verify | result |
|---|---|---|---|
| A1 | isDone is cosmetic-only (never affects resolve/playback/export) | T10, T21, T22 + `grep -rn isDone Source/Audio/` → zero functional hits | Mac PASS (T10/T21/T22 + grep comments-only, 2026-07-14); Windows pending |
| A2 | stackPlayCount.pick honored EXACTLY (never more/fewer) | T1–T5, T18 + jassert guard ArrangementResolver.cpp:320 present | Mac PASS (T1–T5/T18 + jassert present, 2026-07-14); Windows pending |
| A3 | Link swaps: bidirectional, deterministic at 100%, third blocks never move — EXCEPT stack-mates of a BASE endpoint, which move with their stack by design (Carter 4.6, 2026-07-15) | T9, T13, T14, T14b (positions asserted unmutated) | Mac PASS (T9/T13/T14 2026-07-14 + T14b 2026-07-15); Windows pending |
| A4 | Lead-in/tail crossfade at every join (incl. entry 0 full-gain lead-in) | T38 (gain envelopes, numeric) + T2 timeline | Mac PASS (T38+T2, 2026-07-14); Windows pending |
| A5 | Sequential timeline: zero gap, zero unintended overlap | T2, T18 (gapless 10/10) | Mac PASS (T2/T18, 2026-07-14); Windows pending |
| A6 | ONE mixing path: EntryMixer::mixEntryToBuffer serves playback AND export | `grep -rn mixEntryToBuffer Source/` → exactly PlaybackEngine + ExportRenderer call sites, one definition | Mac PASS (grep: one def EntryMixer.h:22, callers PlaybackEngine.cpp:130 + ExportRenderer.cpp:34 only, 2026-07-14); Windows pending |
| A7 | sampleRate math correct at 44.1k/48k (no pitch shift) | T20 (4 rate combos, 440Hz→440Hz) | Mac PASS (T20, 2026-07-14); Windows pending |
| A8 | Single canonical synchronous loadProject() for ALL open paths | `grep -rn loadProject Source/` → Open button, initialise(), anotherInstanceStarted all route to MainComponent::loadProject; blocks visible immediately (11.2) | Mac PASS (grep + MAN 11.2/11.3, 2026-07-14); Windows pending |
| A9 | Undo discipline: one entry per user gesture; exact restore; cleared on load | T12, T15, T35, T37 + MAN 11.4/11.5 | Mac PASS (T12/T15/T35/T37 + MAN 11.4/11.5, 2026-07-14); Windows pending |
| A10 | int64 sample values serialize as JSON strings | T11 round-trip + inspect a saved .bsp: startMark/endMark are quoted strings | Mac PASS (T11 + .bsp inspected: int64 quoted strings, 2026-07-14); Windows pending |
| A11 | Audio paths saved relative with forward slashes; load accepts both separators | T30 + inspect .bsp: `media/...` no backslashes | Mac PASS (T30 + .bsp inspected: media/ forward slashes, 2026-07-14); Windows pending |

## Section B — REGRESSION-HISTORY SWEEP (the five repeat offenders — re-test EXPLICITLY before sign-off)
| # | offender (times regressed) | re-test steps | result |
|---|---|---|---|
| B1 | isDone affecting playback (×4) | Run T10, T21, T22; grep 6.5; MAN: mark a clip AND a block Done mid-session → both still play; export all-Done (6.4) | Mac PASS (harness + MAN, 2026-07-14); Windows pending |
| B2 | Stack play-count violations (×3) | Run T1–T5, T18; MAN: stack of 3, play 1, hit Play 10× → count the players EACH time (exactly one) | Mac PASS (harness + MAN x10, 2026-07-14); Windows pending |
| B3 | Link swap wrong/asymmetric (×3) | Run T9, T13, T14; MAN: the 4.4 scenario (1&3 linked 100%, 2 between) played 5× → 3,2,1 every time | Mac PASS (harness + MAN x5: 3,2,1 every play, 2026-07-14); Windows pending |
| B4 | Lead-in lost/cut (×3) | Run T23 + T38 (envelope) + T39 (pitch); MAN: 7.7 (Play Clip keeps lead-in into next block) | Mac PASS (harness + MAN 7.7, 2026-07-14); Windows pending |
| B5 | Blocks invisible on load (×3) | MAN: save a 10-block project, quit fully, reopen from Finder/Explorer double-click → ALL blocks visible immediately, no interaction needed (11.2/11.3) | Mac PASS (MAN: full quit + Finder double-click, all blocks visible immediately, 2026-07-14); Windows pending |

## Section C — CROSS-PLATFORM PARITY
| # | check | how to verify | result |
|---|---|---|---|
| C1 | .bsp saved on Mac opens on Windows | Copy project folder (bsp + media/); open on Windows: all blocks/clips/settings intact, clips resolve and play | |
| C2 | .bsp saved on Windows opens on Mac | Reverse of C1 | |
| C3 | int64 sample values intact across OSes | After C1/C2: marker positions identical to source machine (compare inspector values) | |
| C4 | Paths relative + forward-slash both directions | Open both .bsp files in a text editor: audioFile entries all `media/...`, no `\` (T30 covers load of legacy `\`) | |
| C5 | Export byte-comparable across OSes at matched sample rate | Same project, same settings, export WAV on both; compare in Audacity (invert+mix → silence) or checksum; document any float-rounding delta | |
| C6 | Windows open-on-launch | Double-click a .bsp with app closed AND with app running → loads in the single instance both times | |
| C7 | Windows .bsp association | Installer/registry step per WINDOWS_PACKAGING.md; .bsp shows app icon and opens on double-click | |
| C8 | Icon in Explorer + taskbar | BlockShuffler.exe and its window show the owl icon (not generic) | |
| C9 | Harness green on MSVC | Build+run ResolverDiag per the invocation at the top → `STEP6 RESULT: ALL PASS` (T1–T37) on the Windows box | |

## BEYOND-SPEC (Carter to ratify — NOT pass/fail rows)
1. **Drop positioning "anchor at target"** (ruling 2026-07-10, committed 47cabbf): formed/joined stack sits at the drop TARGET's slot; stacked→standalone forms a fresh pair; onto another stack joins it; dragged item ends where released. SPEC §4 / ACCEPTANCE 1.6 & 5.11 are silent on position. **[pending Carter ratification]**
2. **Right-click "Stack with…"** routed through the same anchoring as drag (mover = right-clicked block, anchor = clicked block). **[pending Carter ratification]**
3. **Name-into-header on short stacked tiles** (54ed7f8): below ~72px tile height the name renders inside the header band, ellipsized, luminance-aware colour. UI enhancement beyond SPEC. **[pending Carter ratification]**

## Sign-off
All Group 1–13 rows + Sections A/B/C green on BOTH result columns → attach this file to the delivery per ACCEPTANCE_TESTS.md sign-off.
