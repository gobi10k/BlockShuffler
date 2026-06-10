// INVARIANT: isDone is cosmetic only. This file must NEVER reference isDone.
// isDone is for the UI to dim or badge blocks/clips; it has no effect on which
// blocks or clips are selected during arrangement resolution or playback.
// grep -c "isDone" Source/Audio/ArrangementResolver.cpp  must return 0.
#include "ArrangementResolver.h"
#include "TempoStretcher.h"
#include <algorithm>
#include <numeric>
#include <vector>
#include <unordered_map>

namespace BlockShuffler {

// Trim buffer to [startSample, endSample) region
static std::shared_ptr<juce::AudioBuffer<float>> trimBuffer(
    const juce::AudioBuffer<float>& src,
    int64_t startSample, int64_t endSample)
{
    int64_t len = endSample - startSample;
    if (len <= 0 || !src.getNumSamples()) return nullptr;
    auto dst = std::make_shared<juce::AudioBuffer<float>>(src.getNumChannels(), (int)len);
    for (int ch = 0; ch < src.getNumChannels(); ++ch)
        dst->copyFrom(ch, 0, src.getReadPointer(ch, (int)startSample), (int)len);
    return dst;
}

ResolvedArrangement ArrangementResolver::resolve(const Project& project,
                                                  juce::Random& rng) const {
    ResolvedArrangement result;
    result.sampleRate = project.sampleRate;

    // ── 1. Collect blocks + build lookup map ────────────────────────────────
    std::unordered_map<std::string, const Block*> blockById;
    for (auto* b : project.blocks)
        blockById[b->id.toStdString()] = b;

    // ── 2. Shuffle links and apply bidirectional position swaps ─────────────
    // Work on a local position map — never touch block->position directly.
    // The resolver must not mutate the project model; direct writes would cause
    // positions to drift across successive resolve() calls.
    std::unordered_map<std::string, int> localPos;
    localPos.reserve((size_t)project.blocks.size());
    for (auto* b : project.blocks)
        localPos[b->id.toStdString()] = b->position;

    std::vector<BlockLink*> shuffledLinks;
    shuffledLinks.reserve((size_t)project.links.size());
    for (auto* lnk : project.links) shuffledLinks.push_back(lnk);

    for (int i = (int)shuffledLinks.size() - 1; i > 0; --i) {
        int j = rng.nextInt(i + 1);
        std::swap(shuffledLinks[(size_t)i], shuffledLinks[(size_t)j]);
    }

    // Pre-roll every link's swap decision.
    //
    // Cross-stack links (blocks in different stacks, or at least one standalone):
    //   Apply the swap to localPos immediately.  The swapped block then has a
    //   different position than its stack mates; the position-equality guard in
    //   slot-building detects this and treats it as standalone for this pass.
    //
    // Same-stack links (both blocks share the same stackGroup >= 0):
    //   Swapping localPos has no effect (all stack mates already share one position
    //   value, so swapping A ↔ A is a no-op).  Instead, the decision is carried
    //   forward into the sequential-stack slot loop, where it overrides the random
    //   weighted-sample order to produce a deterministic play order.
    struct LinkDecision { BlockLink* link; bool triggered; };
    std::vector<LinkDecision> linkDecisions;
    linkDecisions.reserve(shuffledLinks.size());

    for (auto* lnk : shuffledLinks) {
        bool triggered = (rng.nextFloat() < lnk->swapProbability);
        linkDecisions.push_back({lnk, triggered});
        if (!triggered) continue;

        auto itBa = blockById.find(lnk->blockA.toStdString());
        auto itBb = blockById.find(lnk->blockB.toStdString());
        bool sameStack = (itBa != blockById.end() && itBb != blockById.end()
                          && itBa->second->stackGroup >= 0
                          && itBa->second->stackGroup == itBb->second->stackGroup);
        if (sameStack) continue;  // order enforced in sequential slot loop

        // Cross-stack: swap the two specific blocks in localPos.
        // Stack mates of either block are NOT touched — they stay at the original
        // slot position.  The position-equality guard in slot-building below will
        // separate the swapped block from its former stack mates.
        auto itA = localPos.find(lnk->blockA.toStdString());
        auto itB = localPos.find(lnk->blockB.toStdString());
        if (itA != localPos.end() && itB != localPos.end())
            std::swap(itA->second, itB->second);  // model untouched
    }

    // ── 3. Sort by resolved positions ────────────────────────────────────────
    // Rebuild order from the local (possibly-swapped) position map.
    std::vector<std::pair<int, Block*>> order;
    order.reserve((size_t)project.blocks.size());
    for (auto* b : project.blocks) {
        auto it = localPos.find(b->id.toStdString());
        order.push_back({ it != localPos.end() ? it->second : b->position, b });
    }
    std::sort(order.begin(), order.end(),
              [](const std::pair<int,Block*>& a, const std::pair<int,Block*>& b) {
                  return a.first < b.first;
              });

    std::vector<Block*> sorted;
    sorted.reserve(order.size());
    for (auto& [pos, blk] : order)
        sorted.push_back(blk);

    // ── 4. Group into slots by stackGroup ────────────────────────────────────
    // A slot is one or more blocks sharing the same stackGroup.
    // Blocks NOT in a stack (stackGroup < 0) each occupy their own slot.
    // Stacks (stackGroup >= 0) occupy a single slot at the position of the first
    // block encountered in that stack.
    struct Slot { std::vector<Block*> blocks; };
    std::vector<Slot> slots;
    std::unordered_map<int, size_t> sgToSlot;

    for (auto* b : sorted) {
        int sg = b->stackGroup;
        auto posIt = localPos.find(b->id.toStdString());
        int bPos = (posIt != localPos.end()) ? posIt->second : b->position;

        if (sg < 0) {
            slots.push_back({{b}});
        } else {
            auto it = sgToSlot.find(sg);
            if (it != sgToSlot.end()) {
                // Only group into this slot when the block's resolved position matches.
                // A cross-stack-swapped block ends up at a different position than its
                // former stack mates; it must stand alone rather than be pooled with them
                // (pooling would silently drop it or its mate via stackPlayCount).
                auto* firstBlock = slots[it->second].blocks[0];
                auto fposIt = localPos.find(firstBlock->id.toStdString());
                int slotPos = (fposIt != localPos.end()) ? fposIt->second : firstBlock->position;
                if (slotPos == bPos) {
                    slots[it->second].blocks.push_back(b);
                } else {
                    slots.push_back({{b}});
                }
            } else {
                sgToSlot[sg] = slots.size();
                slots.push_back({{b}});
            }
        }
    }

    // ── 5. Walk slots and build timeline ─────────────────────────────────────
    int64_t cursor        = 0;
    bool    songEnded     = false;
    bool    firstEntryAdded = false;  // used to offset cursor so first clip's lead-in starts at t=0

    for (auto& slot : slots) {
        if (songEnded) break;

        const auto& allBlocks = slot.blocks;

        if (allBlocks.size() == 1) {
            // ── Simple standalone block ───────────────────────────────────
            auto* block = allBlocks[0];
            if (rng.nextFloat() >= block->playChance) continue;  // block skipped this time
            if (block->clips.isEmpty()) continue;
            auto* clip = pickClip(*block, rng);
            if (!clip) continue;

            // Trim buffer to [0, endMark) — preserves lead-in before startMark
            auto trimmed = trimBuffer(*clip->audioBuffer, 0, clip->endMark);
            if (!trimmed) continue;

            // Offset cursor so the first clip's lead-in starts at timeline position 0.
            // timelinePos = body start; lead-in at [timelinePos - startMark, timelinePos).
            if (!firstEntryAdded) { cursor = clip->startMark; firstEntryAdded = true; }
            int64_t tPos    = cursor;  // body start
            int64_t bodyLen = clip->endMark - clip->startMark;

            result.entries.add({
                trimmed,
                clip->startMark, clip->endMark, clip->startMark, clip->retainTailTempo,
                clip->name, clip->id,
                tPos, 1.0f, block->id
            });

            cursor += bodyLen;
            if (clip->isSongEnder) songEnded = true;

        } else {
            // ── Stack slot ────────────────────────────────────────────────
            // Pick how many blocks to play
            int playCount = 1;
            if (allBlocks[0]->stackPlayCount.isValid())
                playCount = allBlocks[0]->stackPlayCount.pick(rng);
            playCount = juce::jlimit(1, (int)allBlocks.size(), playCount);

            // Sample playCount blocks from the pool with weighted probability
            std::vector<Block*> picked;
            std::vector<Block*> pool = allBlocks;
            for (int k = 0; k < playCount && !pool.empty(); ++k) {
                float totalWeight = 0.0f;
                for (auto* b : pool) totalWeight += b->probability;

                if (totalWeight <= 0.0f) {
                    int idx = rng.nextInt((int)pool.size());
                    picked.push_back(pool[(size_t)idx]);
                    pool.erase(pool.begin() + idx);
                } else {
                    float roll = rng.nextFloat() * totalWeight;
                    float cum = 0.0f;
                    for (size_t i = 0; i < pool.size(); ++i) {
                        cum += pool[i]->probability;
                        if (roll <= cum || i == pool.size() - 1) {
                            picked.push_back(pool[i]);
                            pool.erase(pool.begin() + i);
                            break;
                        }
                    }
                }
            }

            const bool isSimultaneous =
                (allBlocks[0]->stackPlayMode == StackPlayMode::Simultaneous);

            if (isSimultaneous) {
                // All picked blocks' bodies start at the same timeline position.
                // Lead-ins may extend before that position (each clip's own startMark back).
                //
                // Pre-roll every playChance gate first so we know the actual survivor
                // count before computing the mix gain.  This prevents the surviving
                // entries from being mixed too quietly when some picked blocks fail
                // their playChance roll (e.g. 4 picked, 3 survive → gain = 1/3 not 1/4).
                std::vector<bool> playChancePassed(picked.size(), false);
                for (size_t pi = 0; pi < picked.size(); ++pi)
                    playChancePassed[pi] = (rng.nextFloat() < picked[pi]->playChance);

                int survivorCount = 0;
                for (size_t pi = 0; pi < picked.size(); ++pi)
                    if (playChancePassed[pi] && !picked[pi]->clips.isEmpty())
                        ++survivorCount;

                const float stackGain = (survivorCount > 0)
                                      ? 1.0f / (float)survivorCount
                                      : 1.0f;

                int64_t maxBodyLen = 0;
                int64_t bodyStart  = -1;  // latched on first valid clip

                for (size_t pi = 0; pi < picked.size(); ++pi) {
                    if (!playChancePassed[pi]) continue;  // playChance gate pre-rolled above
                    auto* b = picked[pi];
                    if (b->clips.isEmpty()) continue;
                    auto* clip = pickClip(*b, rng);
                    if (!clip) continue;

                    auto trimmed = trimBuffer(*clip->audioBuffer, 0, clip->endMark);
                    if (!trimmed) continue;

                    // Initialise cursor on first ever entry so lead-in starts at t=0.
                    // timelinePos = body start; lead-in at [timelinePos - startMark, timelinePos).
                    if (!firstEntryAdded) { cursor = clip->startMark; firstEntryAdded = true; }
                    if (bodyStart < 0) bodyStart = cursor;  // all bodies share this start

                    int64_t tPos    = bodyStart;  // body start
                    int64_t bodyLen = clip->endMark - clip->startMark;

                    result.entries.add({
                        trimmed,
                        clip->startMark, clip->endMark, clip->startMark, clip->retainTailTempo,
                        clip->name, clip->id,
                        tPos, stackGain, b->id
                    });
                    maxBodyLen = std::max(maxBodyLen, bodyLen);
                    if (clip->isSongEnder) { songEnded = true; break; }  // FIX C3: exit inner loop
                }

                if (bodyStart < 0) bodyStart = cursor;  // all clips were empty/invalid
                cursor += maxBodyLen;

            } else {
                // Sequential: each picked block occupies its own time slot.

                // ── Same-stack link ordering ───────────────────────────────────
                // For any link whose both endpoints are in this picked list, enforce
                // a deterministic play order instead of the random weighted-sample
                // order:  triggered → blockB before blockA,  not-triggered → blockA
                // before blockB.  This guarantees a 100% swap probability always
                // produces the same sequence every play.  Cross-stack links have at
                // most one endpoint in picked, so they are silently skipped here.
                for (const auto& ld : linkDecisions) {
                    Block* la = nullptr;
                    Block* lb = nullptr;
                    for (auto* p : picked) {
                        if (p->id == ld.link->blockA) la = p;
                        if (p->id == ld.link->blockB) lb = p;
                    }
                    if (!la || !lb) continue;
                    auto itA = std::find(picked.begin(), picked.end(), la);
                    auto itB = std::find(picked.begin(), picked.end(), lb);
                    bool aIsFirst = (itA < itB);
                    if ( ld.triggered && aIsFirst)  std::swap(*itA, *itB);
                    if (!ld.triggered && !aIsFirst) std::swap(*itA, *itB);
                }

                for (auto* b : picked) {
                    if (songEnded) break;
                    if (rng.nextFloat() >= b->playChance) continue;  // block skipped this time
                    if (b->clips.isEmpty()) continue;
                    auto* clip = pickClip(*b, rng);
                    if (!clip) continue;

                    // Trim buffer to [0, endMark) — preserves lead-in
                    auto trimmed = trimBuffer(*clip->audioBuffer, 0, clip->endMark);
                    if (!trimmed) continue;

                    // Initialise cursor on first ever entry so lead-in starts at t=0.
                    // timelinePos = body start; lead-in at [timelinePos - startMark, timelinePos).
                    if (!firstEntryAdded) { cursor = clip->startMark; firstEntryAdded = true; }
                    int64_t tPos      = cursor;  // body start
                    int64_t bodyLen   = clip->endMark - clip->startMark;
                    int64_t bodyStart = cursor;

                    result.entries.add({
                        trimmed,
                        clip->startMark, clip->endMark, clip->startMark, clip->retainTailTempo,
                        clip->name, clip->id,
                        tPos, 1.0f, b->id
                    });

                    cursor += bodyLen;
                    if (clip->isSongEnder) { songEnded = true; break; }  // FIX C3: exit inner loop
                }
            }
        }
    }

    // ── Post-process: compute tempo-stretch ratios between adjacent entries ──
    // Simultaneous entries sharing the same timelinePos are skipped (no stretch within a slot).
    {
        std::vector<int> primary;
        for (int i = 0; i < result.entries.size(); ++i)
            primary.push_back(i);

        for (size_t k = 0; k + 1 < primary.size(); ++k) {
            auto& entA = result.entries.getReference(primary[k]);
            auto& entB = result.entries.getReference(primary[k + 1]);

            // Skip pairs sharing the same timeline position (simultaneous stack slot)
            if (entA.timelinePos == entB.timelinePos)
                continue;

            // NOTE: We need the tempos for stretch calculation.
            // In a fully robust version, tempo would also be in ResolvedEntry.
            // For now we look them up via the pointers, which is acceptable on the UI thread
            // inside resolve().

            // FIX C4: guard every blockById lookup before dereferencing
            auto itA = blockById.find(entA.blockId.toStdString());
            if (itA == blockById.end()) continue;
            auto itB = blockById.find(entB.blockId.toStdString());
            if (itB == blockById.end()) continue;
            auto* bA = itA->second;
            auto* bB = itB->second;
            const Clip* cA = bA->getClipById(entA.clipId);
            const Clip* cB = bB->getClipById(entB.clipId);

            if (cA && cB) {
                if (!cA->retainTailTempo && cA->tempo > 0.0 && cB->tempo > 0.0)
                    entA.tailStretchRatio = (float)(cA->tempo / cB->tempo);

                if (!cB->retainLeadInTempo && cA->tempo > 0.0 && cB->tempo > 0.0)
                    entB.leadInStretchRatio = (float)(cB->tempo / cA->tempo);
            }
        }
    }

    // Pre-compute WSOLA-stretched buffers for entries that need it.
    for (int i = 0; i < result.entries.size(); ++i)
    {
        auto& entry = result.entries.getReference(i);
        if (!entry.audioBuffer) continue;
        const auto& buf = *entry.audioBuffer;
        const int64_t leadInLen = entry.startMark;
        const int64_t tailLen   = juce::jmax((int64_t)0,
                                     (int64_t)buf.getNumSamples() - entry.endMark);

        if (leadInLen > 0 && std::abs(entry.leadInStretchRatio - 1.0f) > 0.001f)
        {
            auto stretched = TempoStretcher::stretch(buf, 0, (int)leadInLen,
                                                     entry.leadInStretchRatio);
            if (stretched.getNumSamples() > 0)
                entry.stretchedLeadIn =
                    std::make_shared<juce::AudioBuffer<float>>(std::move(stretched));
        }

        if (tailLen > 0 && std::abs(entry.tailStretchRatio - 1.0f) > 0.001f)
        {
            auto stretched = TempoStretcher::stretch(buf, (int)entry.endMark,
                                                     (int)tailLen, entry.tailStretchRatio);
            if (stretched.getNumSamples() > 0)
                entry.stretchedTail =
                    std::make_shared<juce::AudioBuffer<float>>(std::move(stretched));
        }
    }

    // Extend total duration to include the (stretched) tail of the last entry
    if (!result.entries.isEmpty()) {
        int lastIdx = result.entries.size() - 1;
        const ResolvedEntry& last = result.entries.getReference(lastIdx);
        int64_t tailLen = juce::jmax((int64_t)0,
                                     (int64_t)last.audioBuffer->getNumSamples()
                                     - last.endMark);
        // Use the pre-stretched buffer's actual length if it was computed
        int64_t tailTL = last.stretchedTail
                       ? (int64_t)last.stretchedTail->getNumSamples()
                       : (int64_t)(tailLen * last.tailStretchRatio + 0.5f);
        cursor += tailTL;
    }

    result.totalDurationSamples = cursor;

    return result;
}

Clip* ArrangementResolver::pickClip(const Block& block, juce::Random& rng) {
    if (block.clips.isEmpty()) return nullptr;

    juce::Array<Clip*> available;
    for (auto* c : block.clips)
        available.add(c);
    if (available.isEmpty()) return nullptr;
    if (available.size() == 1) return available[0];

    float total = 0.0f;
    for (auto* c : available) total += c->probability;
    if (total <= 0.0f)
        return available[rng.nextInt(available.size())];

    float roll = rng.nextFloat() * total;
    float cum  = 0.0f;
    for (auto* c : available) {
        cum += c->probability;
        if (roll <= cum) return c;
    }
    return available.getLast();
}

} // namespace BlockShuffler
