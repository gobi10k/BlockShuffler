#include "Serialization.h"
#include "Project.h"
#include "../Utils/UuidGenerator.h"

namespace BlockShuffler {
namespace Serialization {

juce::var projectToJSON(const Project& project) {
    auto* root = new juce::DynamicObject();
    root->setProperty("version", 1);
    root->setProperty("name", project.name);
    root->setProperty("sampleRate", project.sampleRate);

    // Blocks
    juce::Array<juce::var> blocksArray;
    for (auto* block : project.blocks) {
        auto* bObj = new juce::DynamicObject();
        bObj->setProperty("id",               block->id);
        bObj->setProperty("name",             block->name);
        bObj->setProperty("color",            block->color.toString());
        bObj->setProperty("position",         block->position);
        bObj->setProperty("stackGroup",       block->stackGroup);
        bObj->setProperty("stackPlayMode",    block->stackPlayMode == StackPlayMode::Sequential
                                                  ? "sequential" : "simultaneous");
        bObj->setProperty("isOverlapping",    block->isOverlapping);
        bObj->setProperty("overlapProb",      (double)block->overlapProbability);
        bObj->setProperty("probability",      (double)block->probability);

        juce::Array<juce::var> apciArr;
        for (auto& s : block->allowedParentClipIds) apciArr.add(juce::var(s));
        bObj->setProperty("allowedParentClipIds", juce::var(apciArr));

        bObj->setProperty("isDone",           block->isDone);
        bObj->setProperty("tempo",            block->tempo);

        // stackPlayCount
        auto* spcObj = new juce::DynamicObject();
        juce::Array<juce::var> spcVals, spcWts;
        for (auto v : block->stackPlayCount.values)  spcVals.add(v);
        for (auto w : block->stackPlayCount.weights) spcWts.add((double)w);
        spcObj->setProperty("values",  spcVals);
        spcObj->setProperty("weights", spcWts);
        bObj->setProperty("stackPlayCount", juce::var(spcObj));

        // Clips
        juce::Array<juce::var> clipsArray;
        for (auto* clip : block->clips) {
            auto* cObj = new juce::DynamicObject();
            cObj->setProperty("id",                 clip->id);
            cObj->setProperty("name",               clip->name);
            cObj->setProperty("color",              clip->color.toString());
            cObj->setProperty("audioFile",          clip->audioFile.getFullPathName());
            cObj->setProperty("nativeSampleRate",   clip->nativeSampleRate);
            // Store large int as string to avoid double precision loss
            cObj->setProperty("startMark",          juce::String(clip->startMark));
            cObj->setProperty("endMark",            juce::String(clip->endMark));
            cObj->setProperty("probability",        (double)clip->probability);
            cObj->setProperty("tempo",              clip->tempo);
            cObj->setProperty("retainLeadInTempo",  clip->retainLeadInTempo);
            cObj->setProperty("retainTailTempo",    clip->retainTailTempo);
            cObj->setProperty("isSongEnder",        clip->isSongEnder);
            cObj->setProperty("isDone",             clip->isDone);
            cObj->setProperty("gridOffsetSamples",  juce::String(clip->gridOffsetSamples));
            clipsArray.add(juce::var(cObj));
        }
        bObj->setProperty("clips", clipsArray);
        blocksArray.add(juce::var(bObj));
    }
    root->setProperty("blocks", blocksArray);

    // Links
    juce::Array<juce::var> linksArray;
    for (auto* link : project.links) {
        auto* lObj = new juce::DynamicObject();
        lObj->setProperty("blockA",        link->blockA);
        lObj->setProperty("blockB",        link->blockB);
        lObj->setProperty("swapProbability", (double)link->swapProbability);
        linksArray.add(juce::var(lObj));
    }
    root->setProperty("links", linksArray);

    return juce::var(root);
}

bool projectFromJSON(const juce::var& json, Project& project) {
    if (!json.isObject()) return false;

    project.name       = json.getProperty("name",       "Untitled").toString();
    project.sampleRate = (double)json.getProperty("sampleRate", 48000.0);

    // Blocks
    if (auto* blocksArr = json.getProperty("blocks", juce::var()).getArray()) {
        for (auto& bVar : *blocksArr) {
            auto block = std::make_unique<Block>();
            block->id             = bVar.getProperty("id",   generateUuid()).toString();
            block->name           = bVar.getProperty("name", "Block").toString();
            block->color          = juce::Colour::fromString(
                                        bVar.getProperty("color", "FF5599FF").toString());
            block->position       = (int)   bVar.getProperty("position",    0);
            block->stackGroup     = (int)   bVar.getProperty("stackGroup",  -1);
            block->stackPlayMode  = bVar.getProperty("stackPlayMode", "sequential")
                                        .toString() == "simultaneous"
                                        ? StackPlayMode::Simultaneous
                                        : StackPlayMode::Sequential;
            block->isOverlapping  = (bool)  bVar.getProperty("isOverlapping",  false);
            block->overlapProbability = (float)(double)bVar.getProperty("overlapProb", 0.5);
            block->probability    = (float)(double)bVar.getProperty("probability", 1.0);

            block->allowedParentClipIds.clear();
            if (auto* apciArr = bVar.getProperty("allowedParentClipIds", juce::var()).getArray())
                for (auto& v : *apciArr) block->allowedParentClipIds.add(v.toString());

            block->isDone         = (bool)  bVar.getProperty("isDone",         false);
            block->tempo          = (double)bVar.getProperty("tempo",          120.0);

            // stackPlayCount
            auto spcVar = bVar.getProperty("stackPlayCount", juce::var());
            if (spcVar.isObject()) {
                block->stackPlayCount.values.clear();
                block->stackPlayCount.weights.clear();
                if (auto* vals = spcVar.getProperty("values",  juce::var()).getArray())
                    for (auto& v : *vals) block->stackPlayCount.values.add((int)v);
                if (auto* wts  = spcVar.getProperty("weights", juce::var()).getArray())
                    for (auto& w : *wts)  block->stackPlayCount.weights.add((float)(double)w);
            }

            // Clips
            if (auto* clipsArr = bVar.getProperty("clips", juce::var()).getArray()) {
                for (auto& cVar : *clipsArr) {
                    auto clip = std::make_unique<Clip>();
                    clip->id               = cVar.getProperty("id",   generateUuid()).toString();
                    clip->name             = cVar.getProperty("name", "Clip").toString();
                    clip->color            = juce::Colour::fromString(
                                                cVar.getProperty("color", "FFFFFFFF").toString());
                    clip->audioFile        = juce::File(cVar.getProperty("audioFile", "").toString());
                    clip->probability      = (float)(double)cVar.getProperty("probability",     1.0);
                    clip->tempo            = (double)cVar.getProperty("tempo",            120.0);
                    clip->retainLeadInTempo= (bool)cVar.getProperty("retainLeadInTempo",  false);
                    clip->retainTailTempo  = (bool)cVar.getProperty("retainTailTempo",    false);
                    clip->isSongEnder      = (bool)cVar.getProperty("isSongEnder",        false);
                    clip->isDone           = (bool)cVar.getProperty("isDone",             false);

                    // Load audio (sets nativeSampleRate and resamples buffer to project SR)
                    if (clip->audioFile.existsAsFile()) {
                        clip->loadFromFile(clip->audioFile, project.formatManager,
                                           project.sampleRate);
                    } else if (clip->audioFile != juce::File{}) {
                        project.missingFilesOnLoad.add(clip->audioFile.getFullPathName());
                    }

                    // Compute scale from saved native SR to current project SR.
                    // Priority:
                    // 1. nativeSampleRate field from JSON (if present and > 0)
                    // 2. clip->nativeSampleRate (if file loaded successfully)
                    // 3. Fallback to project.sampleRate (scale = 1.0)
                    double savedNativeSR = 0.0;
                    if (cVar.hasProperty("nativeSampleRate"))
                        savedNativeSR = (double)cVar.getProperty("nativeSampleRate", 0.0);

                    if (savedNativeSR <= 0.0)
                        savedNativeSR = clip->nativeSampleRate;

                    if (savedNativeSR <= 0.0)
                        savedNativeSR = project.sampleRate;

                    double markScale = (savedNativeSR > 0.0 && project.sampleRate > 0.0
                                        && std::abs(savedNativeSR - project.sampleRate) > 0.5)
                                       ? project.sampleRate / savedNativeSR
                                       : 1.0;

                    clip->startMark = (int64_t)(
                        cVar.getProperty("startMark", "0").toString().getLargeIntValue()
                        * markScale + 0.5);
                    clip->endMark   = (int64_t)(
                        cVar.getProperty("endMark", "0").toString().getLargeIntValue()
                        * markScale + 0.5);
                    clip->gridOffsetSamples = (int64_t)(
                        cVar.getProperty("gridOffsetSamples", "0").toString().getLargeIntValue()
                        * markScale + 0.5);

                    // Clamp marks to buffer bounds (handles missing-file case gracefully)
                    int bufLen = clip->audioBuffer ? clip->audioBuffer->getNumSamples() : 0;
                    clip->startMark = juce::jlimit((int64_t)0, (int64_t)juce::jmax(0, bufLen - 1), clip->startMark);
                    clip->endMark   = juce::jlimit(clip->startMark, (int64_t)bufLen, clip->endMark);

                    block->clips.add(clip.release());
                }
            }

            project.blocks.add(block.release());
        }
    }

    // Links
    if (auto* linksArr = json.getProperty("links", juce::var()).getArray()) {
        for (auto& lVar : *linksArr) {
            project.addLink(
                lVar.getProperty("blockA", "").toString(),
                lVar.getProperty("blockB", "").toString(),
                (float)(double)lVar.getProperty("swapProbability", 0.5f),
                false);
        }
    }

    return true;
}

juce::var exportManifestToJSON(const Project& project) {
    auto* obj = new juce::DynamicObject();
    obj->setProperty("version",    1);
    obj->setProperty("title",      project.name);
    obj->setProperty("sampleRate", project.sampleRate);
    obj->setProperty("clips",      juce::Array<juce::var>());
    obj->setProperty("arrangement",juce::Array<juce::var>());
    return juce::var(obj);
}

} // namespace Serialization
} // namespace BlockShuffler
