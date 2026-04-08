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

    DBG("=== Blocks after sort ===");
    for (auto* block : sorted)
        DBG("Block: " + block->name + " pos=" + juce::String(block->position)
            + " clips=" + juce::String(block->clips.size())
            + " isDone=" + juce::String(block->isDone ? 1 : 0));

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

    DBG("=== Slots after grouping ===");
    for (size_t i = 0; i < slots.size(); ++i) {
        juce::String names;
        for (auto* b : slots[i].blocks) names += b->name + " ";
        DBG("Slot " + juce::String(i) + ": [" + names + "]");
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
            // No non-overlapping blocks in this slot.  If there are overlapping
            // blocks, attach them to the most recent primary entry already
            // in the arrangement (handles standalone isOverlapping blocks
            // whose stackGroup was never set, i.e. stackGroup == -1).
            if (!overlapping.empty() && !result.entries.isEmpty())
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
                    {
                        DBG("OVERLAP CHECK: block=" + ob->name
                            + " isOverlapping=" + juce::String(ob->isOverlapping ? 1 : 0)
                            + " overlapProb=" + juce::String(ob->overlapProbability)
                            + " numClips=" + juce::String(ob->clips.size()));
                        if (ob->isDone || ob->clips.isEmpty()) continue;
                        float roll = rng.nextFloat();
                        DBG("Overlap block (standalone): " + ob->name
                            + " prob=" + juce::String(ob->overlapProbability)
                            + " rolled=" + juce::String(roll)
                            + " triggered=" + juce::String(roll < ob->overlapProbability ? 1 : 0));
                        if (roll < ob->overlapProbability)
                        {
                            auto* oc = pickClip(*ob, rng);
                            if (oc && oc->endMark > oc->startMark) {
                                DBG("OVERLAY ENTRY ADDED: clip=" + oc->name
                                    + " timelinePos=" + juce::String(overlayStart)
                                    + " isOverlay=true");
                                result.entries.add({
                                    oc->audioBuffer,
                                    oc->startMark, oc->endMark, oc->retainTailTempo,
                                    oc->name, oc->id,
                                    overlayStart, 1.0f, ob->id, true
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
            // Advance cursor by endMark (= startMark + bodyLen) so that the NEXT
            // block's lead-in starts at this block's body-end (= tail start).
            // This makes tail of N and lead-in of N+1 overlap at the transition point.
            cursor += clip->endMark;
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
                // All picked blocks start at the same timeline position
                const int64_t slotStart = cursor;
                const float stackGain = 1.0f / (float)juce::jmax(1, (int)picked.size());

                // Collect picked clips so overlapping-block targeting can check them
                juce::Array<Clip*> simultaneousClips;
                int64_t maxEndMark = 0;
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
                    maxEndMark = std::max(maxEndMark, (int64_t)clip->endMark);
                    simultaneousClips.add(clip);
                    if (clip->isSongEnder) songEnded = true;
                }

                // Overlapping blocks layer on top of this slot
                for (auto* ob : overlapping) {
                    DBG("OVERLAP CHECK: block=" + ob->name
                        + " isOverlapping=" + juce::String(ob->isOverlapping ? 1 : 0)
                        + " overlapProb=" + juce::String(ob->overlapProbability)
                        + " numClips=" + juce::String(ob->clips.size()));
                    if (ob->isDone || ob->clips.isEmpty()) continue;
                    // Clip targeting: allow only if at least one simultaneous clip is in the allowed list
                    if (!ob->allowedParentClipIds.isEmpty()) {
                        bool anyAllowed = false;
                        for (auto* sc : simultaneousClips)
                            if (ob->allowedParentClipIds.contains(sc->id)) { anyAllowed = true; break; }
                        if (!anyAllowed) {
                            DBG("Overlap block skipped (clip targeting): " + ob->name);
                            continue;
                        }
                    }
                    if (rng.nextFloat() < ob->overlapProbability) {
                        auto* clip = pickClip(*ob, rng);
                        if (clip && clip->endMark > clip->startMark) {
                            DBG("OVERLAY ENTRY ADDED: clip=" + clip->name
                                + " timelinePos=" + juce::String(slotStart)
                                + " isOverlay=true");
                            result.entries.add({
                                clip->audioBuffer,
                                clip->startMark, clip->endMark, clip->retainTailTempo,
                                clip->name, clip->id,
                                slotStart, 1.0f, ob->id, true
                            });
                        }
                    }
                }
                cursor += maxEndMark;  // advance by max(endMark) so next lead-in overlaps at tail start

            } else {
                // Sequential: each picked block occupies its own time slot.
                // Each picked block can have overlapping layers on top of it.
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
                    cursor += clip->endMark;  // advance by endMark so next lead-in overlaps at tail start

                    // Layer overlapping blocks on top of this picked block
                    for (auto* ob : overlapping) {
                        DBG("OVERLAP CHECK: block=" + ob->name
                            + " isOverlapping=" + juce::String(ob->isOverlapping ? 1 : 0)
                            + " overlapProb=" + juce::String(ob->overlapProbability)
                            + " numClips=" + juce::String(ob->clips.size()));
                        if (ob->isDone || ob->clips.isEmpty()) continue;
                        // Clip targeting: skip if this overlay isn't allowed over the selected parent clip
                        if (!ob->allowedParentClipIds.isEmpty() &&
                            !ob->allowedParentClipIds.contains(clip->id))
                        {
                            DBG("Overlap block skipped (clip targeting): " + ob->name
                                + " parentClip=" + clip->id);
                            continue;
                        }
                        float roll = rng.nextFloat();
                        DBG("Overlap block: " + ob->name
                            + " prob=" + juce::String(ob->overlapProbability)
                            + " rolled=" + juce::String(roll)
                            + " triggered=" + juce::String(roll < ob->overlapProbability ? 1 : 0));
                        if (roll < ob->overlapProbability) {
                            auto* oc = pickClip(*ob, rng);
                            if (oc && oc->endMark > oc->startMark) {
                                DBG("OVERLAY ENTRY ADDED: clip=" + oc->name
                                    + " timelinePos=" + juce::String(entryStart)
                                    + " isOverlay=true");
                                result.entries.add({
                                    oc->audioBuffer,
                                    oc->startMark, oc->endMark, oc->retainTailTempo,
                                    oc->name, oc->id,
                                    entryStart, 1.0f, ob->id, true
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

            // Actually, we can get tempos from the project during resolve()
            auto* bA = blockById.find(entA.blockId.toStdString())->second;
            auto* bB = blockById.find(entB.blockId.toStdString())->second;
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

    DBG("=== Resolved Arrangement ===");
    DBG("Total entries: " + juce::String(result.entries.size()));
    for (int i = 0; i < result.entries.size(); ++i) {
        auto& e = result.entries.getReference(i);
        DBG("Entry " + juce::String(i)
            + " blockId=" + e.blockId
            + " clipName=" + e.clipName
            + " timelinePos=" + juce::String(e.timelinePos)
            + " bodyLen=" + juce::String(e.endMark - e.startMark));
    }
    DBG("totalDurationSamples: " + juce::String(result.totalDurationSamples));

    DBG("=== TIMELINE CHECK ===");
    // New model: timelinePos = lead-in start; bodyStart = timelinePos + startMark;
    // tail starts at timelinePos + endMark. Adjacent entries' timelinePos should
    // equal prev's (timelinePos + endMark), i.e. gap = 0 between prev tail-start and next lead-in-start.
    for (int i = 0; i < (int)result.entries.size(); ++i) {
        auto& e = result.entries.getReference(i);
        auto bodyStart2 = e.timelinePos + e.startMark;
        auto bodyEnd2   = e.timelinePos + e.endMark;
        auto leadIn2    = e.startMark;
        DBG("[" + juce::String(i) + "] leadInStart=" + juce::String((int64_t)e.timelinePos)
            + " bodyStart=" + juce::String((int64_t)bodyStart2)
            + " bodyEnd=" + juce::String((int64_t)bodyEnd2)
            + " leadIn=" + juce::String((int64_t)leadIn2));
        if (i > 0) {
            auto& prev = result.entries.getReference(i-1);
            auto prevTailStart = prev.timelinePos + prev.endMark;
            DBG("  prevTailStart=" + juce::String((int64_t)prevTailStart)
                + " thisLeadInStart=" + juce::String((int64_t)e.timelinePos)
                + " gap=" + juce::String((int64_t)(e.timelinePos - prevTailStart)));
            if (e.timelinePos != prevTailStart)
                DBG("  *** TAIL/LEAD-IN ALIGNMENT ERROR ***");
        }
    }

    return result;
}

Clip* ArrangementResolver::pickClip(const Block& block, juce::Random& rng) {
    if (block.clips.isEmpty()) return nullptr;

    // Filter out done clips
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
