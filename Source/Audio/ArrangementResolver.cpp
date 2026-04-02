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
                    bool isOver = false;
                    for (auto* b : project.blocks)
                        if (b->id == e.blockId) { isOver = b->isOverlapping; break; }
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
                                result.entries.add({oc, overlayStart, 1.0f, ob->id, true});
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

            result.entries.add({clip, cursor, 1.0f, block->id});
            cursor += bodyLen;
            if (clip->isSongEnder) songEnded = true;

        } else {
            // ── Stack slot ────────────────────────────────────────────────
            // Pick how many normal blocks to play
            int playCount = 1;
            if (normal[0]->stackPlayCount.isValid())
                playCount = normal[0]->stackPlayCount.pick(rng);
            playCount = juce::jlimit(1, (int)normal.size(), playCount);

            // Sample playCount blocks from normal pool with equal probability
            std::vector<size_t> indices(normal.size());
            std::iota(indices.begin(), indices.end(), 0);
            for (int i = (int)indices.size() - 1; i > 0; --i) {
                int j = rng.nextInt(i + 1);
                std::swap(indices[(size_t)i], indices[(size_t)j]);
            }

            std::vector<Block*> picked;
            for (int k = 0; k < playCount; ++k)
                picked.push_back(normal[indices[(size_t)k]]);

            const bool isSimultaneous =
                (normal[0]->stackPlayMode == StackPlayMode::Simultaneous);

            if (isSimultaneous) {
                // All picked blocks start at the same timeline position
                const int64_t slotStart = cursor;
                int64_t maxLen = 0;

                // Collect picked clips so overlapping-block targeting can check them
                juce::Array<Clip*> simultaneousClips;
                for (auto* b : picked) {
                    if (b->clips.isEmpty()) continue;
                    auto* clip = pickClip(*b, rng);
                    if (!clip) continue;
                    int64_t bodyLen = clip->endMark - clip->startMark;
                    if (bodyLen <= 0) continue;
                    result.entries.add({clip, slotStart, 1.0f, b->id});
                    maxLen = std::max(maxLen, bodyLen);
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
                            result.entries.add({clip, slotStart, 1.0f, ob->id, true});
                        }
                    }
                }
                cursor += maxLen;

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
                    result.entries.add({clip, entryStart, 1.0f, b->id});
                    cursor += bodyLen;

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
                                result.entries.add({oc, entryStart, 1.0f, ob->id, true});
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
        std::unordered_map<std::string, const Block*> blockById;
        for (auto* b : project.blocks)
            blockById[b->id.toStdString()] = b;

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

            const Clip* clipA = entA.clip;
            const Clip* clipB = entB.clip;

            if (!clipA->retainTailTempo && clipA->tempo > 0.0 && clipB->tempo > 0.0)
                entA.tailStretchRatio = (float)(clipA->tempo / clipB->tempo);

            if (!clipB->retainLeadInTempo && clipA->tempo > 0.0 && clipB->tempo > 0.0)
                entB.leadInStretchRatio = (float)(clipB->tempo / clipA->tempo);
        }
    }

    // Pre-compute WSOLA-stretched buffers for entries that need it.
    for (int i = 0; i < result.entries.size(); ++i)
    {
        auto& entry = result.entries.getReference(i);
        if (entry.isOverlay) continue;  // overlay entries play at original tempo; no stretching
        const auto& buf = entry.clip->audioBuffer;
        const int64_t leadInLen = entry.clip->startMark;
        const int64_t tailLen   = juce::jmax((int64_t)0,
                                     (int64_t)buf.getNumSamples() - entry.clip->endMark);

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
            auto stretched = TempoStretcher::stretch(buf, (int)entry.clip->endMark,
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
            bool isOver = false;
            for (auto* b : project.blocks)
                if (b->id == e.blockId) { isOver = b->isOverlapping; break; }
            if (!isOver) { lastIdx = i; break; }
        }
        const ResolvedEntry& last = result.entries.getReference(lastIdx);
        int64_t tailLen = juce::jmax((int64_t)0,
                                     (int64_t)last.clip->audioBuffer.getNumSamples()
                                     - last.clip->endMark);
        // Use the pre-stretched buffer's actual length if it was computed
        int64_t tailTL = last.stretchedTail
                       ? (int64_t)last.stretchedTail->getNumSamples()
                       : (int64_t)(tailLen * last.tailStretchRatio + 0.5f);
        cursor += tailTL;
    }

    result.totalDurationSamples = cursor;

    DBG("========== RESOLVED ARRANGEMENT ==========");
    DBG("Project sampleRate: " + juce::String(project.sampleRate));
    DBG("Blocks in project: " + juce::String(project.blocks.size()));
    for (auto* b : project.blocks)
        DBG("  Block: " + b->name + " pos=" + juce::String(b->position)
            + " clips=" + juce::String(b->clips.size())
            + " isDone=" + juce::String(b->isDone ? 1 : 0)
            + " stackGroup=" + juce::String(b->stackGroup)
            + " isOverlapping=" + juce::String(b->isOverlapping ? 1 : 0));
    DBG("---");
    DBG("Resolved entries: " + juce::String((int)result.entries.size()));
    for (int i = 0; i < (int)result.entries.size(); ++i) {
        auto& e = result.entries.getReference(i);
        auto bodyLen = e.clip ? (e.clip->endMark - e.clip->startMark) : 0;
        DBG("  [" + juce::String(i) + "] block=" + e.blockId
            + " clip=" + (e.clip ? e.clip->name : "NULL")
            + " timeline=" + juce::String((int64_t)e.timelinePos)
            + " body=" + juce::String((int64_t)bodyLen)
            + " overlay=" + juce::String(e.isOverlay ? 1 : 0)
            + " startMark=" + juce::String(e.clip ? (int64_t)e.clip->startMark : 0)
            + " endMark=" + juce::String(e.clip ? (int64_t)e.clip->endMark : 0)
            + " clipSR=" + juce::String(e.clip ? e.clip->nativeSampleRate : 0)
            + " bufferLen=" + juce::String(e.clip ? (int64_t)e.clip->audioBuffer.getNumSamples() : 0));
    }
    DBG("totalDuration=" + juce::String((int64_t)result.totalDurationSamples));
    DBG("===========================================");

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
