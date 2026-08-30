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
#include <map>
#include <algorithm>
#include <climits>
#include <juce_core/juce_core.h>
#include "../Model/Block.h"

namespace BlockShuffler {
namespace StackPicker {

/** The stack's BASE block: the FIRST block in project/model order whose
 *  stackGroup matches. SINGLE SOURCE OF TRUTH for "base of a stack" — used by
 *  the always-play-base branch below and by ArrangementResolver's link-swap
 *  base detection (Carter 4.6: a link to the base swaps the WHOLE stack). */
inline Block* findStackBase(const juce::OwnedArray<Block>& projectBlocks, int stackGroup) {
    if (stackGroup < 0) return nullptr;
    for (auto* b : projectBlocks)
        if (b->stackGroup == stackGroup) return b;
    return nullptr;
}

struct Result {
    int playCount = 1;              // resolved "How Many to Play" for this pass
    std::vector<Block*> picked;     // exactly playCount blocks (fewer only if group is smaller)
};

/** One stack-slot selection pass.
 *  groupBlocks   — the stack group's blocks (resolver passes them in slot order).
 *  projectBlocks — all project blocks in model order; defines the base block
 *                  (the FIRST block of this group in project order).
 *  forceInclude  — PIN (play-from-here, 2026-08-22): if this block is a member
 *                  of the group it is PRE-PICKED before the weighted sampling,
 *                  exactly as the alwaysPlayBase branch does, and consumes no
 *                  randomness. It REPLACES one of the sampled picks rather than
 *                  adding to them: the sampling loop below starts from
 *                  picked.size(), so the total is still exactly playCount.
 *                  nullptr (the default) reproduces the pre-pin routine
 *                  bit-identically, RNG draw order included.
 *  RNG call order is part of the contract: playCount draw, then the sampling
 *  loop (one weight roll or one uniform draw per pick). The base branch does
 *  not consume randomness. */
inline Result pick(const std::vector<Block*>& groupBlocks,
                   const juce::OwnedArray<Block>& projectBlocks,
                   juce::Random& rng,
                   const Block* forceInclude = nullptr)
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

    // PIN (play-from-here): pre-pick the clicked member, if it belongs to this
    // group, before anything else. Consumes no randomness. It goes FIRST so that
    // when playCount leaves room for only one pick it is the pin that survives —
    // an explicit user action outranks the standing alwaysPlayBase preference.
    if (forceInclude != nullptr) {
        auto pit = std::find_if(pool.begin(), pool.end(),
                                [&](Block* b) { return b->id == forceInclude->id; });
        if (pit != pool.end()) {
            out.picked.push_back(*pit);
            pool.erase(pit);
        }
    }

    // "Always play base block" (simultaneous stacks only): the base — the
    // FIRST block of this stack group in project->blocks order — is
    // pre-picked; the remaining (playCount - 1) are weighted-sampled from
    // the rest. With playCount == 1 only the base plays.
    // The two guards below are no-ops without a pin: picked is empty (so the
    // room check always passes) and the base is always still in the pool.
    if (isSimultaneous && groupBlocks[0]->alwaysPlayBase
        && (int)out.picked.size() < out.playCount) {
        Block* baseBlock = findStackBase(projectBlocks, groupBlocks[0]->stackGroup);
        if (baseBlock == nullptr
            || std::find(groupBlocks.begin(), groupBlocks.end(), baseBlock) == groupBlocks.end()) {
            // Base not in this slot (defensive — links never extract a base
            // post-4.6): fall back to the first member present in model order,
            // the pre-4.6 definition, preserving behaviour exactly.
            baseBlock = nullptr;
            for (auto* pb : projectBlocks) {
                if (std::find(groupBlocks.begin(), groupBlocks.end(), pb) != groupBlocks.end()) {
                    baseBlock = pb;
                    break;
                }
            }
        }
        if (baseBlock != nullptr) {
            // A base that is no longer in the pool IS the pinned block — already
            // pre-picked above, so there is nothing left to do (and erasing an
            // end() iterator would be undefined behaviour).
            auto bit = std::find(pool.begin(), pool.end(), baseBlock);
            if (bit != pool.end()) {
                out.picked.push_back(baseBlock);
                pool.erase(bit);
            }
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

/** Per-block INCLUSION probability for a stack group (the inspector's
 *  "effective %"), estimated by Monte Carlo driving the same pick() above —
 *  the display can never diverge from what playback actually selects.
 *  Returns blockId → probability in [0, 1].
 *
 *  DETERMINISTIC: the RNG is seeded from a hash of the stack state (mode,
 *  alwaysPlayBase, stackPlayCount values+weights, member ids + weights), so
 *  identical state always produces identical numbers — across recomputes,
 *  Plays, and save/reopen.
 *
 *  Shortcuts:
 *    - minimum possible playCount >= groupSize → everyone always plays:
 *      all 100%, no simulation;
 *    - alwaysPlayBase ON (simultaneous) → pick() pre-picks the base every
 *      trial (base counts trials/trials = exactly 100%) and samples the
 *      remaining (playCount − 1) from the rest;
 *    - all-zero weights → pick()'s uniform fallback applies. */
inline std::map<juce::String, float> inclusionProbabilities(
        const std::vector<Block*>& groupBlocks,
        const juce::OwnedArray<Block>& projectBlocks,
        int trials = 20000)   // 50000 measured 130ms (Debug, 6-block stack) — over the
                              // 50ms recompute budget, so dropped per Step 4 amendment
{
    std::map<juce::String, float> result;
    if (groupBlocks.empty()) return result;

    // If the minimum play count covers the whole stack, every block always plays.
    int minPlayCount = INT_MAX;
    if (groupBlocks[0]->stackPlayCount.isValid())
        for (int v : groupBlocks[0]->stackPlayCount.values)
            minPlayCount = std::min(minPlayCount, v);
    if (minPlayCount == INT_MAX) minPlayCount = 1;
    if (minPlayCount >= (int)groupBlocks.size()) {
        for (auto* b : groupBlocks) result[b->id] = 1.0f;
        return result;
    }

    // Seed from the stack state so the same state always yields the same numbers.
    juce::String state;
    state << (groupBlocks[0]->stackPlayMode == StackPlayMode::Simultaneous ? "SIM" : "SEQ")
          << '|' << (groupBlocks[0]->alwaysPlayBase ? 1 : 0) << '|';
    for (int v : groupBlocks[0]->stackPlayCount.values)    state << v << ',';
    state << '|';
    for (float w : groupBlocks[0]->stackPlayCount.weights) state << juce::String(w, 6) << ',';
    state << '|';
    for (auto* b : groupBlocks)
        state << b->id << ':' << juce::String(b->playChance, 6) << ';';
    juce::Random rng(state.hashCode64());

    std::map<juce::String, int> counts;
    for (auto* b : groupBlocks) counts[b->id] = 0;

    for (int t = 0; t < trials; ++t) {
        auto trial = pick(groupBlocks, projectBlocks, rng);
        for (auto* b : trial.picked)
            counts[b->id]++;
    }

    for (auto* b : groupBlocks)
        result[b->id] = (float)counts[b->id] / (float)trials;
    return result;
}

} // namespace StackPicker
} // namespace BlockShuffler
