#include "ArrangementResolver.h"
#include "TempoStretcher.h"
#include <algorithm>
#include <numeric>
#include <vector>
#include <unordered_map>

namespace BlockShuffler {

ResolvedArrangement ArrangementResolver::resolve(const Project& project,
                                                  juce::Random& rng) const {
    ResolvedArrangement result;
    result.sampleRate = project.sampleRate;

    // ── 1. Collect blocks + build mutable position map ───────────────────────
    std::vector<Block*> sorted;
    sorted.reserve((size_t)project.blocks.size());
    for (auto* b : project.blocks) sorted.push_back(b);

    std::unordered_map<std::string, int> posMap;
    std::unordered_map<std::string, const Block*> blockById;
    for (auto* b : project.blocks)
        blockById[b->id.toStdString()] = b;

    for (auto* b : sorted) posMap[b->id.toStdString()] = b->position;

    // ── 2. Shuffle links and apply swaps ─────────────────────────────────────
    std::vector<BlockLink*> shuffledLinks;
    shuffledLinks.reserve((size_t)project.links.size());
    for (auto* lnk : project.links) shuffledLinks.push_back(lnk);

    for (int i = (int)shuffledLinks.size() - 1; i > 0; --i) {
        int j = rng.nextInt(i + 1);
        std::swap(shuffledLinks[(size_t)i], shuffledLinks[(size_t)j]);
    }
    for (auto* lnk : shuffledLinks) {
        if (rng.nextFloat() < lnk->swapProbability)
            std::swap(posMap[lnk->blockA.toStdString()],
                      posMap[lnk->blockB.toStdString()]);
    }

    // ── 3. Sort by resolved positions ────────────────────────────────────────
    std::sort(sorted.begin(), sorted.end(), [&posMap](Block* a, Block* b) {
        return posMap[a->id.toStdString()] < posMap[b->id.toStdString()];
    });

    // ── 4. Group into slots by stackGroup ────────────────────────────────────
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
    int64_t cursor   = 0;
    bool    songEnded = false;

    for (auto& slot : slots) {
        if (songEnded) break;

        // Separate overlapping (layer-on-top) from normal blocks
        std::vector<Block*> normal, overlapping;
        for (auto* b : slot.blocks) {
            if (b->isOverlapping) overlapping.push_back(b);
            else                  normal.push_back(b);
        }
        // Remove done normal blocks
        normal.erase(std::remove_if(normal.begin(), normal.end(),
            [](Block* b){ return b->isDone; }), normal.end());

        if (normal.empty())
        {
            // No non-overlapping blocks in this slot.  Attach overlaying blocks
            // to the most recent primary entry already in the arrangement.
            if (!overlapping.empty() && !result.entries.isEmpty())
            {
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
                    {
                        if (ob->isDone || ob->clips.isEmpty()) continue;
                        if (rng.nextFloat() < ob->overlapProbability)
                        {
                            auto* oc = pickClip(*ob, rng);
                            if (oc && oc->endMark > oc->startMark) {
                                result.entries.add({
                                    oc->audioBuffer,
                                    oc->startMark, oc->endMark, oc->retainTailTempo,
                                    oc->name, oc->id,
                                    overlayStart, 0.7f, ob->id, true
                                });
                            }
                        }
                    }
                }
            }
            continue;
        }

        if (normal.size() == 1 && overlapping.empty()) {
            // ── Simple standalone block ───────────────────────────────────
            auto* block = normal[0];
            if (block->clips.isEmpty()) continue;
            auto* clip = pickClip(*block, rng);
            if (!clip) continue;
            int64_t bodyLen = clip->endMark - clip->startMark;
            if (bodyLen <= 0) continue;

            result.entries.add({
                clip->audioBuffer,
                clip->startMark, clip->endMark, clip->retainTailTempo,
                clip->name, clip->id,
                cursor, 1.0f, block->id
            });
            cursor += bodyLen;
            if (clip->isSongEnder) songEnded = true;

        } else {
            // ── Stack slot ────────────────────────────────────────────────
            int playCount = 1;
            if (normal[0]->stackPlayCount.isValid())
                playCount = normal[0]->stackPlayCount.pick(rng);
            playCount = juce::jlimit(1, (int)normal.size(), playCount);

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
                const int64_t slotStart = cursor;
                int64_t maxLen = 0;

                const float stackGain = 1.0f / (float)juce::jmax(1, (int)picked.size());

                juce::Array<Clip*> simultaneousClips;
                for (auto* b : picked) {
                    if (b->clips.isEmpty()) continue;
                    auto* clip = pickClip(*b, rng);
                    if (!clip) continue;
                    int64_t bodyLen = clip->endMark - clip->startMark;
                    if (bodyLen <= 0) continue;
                    result.entries.add({
                        clip->audioBuffer,
                        clip->startMark, clip->endMark, clip->retainTailTempo,
                        clip->name, clip->id,
                        slotStart, stackGain, b->id
                    });
                    maxLen = std::max(maxLen, bodyLen);
                    simultaneousClips.add(clip);
                    if (clip->isSongEnder) songEnded = true;
                }

                // Overlapping blocks layer on top of this slot at 0.7 gain
                for (auto* ob : overlapping) {
                    if (ob->isDone || ob->clips.isEmpty()) continue;
                    if (!ob->allowedParentClipIds.isEmpty()) {
                        bool anyAllowed = false;
                        for (auto* sc : simultaneousClips)
                            if (ob->allowedParentClipIds.contains(sc->id)) { anyAllowed = true; break; }
                        if (!anyAllowed) continue;
                    }
                    if (rng.nextFloat() < ob->overlapProbability) {
                        auto* clip = pickClip(*ob, rng);
                        if (clip && clip->endMark > clip->startMark) {
                            result.entries.add({
                                clip->audioBuffer,
                                clip->startMark, clip->endMark, clip->retainTailTempo,
                                clip->name, clip->id,
                                slotStart, 0.7f, ob->id, true
                            });
                        }
                    }
                }
                cursor += maxLen;

            } else {
                // Sequential: each picked block occupies its own time slot
                for (auto* b : picked) {
                    if (songEnded) break;
                    if (b->clips.isEmpty()) continue;
                    auto* clip = pickClip(*b, rng);
                    if (!clip) continue;
                    int64_t bodyLen = clip->endMark - clip->startMark;
                    if (bodyLen <= 0) continue;

                    const int64_t entryStart = cursor;
                    result.entries.add({
                        clip->audioBuffer,
                        clip->startMark, clip->endMark, clip->retainTailTempo,
                        clip->name, clip->id,
                        entryStart, 1.0f, b->id
                    });
                    cursor += bodyLen;

                    // Layer overlapping blocks on top at 0.7 gain
                    for (auto* ob : overlapping) {
                        if (ob->isDone || ob->clips.isEmpty()) continue;
                        if (!ob->allowedParentClipIds.isEmpty() &&
                            !ob->allowedParentClipIds.contains(clip->id))
                            continue;
                        if (rng.nextFloat() < ob->overlapProbability) {
                            auto* oc = pickClip(*ob, rng);
                            if (oc && oc->endMark > oc->startMark) {
                                result.entries.add({
                                    oc->audioBuffer,
                                    oc->startMark, oc->endMark, oc->retainTailTempo,
                                    oc->name, oc->id,
                                    entryStart, 0.7f, ob->id, true
                                });
                            }
                        }
                    }
                    if (clip->isSongEnder) songEnded = true;
                }
            }
        }
    }

    // ── Post-process: compute tempo-stretch ratios between adjacent primary entries ──
    {
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

            if (entA.isOverlay || entB.isOverlay) continue;
            if (entA.timelinePos == entB.timelinePos) continue;  // simultaneous slot

            // FIX #14: guard both find() results before dereferencing
            auto itA = blockById.find(entA.blockId.toStdString());
            auto itB = blockById.find(entB.blockId.toStdString());
            if (itA == blockById.end() || itB == blockById.end()) continue;

            const Clip* cA = itA->second->getClipById(entA.clipId);
            const Clip* cB = itB->second->getClipById(entB.clipId);

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
        if (entry.isOverlay || !entry.audioBuffer) continue;
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

    // ── Compute total duration ────────────────────────────────────────────────
    // cursor already holds the sum of all primary body lengths.  We extend it by
    // the tail of the last primary entry, then check every overlay entry to see
    // if its body + tail extends past that value.
    //
    // FIX #1: null-check last.audioBuffer before dereferencing.
    // FIX #8: walk all entries (including overlays) to find the true end.
    if (!result.entries.isEmpty())
    {
        // Tail of the last primary entry
        int lastPrimaryIdx = -1;
        for (int i = result.entries.size() - 1; i >= 0; --i)
        {
            const auto& e = result.entries.getReference(i);
            auto it = blockById.find(e.blockId.toStdString());
            bool isOver = (it != blockById.end() && it->second->isOverlapping);
            if (!isOver) { lastPrimaryIdx = i; break; }
        }

        if (lastPrimaryIdx >= 0)
        {
            const ResolvedEntry& last = result.entries.getReference(lastPrimaryIdx);
            int64_t tailLen = 0;
            if (last.audioBuffer)
                tailLen = juce::jmax((int64_t)0,
                                     (int64_t)last.audioBuffer->getNumSamples() - last.endMark);
            const int64_t tailTL = last.stretchedTail
                                 ? (int64_t)last.stretchedTail->getNumSamples()
                                 : (int64_t)std::llround((double)tailLen * last.tailStretchRatio);
            cursor += tailTL;
        }

        // Check whether any overlay entry (body + tail) ends after the current cursor
        for (int i = 0; i < result.entries.size(); ++i)
        {
            const auto& e = result.entries.getReference(i);
            if (!e.isOverlay || !e.audioBuffer) continue;
            const int64_t eTailLen = juce::jmax((int64_t)0,
                                         (int64_t)e.audioBuffer->getNumSamples() - e.endMark);
            const int64_t eTailTL  = e.stretchedTail
                                   ? (int64_t)e.stretchedTail->getNumSamples()
                                   : eTailLen;  // overlays never get stretch ratios set
            const int64_t eEnd = e.timelinePos + (e.endMark - e.startMark) + eTailTL;
            if (eEnd > cursor) cursor = eEnd;
        }
    }

    result.totalDurationSamples = cursor;
    return result;
}

Clip* ArrangementResolver::pickClip(const Block& block, juce::Random& rng) {
    if (block.clips.isEmpty()) return nullptr;

    juce::Array<Clip*> available;
    for (auto* c : block.clips)
        if (!c->isDone) available.add(c);
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
