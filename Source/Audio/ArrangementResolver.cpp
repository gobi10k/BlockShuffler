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
    for (auto* lnk : shuffledLinks) {
        if (rng.nextFloat() < lnk->swapProbability) {
            auto itA = localPos.find(lnk->blockA.toStdString());
            auto itB = localPos.find(lnk->blockB.toStdString());
            if (itA != localPos.end() && itB != localPos.end())
                std::swap(itA->second, itB->second);  // both sides updated, model untouched
        }
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
        if (sg < 0) {
            slots.push_back({{b}});
        } else {
            auto it = sgToSlot.find(sg);
            if (it != sgToSlot.end()) {
                slots[it->second].blocks.push_back(b);
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

    // Standalone overlapping blocks (stackGroup == -1, isOverlapping == true) that
    // appear before any primary entry in position order cannot be attached yet.
    // We defer them and fire at the first primary entry added to the timeline.
    std::vector<Block*> deferredStandaloneOverlays;

    // Helper: resolve one overlapping block onto the timeline.
    // Step 1: block->playChance determines if the overlay triggers at all.
    // Step 2: pickClip() picks exactly one clip by weighted selection.
    // This separates "does the overlay fire?" from "which clip plays?" —
    // clip probability is a relative weight, not an independent trigger.
    auto addOverlay = [&](Block* ob, int64_t overlayStart) {
        if (ob->clips.isEmpty()) return;
        if (rng.nextFloat() >= ob->playChance) return;
        auto* oc = pickClip(*ob, rng);
        if (!oc || !oc->audioBuffer) return;
        auto trimmed = trimBuffer(*oc->audioBuffer, oc->startMark, oc->endMark);
        if (!trimmed) return;
        result.entries.add({
            trimmed,
            0, (int64_t)trimmed->getNumSamples(), oc->startMark, oc->retainTailTempo,
            oc->name, oc->id,
            overlayStart, 1.0f, ob->id, true
        });
    };

    for (auto& slot : slots) {
        if (songEnded) break;

        // Separate overlapping (layer-on-top) from normal blocks
        std::vector<Block*> normal, overlapping;
        for (auto* b : slot.blocks) {
            if (b->isOverlapping) overlapping.push_back(b);
            else                  normal.push_back(b);
        }
        if (normal.empty())
        {
            // No non-overlapping blocks in this slot.  If there are overlapping
            // blocks, attach them to the most recent primary entry already
            // in the arrangement (handles standalone isOverlapping blocks
            // whose stackGroup was never set, i.e. stackGroup == -1).
            if (!overlapping.empty())
            {
                if (!result.entries.isEmpty())
                {
                    // Find the last non-overlapping entry's timeline position.
                    int64_t overlayStart = -1;
                    for (int i = result.entries.size() - 1; i >= 0; --i)
                    {
                        const auto& e = result.entries.getReference(i);
                        auto it = blockById.find(e.blockId.toStdString());
                        bool isOver = (it != blockById.end() && it->second->isOverlapping);
                        if (!isOver) { overlayStart = e.timelinePos; break; }
                    }
                    if (overlayStart >= 0)
                    {
                        for (auto* ob : overlapping)
                            addOverlay(ob, overlayStart);
                    }
                }
                else
                {
                    // No primary entries exist yet — this overlapping block comes before any
                    // normal block in position order.  Defer it to the first primary entry.
                    for (auto* ob : overlapping)
                        deferredStandaloneOverlays.push_back(ob);
                }
            }
            continue;
        }

        if (normal.size() == 1 && overlapping.empty()) {
            // ── Simple standalone block ───────────────────────────────────
            auto* block = normal[0];
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

            // Fire any overlapping blocks that were positioned before this entry in the
            // arrangement order and had to be deferred until the first primary entry appeared.
            if (!deferredStandaloneOverlays.empty()) {
                for (auto* ob : deferredStandaloneOverlays)
                    addOverlay(ob, tPos);
                deferredStandaloneOverlays.clear();
            }

            cursor += bodyLen;
            if (clip->isSongEnder) songEnded = true;

        } else {
            // ── Stack slot ────────────────────────────────────────────────
            // Pick how many normal blocks to play
            int playCount = 1;
            if (normal[0]->stackPlayCount.isValid())
                playCount = normal[0]->stackPlayCount.pick(rng);
            playCount = juce::jlimit(1, (int)normal.size(), playCount);

            // Sample playCount blocks from normal pool with weighted probability
            std::vector<Block*> picked;
            std::vector<Block*> pool = normal;
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
                (normal[0]->stackPlayMode == StackPlayMode::Simultaneous);

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

                juce::Array<Clip*> simultaneousClips;
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
                    simultaneousClips.add(clip);
                    if (clip->isSongEnder) { songEnded = true; break; }  // FIX C3: exit inner loop
                }

                if (bodyStart < 0) bodyStart = cursor;  // all clips were empty/invalid

                // Fire any deferred standalone overlays at this first primary entry.
                if (!deferredStandaloneOverlays.empty()) {
                    for (auto* ob : deferredStandaloneOverlays)
                        addOverlay(ob, bodyStart < 0 ? cursor : bodyStart);
                    deferredStandaloneOverlays.clear();
                }

                // Overlapping blocks layer on top of this slot (timelinePos = bodyStart)
                for (auto* ob : overlapping) {
                    if (!ob->allowedParentClipIds.isEmpty()) {
                        bool anyAllowed = false;
                        for (auto* sc : simultaneousClips)
                            if (ob->allowedParentClipIds.contains(sc->id)) { anyAllowed = true; break; }
                        if (!anyAllowed) continue;
                    }
                    addOverlay(ob, bodyStart);
                }
                cursor += maxBodyLen;

            } else {
                // Sequential: each picked block occupies its own time slot.
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

                    // Fire any deferred standalone overlays at this first primary entry.
                    if (!deferredStandaloneOverlays.empty()) {
                        for (auto* ob : deferredStandaloneOverlays)
                            addOverlay(ob, bodyStart);
                        deferredStandaloneOverlays.clear();
                    }

                    // Layer overlapping blocks on top of this picked block (bodies aligned).
                    for (auto* ob : overlapping) {
                        if (!ob->allowedParentClipIds.isEmpty() &&
                            !ob->allowedParentClipIds.contains(clip->id))
                        {
                            continue;
                        }
                        addOverlay(ob, bodyStart);
                    }
                    cursor += bodyLen;
                    if (clip->isSongEnder) { songEnded = true; break; }  // FIX C3: exit inner loop
                }
            }
        }
    }

    // ── Post-process: compute tempo-stretch ratios between adjacent primary entries ──
    // "Primary" = non-overlapping blocks. Simultaneous entries sharing the same
    // timelinePos are skipped (no stretch between entries in the same slot).
    {
        // Collect primary (non-overlapping, non-simultaneous) entry indices in order
        std::vector<int> primary;
        for (int i = 0; i < result.entries.size(); ++i) {
            const auto& e = result.entries.getReference(i);
            auto it = blockById.find(e.blockId.toStdString());
            if (it != blockById.end() && !it->second->isOverlapping)
                primary.push_back(i);
        }

        for (size_t k = 0; k + 1 < primary.size(); ++k) {
            auto& entA = result.entries.getReference(primary[k]);
            auto& entB = result.entries.getReference(primary[k + 1]);

            // Skip overlay entries — they must never receive stretch ratios, or they
            // will be silenced by the WSOLA pre-computation pass (which skips isOverlay).
            if (entA.isOverlay || entB.isOverlay) continue;

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
        if (entry.isOverlay || !entry.audioBuffer) continue;  // overlay entries play at original tempo; no stretching
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

    // Extend total duration to include the (stretched) tail of the last primary entry
    if (!result.entries.isEmpty()) {
        // Find last non-overlapping entry index
        int lastIdx = result.entries.size() - 1;
        for (int i = result.entries.size() - 1; i >= 0; --i) {
            const auto& e = result.entries.getReference(i);
            auto it = blockById.find(e.blockId.toStdString());
            bool isOver = (it != blockById.end() && it->second->isOverlapping);
            if (!isOver) { lastIdx = i; break; }
        }
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
