# Phase 3: Long-Run Stability Test Report

## Test Procedure

1. Enable auto-stats by typing `p` in the serial console
2. The synth will print CPU% and heap every ~1 second
3. Let it run for 8+ hours with:
   - MIDI loop playing notes
   - Parameters being modulated
   - Presets being switched

## Expected Results

| Metric | Target |
|--------|--------|
| Heap | > 10000 bytes free |
| CPU | < 80% |
| No audio dropouts | Continuous |
| No crashes | 0 |

## How to Run

1. Connect via serial at 115200 baud
2. Type `p<Enter>` to enable auto-stats
3. Send MIDI notes or enable arpeggiator
4. Monitor output for 8+ hours
5. Type `p<Enter>` again to disable

## Monitoring Commands

- `p` — Toggle auto-stats (CPU% and heap)
- `m` — Print current heap stats

## Acceptance Criteria

- No memory leaks (heap stays stable)
- No stack overflows
- Audio continues without dropouts
- No crashes

---

*Test template created April 2026*