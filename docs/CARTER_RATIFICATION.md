# CARTER_RATIFICATION.md — three beyond-spec decisions for sign-off

The locked SPEC and ACCEPTANCE_TESTS are silent on these three behaviours, so we implemented
the most natural reading. Everything is built, tested, and passing on Mac and Windows — this
asks you to **ratify the choices**, not to test anything. An "OK" per line is all we need;
if any ruling differs, we adjust post-delivery.

1. **Drop anchoring** — when you drag one block onto another to stack them, the resulting
   stack sits where the block you dropped ONTO was (the drop target's slot), and the dragged
   block ends up where you released it. *(ruling 2026-07-10, commit 47cabbf; VALIDATION_PLAN
   BEYOND-SPEC #1)*
   - [ ] Ratified

2. **Right-click "Stack with…" matches drag** — stacking from the right-click menu anchors
   exactly like dragging: the block you right-clicked moves to the block you then click, and
   the stack stays at that clicked block's slot. *(BEYOND-SPEC #2)*
   - [ ] Ratified

3. **Name moves into the header on short tiles** — when a stacked block's tile is too short
   to show its name in the body (below ~72 px), the name renders inside the coloured header
   band instead, ellipsized, with a text colour chosen for contrast. *(commit 54ed7f8;
   BEYOND-SPEC #3)*
   - [ ] Ratified
