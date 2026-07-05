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
1. **Stack play count regression** — a stack set to "play 1" plays ALL blocks; both modes. ROOT CAUSE NOT YET CONFIRMED. See MASTER_PROMPT Step 1.
2. **Simultaneous layering** — verify entries share timelinePos and PlaybackEngine mixes all overlapping entries (no leftover isOverlay skip / no break in loop). MASTER_PROMPT Step 2.
3. **NEW FEATURE: "Always play base block"** (simultaneous only) — not yet built. Model + serialization + resolver + inspector toggle + BSF. MASTER_PROMPT Step 3.
4. **Effective % = inclusion probability** — verify against client numbers (33 / 67 / 100). Share the picker function between resolver and display. MASTER_PROMPT Step 4.
5. **Logo polish** — background must match transport bar colour exactly, larger, sit just left of Save As. Verify logo + app icon load via BinaryData (cross-platform), not runtime file paths. ACCEPTANCE_TESTS 12.3.
6. **Windows parity pass** — ACCEPTANCE_TESTS 12.2.

### NEXT UP
Run `docs/MASTER_PROMPT.md` in order (Steps 1–7). It fixes items 1–4 above with proof required at each step. Then work the remaining ACCEPTANCE_TESTS groups, then logo (item 5), then Windows parity (item 6).

---

## VERIFIED WORKING (do not re-break)
Core sequential playback; entry-0 full-gain lead-in; lead-in/tail crossfades via shared EntryMixer.h (playback + export identical); WSOLA stretch with retain flags; block add/remove/rename/colour; drag reorder + drag-to-stack + drag-to-gap-unstack + Shift+drag whole stack + rearrange within stack; clip add (Finder / block tile / browse) + drag clips between blocks + independent per-clip weights + song enders + per-clip play; markers with grid snap + Shift bypass + adaptive grid + proportional zoom persistence + arrow nudge; one-click tempo field + block tempo + per-clip override + project default tempo; links create/remove + name labels (no UUIDs) + collision-avoided arc labels; white playing indicator + waveform follows playing block + playhead line + time display + selection independent of playback + Play Block/Play Clip; Save/Save As/Open + window title + single synchronous loadProject() + relative portable paths + missing-file warning + undo cleared on load; snapshot undo + one-entry-per-drag + undoable stack-count buttons; export WAV/FLAC/BSF + int64 strings + project-rate writers; app icon wired.

---

## SESSION LOG

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
