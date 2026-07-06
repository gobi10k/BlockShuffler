#pragma once
// Shared stack-selection routine used by BOTH ArrangementResolver (playback /
// export) and the inspector's effective-% display (Monte Carlo inclusion
// probability). Single source of truth: the displayed percentages can never
// diverge from what the resolver actually picks.
//
// INVARIANT (client-confirmed semantics, SPEC.md):
//   playCount = one draw from the group's stackPlayCount, clamped [1, groupSize];
//   weighted sample WITHOUT replacement using each block's playChance as weight;
//   all-zero weights → UNIFORM random fallback (never "always the last block");
//   "Always play base block" (simultaneous only) → base pre-picked, remaining
//   (playCount − 1) sampled from the rest; playCount == 1 → base only.
#include <vector>
#include <algorithm>
#include <juce_core/juce_core.h>
#include "../Model/Block.h"

namespace BlockShuffler {
namespace StackPicker {

struct Result {
    int playCount = 1;              // resolved "How Many to Play" for this pass
    std::vector<Block*> picked;     // exactly playCount blocks (fewer only if group is smaller)
};

/** One stack-slot selection pass.
 *  groupBlocks   — the stack group's blocks (resolver passes them in slot order).
 *  projectBlocks — all project blocks in model order; defines the base block
 *                  (the FIRST block of this group in project order).
 *  RNG call order is part of the contract: playCount draw, then the sampling
 *  loop (one weight roll or one uniform draw per pick). The base branch does
 *  not consume randomness. */
inline Result pick(const std::vector<Block*>& groupBlocks,
                   const juce::OwnedArray<Block>& projectBlocks,
                   juce::Random& rng)
{
    Result out;
    if (groupBlocks.empty()) return out;

    // Pick how many blocks to play
    out.playCount = 1;
    if (groupBlocks[0]->stackPlayCount.isValid())
        out.playCount = groupBlocks[0]->stackPlayCount.pick(rng);
    out.playCount = juce::jlimit(1, (int)groupBlocks.size(), out.playCount);

    const bool isSimultaneous =
        (groupBlocks[0]->stackPlayMode == StackPlayMode::Simultaneous);

    // Sample playCount blocks from the pool with weighted probability.
    // playChance is the selection weight — the same field the inspector exposes.
    // Without-replacement weighted sampling: each pick reduces the pool by one.
    std::vector<Block*> pool = groupBlocks;

    // "Always play base block" (simultaneous stacks only): the base — the
    // FIRST block of this stack group in project->blocks order — is
    // pre-picked; the remaining (playCount - 1) are weighted-sampled from
    // the rest. With playCount == 1 only the base plays.
    if (isSimultaneous && groupBlocks[0]->alwaysPlayBase) {
        Block* baseBlock = nullptr;
        for (auto* pb : projectBlocks) {
            if (std::find(groupBlocks.begin(), groupBlocks.end(), pb) != groupBlocks.end()) {
                baseBlock = pb;
                break;
            }
        }
        if (baseBlock != nullptr) {
            out.picked.push_back(baseBlock);
            pool.erase(std::find(pool.begin(), pool.end(), baseBlock));
        }
    }

    for (int k = (int)out.picked.size(); k < out.playCount && !pool.empty(); ++k) {
        float totalWeight = 0.0f;
        for (auto* b : pool) totalWeight += b->playChance;

        if (totalWeight <= 0.0f) {
            // All-zero weights → uniform random fallback (spec-required).
            int idx = rng.nextInt((int)pool.size());
            out.picked.push_back(pool[(size_t)idx]);
            pool.erase(pool.begin() + idx);
        } else {
            float roll = rng.nextFloat() * totalWeight;
            float cum = 0.0f;
            for (size_t i = 0; i < pool.size(); ++i) {
                cum += pool[i]->playChance;
                if (roll <= cum || i == pool.size() - 1) {
                    out.picked.push_back(pool[i]);
                    pool.erase(pool.begin() + i);
                    break;
                }
            }
        }
    }

    return out;
}

} // namespace StackPicker
} // namespace BlockShuffler
