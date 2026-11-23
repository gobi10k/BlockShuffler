# BlockShuffler — manual QA checklist (UI_firstdraft, four uncommitted fixes)

Build under test: **`/Users/alecgordon/Documents/BlockShuffler-Hail Mary/build-release/BlockShuffler_artefacts/Release/Standalone/BlockShuffler.app`**.
Do NOT use `~/Documents/bs-head/build-head/.../BlockShuffler.app`. It is an old a051043 build without any of these fixes, and it is why the 11-14 stack-link check failed.
Open each file with **Open [Cmd+O]**. Every file was written by the app's own save path and reloaded through its own load path. Each round trip matched, with 0 missing audio files.
Every clip is a spoken "Block N", 2.0 s long, at 120 BPM (one bar). Your ear tells you which block is playing.

| Fix | What changed |
|---|---|
| 1 | The link-mode source marker is now an outline on the source block's own tile. It used to be a full-height line through the whole stack column. The overlay rects were also 8 px off and are now aligned. |
| 2 | Links between stacks fan out from each member tile's side edge. Each label sits on its own arc, with no overlaps. |
| 3 | Tooltips wrap, and the tooltip window is sized from the same layout that draws it, so nothing is clipped or ellipsized. |
| 4 | When a dragged tile is over a column, the outer 25 % on each side INSERTs and the middle 50 % STACKs. Gaps INSERT. The hover preview and the drop use the same code. |

---

## QA1_link_mode.bsp — Fix 1 (acceptance 4.1)
Layout: `1 · 2 · [3 base, 4, 6] · 5 · 7 · [8 base, 9]`. There are no links.

- [ ] Right-click **Block 4** (middle of stack A) → *Link to...*. Only Block 4's tile is tinted with a thick accent outline. No vertical line runs through Blocks 3 or 6, or anywhere else.
- [ ] Press Esc. The outline disappears.
- [ ] Repeat from **Block 3** (top of the stack) and **Block 9** (bottom of stack B). The outline is always on that tile only.
- [ ] Repeat from **Block 5** (unstacked). The outline sits exactly on the tile and is not shifted right. Before the fix it was 8 px off.
- [ ] In link mode from Block 4, click **Block 8**. The link is created, and its arc and label appear (4.1).
- [ ] Narrow the window so the strip scrolls, enter link mode from Block 9, then scroll. The outline is cut off at the strip edge and never drawn over the waveform or inspector.

## QA2_links_between_stacks.bsp — Fix 2, the client's screenshot (4.1, 4.2)
Layout: `1 · 2 · [3 base, 4, 6, 12] · 5 · 7 · 8 · 9 · [10 base, 11]`. It has 13 links, all at 50 %: 3↔4, 3↔6, 6↔4, 12↔4, 12↔3, 11↔12, 12↔10, 3↔10, 6↔10, 11↔6, 4↔10, 11↔4, 11↔3.

- [ ] The links inside stack A (3↔4, 3↔6, 6↔4, 12↔4, 12↔3) bracket out to the side of the column, as before.
- [ ] The 8 links between the stacks are 8 distinct curves. Before the fix they were drawn on top of each other. Each curve starts at its own member tile's side edge, and the endpoints on one tile are spread apart.
- [ ] Every label reads "Block X ↔ Block Y" with a 50 % pill. Each label sits on or right beside its own curve. No two labels overlap (4.2).
- [ ] If a label had to move off its curve, a thin leader line joins it to the middle of that same curve.
- [ ] Enter link mode from any block. No label, arc label or leader covers the link-mode banner (round 2, step 3).
- [ ] **Arc direction (round 2, step 4).** A link with a stacked end bulges DOWN when its two ends average below the strip's middle, and UP otherwise. 11↔12, 12↔10, 11↔6 and 11↔4 curve through the lower half. 3↔10, 4↔10, 6↔10 and 11↔3 curve over the top. No arc is cut off at the top or bottom, and the labels stay on their curves without overlapping.
- [ ] **Selection focus (round 2, step 5).** Click Block 4. Its links (3↔4, 6↔4, 12↔4, 4↔10, 11↔4) stay at full strength and are drawn on top. Every other arc fades to about 15 % and its label to about 25 %. Click Block 10 and the focus moves to 12↔10, 3↔10, 6↔10 and 4↔10. Click empty space and all links return to full strength. No arc or label MOVES when you change the selection.
- [ ] In link mode (right-click Block 4 → Link to...), Block 4's links are the focused ones.

## QA2b_flat_links.bsp — Fix 2 regression, no stacks (4.1, 4.2)
Layout: Blocks 1–8, unstacked. Links at 50 %: 1↔3, 2↔5, 4↔7, 6↔8.

- [ ] There are four arcs over the strip, and their labels do not overlap.
- [ ] Each label sits on its own arc. This is the intended change: the labels used to sit at the top and could drift away from their arc.
- [ ] Round 2 does not change this file with nothing selected. It was pixel-identical to commit 08d1462 in the live app. All four arcs still bulge UP.

## QA2c_tall_stack_links.bsp — Fix 2 stress (4.1, 4.2, 13.2)
Layout: `1 · [2 base … 10] (9 tall) · 11 · 12 · [13 base, 14, 15] · 16`.
Links at 50 %:

| Link | Ends |
|---|---|
| 2↔13 | top ↔ top |
| 6↔14 | middle ↔ middle |
| 10↔15 | bottom ↔ bottom |
| 2↔15 | top ↔ bottom |
| 10↔13 | bottom ↔ top |
| 6↔13 | middle ↔ top |
| 4↔16 | stack A member → unstacked block on the right |
| 14↔1 | stack B member → unstacked block on the left |

- [ ] All 9 tiles of stack A are visible and reachable (13.2). The strip has **no vertical scroll bar** at any window size, only the horizontal one (round 2, step 2).
- [ ] Each of the 8 links leaves from its own tile's height: top, middle or bottom. None of them leave from the column centre.
- [ ] No arc is clipped off the top or bottom of the strip. Tall arcs are flattened rather than cut. Links from the bottom of the stacks (10↔13, 14↔1) now bulge DOWN.
- [ ] No two labels overlap.
- [ ] Scroll the strip. The labels move with their arcs and disappear with them, and none is left stranded at the edge.

## QA3_tooltips.bsp — Fix 3 (all 28 tooltips)
Layout: `1 (3 clips: take 1 = 60 %, take 2 = 30 %, take 3 = 10 %) · [2 base, 3] SIMULTANEOUS, play 2, base ON · [4 base, 5] SEQUENTIAL, play 1 · 6`. Link 1↔6 at 50 %.

Hover over each control until its tooltip shows. **Pass** = the text is complete, nothing is ellipsized ("…"), and no line is clipped at the box edge. Long tips wrap onto two or three balanced lines. Near the window edge, the box stays fully on-screen.

| # | Where | Expected text (start) | ✓ |
|---|---|---|---|
| 1 | Waveform editor, zoom + | Zoom in [Cmd+scroll] | [ ] |
| 2 | Waveform editor, zoom − | Zoom out [Cmd+scroll] | [ ] |
| 3 | Waveform editor, fit | Reset zoom to fit | [ ] |
| 4 | Select Block 1, hover the take 1 row | block_1_take_1 \| Weight: 60% \| Effective: 60.0% | [ ] |
| 5 | Hover any stacked tile (Block 2–5) | Hold Shift and drag to move the entire stack | [ ] |
| 6 | Block strip "+" | Add a new block | [ ] |
| 7 | Transport ⏮ | Rewind to start | [ ] |
| 8 | Transport ▶ | Play / Pause [Space] | [ ] |
| 9 | Transport ■ | Stop and rewind | [ ] |
| 10 | Transport Export | Export arrangement to WAV, FLAC, or BSF bundle | [ ] |
| 11 | Transport Save | Save project [Cmd+S] | [ ] |
| 12 | Transport Save As | Save project to a new location [Cmd+Shift+S] | [ ] |
| 13 | Transport Open | Open project [Cmd+O] | [ ] |
| 14 | Inspector, clip weight slider (select a clip in Block 1) | Relative weight for clip selection within a block. | [ ] |
| 15 | Inspector, clip BPM field | BPM of this clip - sets the tempo grid… | [ ] |
| 16 | Inspector, clip tempo reset ↺ | Reset to the block's inherited tempo | [ ] |
| 17 | Inspector, Retain lead-in | Play the lead-in at its original speed… | [ ] |
| 18 | Inspector, Retain tail | Play the tail at its original speed… | [ ] |
| 19 | Inspector, Song ender | If this clip plays, the arrangement stops after it ends | [ ] |
| 20 | Inspector, clip Done | Mark this clip as done (visual flag only…) | [ ] |
| 21 | Inspector, Block tempo field | Set the tempo for this block's clips… | [ ] |
| 22 | Inspector, block tempo reset ↺ | Reset to the project default tempo | [ ] |
| 23 | Inspector, block Done | Mark this block as done (visual flag only…) | [ ] |
| 24 | Select Block 4, hover a stack chance slider | Chance (%) this block is included in the arrangement. | [ ] |
| 25 | Inspector, Play mode combo (Block 4 or Block 2) | Sequential: play chosen blocks one after another… | [ ] |
| 26 | Select Block 2, hover Always play base | The stack's base block (first in the stack) always plays… | [ ] |
| 27 | Inspector, Project section, default tempo | Project default tempo - blocks and clips without… | [ ] |
| 28 | Inspector, Project section, Raw summing (the longest tip, 3 sentences) | Raw summing (no automatic fades or level compensation) - … Can exceed full scale. | [ ] |

- [ ] Rows 5 and 28 both show their final word, so nothing is cut at the end.
- [ ] Rows 2–3 are near the window edge. Their box sits fully inside the screen.

## QA4_drag_zones.bsp — Fix 4 (1.5, 1.6, 5.11)
Layout: `1 · [2 base, 3, 4] · 5 · 6 · 7 · [8 base … 16] (9 tall) · 17 · 18 · 19 · 20`. Each stack has unstacked blocks on both sides.

**How the zones are measured.** The zones follow the **centre of the dragged tile**, not the mouse pointer. The 100 px column splits into 25 px INSERT, 50 px STACK and 25 px INSERT, and the 10 px gaps between columns are INSERT. If you grab a tile near its left edge, its centre is about 40 px right of the pointer. The zones will then seem to trigger "early" when you move right and "late" when you move left. Grabbing near the right edge does the opposite. This is deliberate: the preview and the drop read the same point, so whatever the preview shows is what the drop does. To judge the zones fairly, grab tiles in the middle. Then pointer and centre are the same point.

- [ ] Drag Block 6 slowly from left to right across Block 7. The preview goes: insert line, then the stack highlight over the middle half, then the insert line again. Release in each zone, and confirm the result matches the preview every time. Undo after each drop.
- [ ] Drag Block 5 into the gap between Block 5 and stack [2,3,4]. It inserts and does not stack (1.5).
- [ ] Drag Block 17 onto the middle of the 9-stack, including the 4 px gaps *between* stacked tiles. It stacks there, which used to fall through to insert (1.6).
- [ ] Drag Block 17 to the left or right quarter of the 9-stack column. It inserts beside the stack, not into it.
- [ ] Drag stack member Block 3 onto sibling Block 2. It rearranges within the stack, which is unchanged behaviour.
- [ ] Drag stack member Block 12 out to the gap between 19 and 20. It unstacks and stays visible (5.11).
- [ ] Shift+drag Block 9 to between Blocks 1 and 5. The whole 9-stack moves and only ever inserts (5.11).
- [ ] Every drop is undone by exactly one Cmd+Z.
- [ ] Drag Blocks 2 and 3 out of stack [2,3,4]. The last remaining block auto-unstacks (5.11).

## QA4b_stress50.bsp — Fix 4 stress (12.1)
Layout: 50 empty, unstacked blocks with no audio and no links. The source fixture is the same.

- [ ] Do about 20–30 quick drags that mix stack and insert, then press Cmd+Z 15 times rapidly. There is no crash, and the strip ends up identical to the freshly opened file (12.1).
- [ ] Every drop's result matches its preview.

## QA5_link_swaps_audio.bsp — link swaps by ear (4.3, 4.4, 4.6, 10.5)
Layout: `1 · 2 · 3 · 4 · [5 base, 6, 7] SEQUENTIAL play 3 · 8 · 9`. All links are at 100 %:

| Link | Tests | Effect |
|---|---|---|
| 1↔3 | 4.4 | 1 and 3 swap, and 2 never moves |
| 4↔5 | 4.6, base | The WHOLE stack swaps with Block 4 |
| 8↔6 | 4.6 / A3, non-base | Only Block 6 is pulled out and plays in Block 8's slot. Block 8 takes 6's old position. |

Blocks 2 and 7 are 48 kHz files, and everything else is 44.1 kHz (10.5).

What the real resolver produced on this reloaded file. The time is each body's start:
```
run 1: 3@0.0s  2@2.0s  1@4.0s  7@6.0s  5@8.0s  4@10.0s  8@12.0s  6@14.0s  9@16.0s
run 2: 3@0.0s  2@2.0s  1@4.0s  5@6.0s  7@8.0s  4@10.0s  8@12.0s  6@14.0s  9@16.0s
run 3: 3@0.0s  2@2.0s  1@4.0s  5@6.0s  7@8.0s  4@10.0s  8@12.0s  6@14.0s  9@16.0s
run 4: 3@0.0s  2@2.0s  1@4.0s  7@6.0s  5@8.0s  4@10.0s  8@12.0s  6@14.0s  9@16.0s
run 5: 3@0.0s  2@2.0s  1@4.0s  7@6.0s  5@8.0s  4@10.0s  8@12.0s  6@14.0s  9@16.0s
1000 runs: 490× "3 2 1 5 7 4 8 6 9", 510× "3 2 1 7 5 4 8 6 9" (no other order)
```
**Expected by ear: "3, 2, 1, {5, 7 in either order}, 4, 8, 6, 9"**, nine clips, 18 s.
The pair 5/7 is the sequential stack's inner order, and that order is random by design (5.3). Every other slot is fixed.
The stack plays 2 blocks, not 3, because Block 6 was pulled out by the 8↔6 link and play 3 clamps to the 2 members left.
Block 8 does **not** join the stack; it plays on its own right after Block 4.

- [ ] Press Play 5 times. Every play matches the expected order above (4.3).
- [ ] "Block 2" always plays second and is never dropped (4.4).
- [ ] Blocks 5 and 7 play together in the 4th and 5th slots, *before* Block 4. That means the whole stack swapped with 4 (4.6, base).
- [ ] Block 6 plays in the 7th slot, apart from its stack (4.6 non-base / A3).
- [ ] "Block 2" and "Block 7" (48 kHz) sound the same pitch and speed as the others, with no chipmunk effect and no drag (10.5).
- [ ] Optional: export to WAV and check that the order and pitch match what you heard (10.2, 10.5).

## QA6_stack_insert_order.bsp — round 2, step 1 (1.6, 5.11)
Layout: `1 · 2 · [3 base, 4, 5, 6] · 7 · 8`. There are no links.

A block dropped onto a stack lands **directly below the tile it was dropped on**. The base never changes.

| Drag Block 1 onto (middle of tile / gap) | Stack top→bottom afterwards |
|---|---|
| Block 3 (base) | 3 · **1** · 4 · 5 · 6 |
| Block 4 | 3 · 4 · **1** · 5 · 6 |
| Block 5 | 3 · 4 · 5 · **1** · 6 |
| Block 6 (bottom) | 3 · 4 · 5 · 6 · **1** |
| the gap between 3 and 4 | 3 · **1** · 4 · 5 · 6 |
| the gap between 5 and 6 | 3 · 4 · 5 · **1** · 6 |

- [ ] Every row above matches. While hovering, the preview shows a short bar under the target tile, and the drop lands there.
- [ ] Every drop is undone by exactly one Cmd+Z. The stack is back to `3 · 4 · 5 · 6`.
- [ ] Shift+drag the whole stack: it only ever INSERTs between columns (unchanged from round 1).
- [ ] Stack Block 7 onto Block 8: you get `[8 base, 7]`. Dropping an unstacked block onto an unstacked block works as before.

## QA7_default_tempo.bsp — round 2, step 6 (9.3)
Layout: Blocks 1–3, unstacked. The **project default tempo is 97**. Blocks 1 and 3 inherit it. Block 2 is overridden to **140**.

- [ ] Tiles read: Block 1 "97 bpm", Block 2 "140 bpm", Block 3 "97 bpm".
- [ ] Select Block 2 (the overridden one), then click "+". The new Block 4 reads **97 bpm**, not 120 and not 140. In the inspector, its tempo field is dimmed (inheriting) and has no ↺ reset button.
- [ ] Click empty space and set the Project Default Tempo to 88. Blocks 1, 3 and 4 change to 88, and Block 2 stays at 140.
- [ ] Cmd+Z once: the default goes back to 97. Cmd+Z once more: Block 4 is gone (one undo per added block).
- [ ] Type a new default and click "+" without pressing Return. The new block still ends up at the typed default.
