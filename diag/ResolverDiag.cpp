// STEP1 DIAG (temporary — remove in MASTER_PROMPT Step 7).
// Headless reproduction of the stack play-count regression:
// builds a 3-block stack through the same model APIs the UI uses
// (Project::addBlock / stackBlocks / propagateStackSettings) and calls the
// same ArrangementResolver::resolve() the transport Play button calls.
#include <iostream>
#include <map>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Model/Project.h"
#include "Audio/ArrangementResolver.h"
#include "Audio/ExportRenderer.h"
#include "Audio/StackPicker.h"

using namespace BlockShuffler;

static std::shared_ptr<juce::AudioBuffer<float>> makeBuf(int len = 1000) {
    auto b = std::make_shared<juce::AudioBuffer<float>>(2, len);
    b->clear();
    return b;
}

static void addClipTo(Block* blk, const juce::String& nm, int bodyLen = 1000) {
    auto c = std::make_unique<Clip>();
    c->name = nm;
    c->audioBuffer = makeBuf(bodyLen);
    c->startMark = 0;
    c->endMark = bodyLen;
    c->probability = 1.0f;
    blk->addClip(std::move(c));
}

static void runResolves(Project& p, ArrangementResolver& r, juce::Random& rng,
                        const char* label) {
    std::cout << "--- " << label << " ---\n";
    for (int i = 0; i < 10; ++i) {
        auto arr = r.resolve(p, rng);
        std::cout << "resolve#" << i << " entries=" << arr.entries.size() << " [";
        for (const auto& e : arr.entries) {
            auto* b = p.getBlockById(e.blockId);
            std::cout << (b ? b->name : juce::String("?")) << "@" << e.timelinePos << " ";
        }
        std::cout << "]\n";
    }
}

static void dumpModel(Project& p, const char* label) {
    std::cout << "=== model state: " << label << " ===\n";
    for (auto* blk : p.blocks) {
        std::cout << blk->name << " sg=" << blk->stackGroup
                  << " mode=" << (blk->stackPlayMode == StackPlayMode::Simultaneous ? "Sim" : "Seq")
                  << " spc.values=[";
        for (auto v : blk->stackPlayCount.values) std::cout << v << ",";
        std::cout << "] spc.weights=[";
        for (auto w : blk->stackPlayCount.weights) std::cout << w << ",";
        std::cout << "] playChance=" << blk->playChance
                  << " clips=" << blk->clips.size() << "\n";
    }
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;

    Project p;
    auto* a = p.addBlock("A");
    auto* b = p.addBlock("B");
    auto* c = p.addBlock("C");
    addClipTo(a, "cA");
    addClipTo(b, "cB");
    addClipTo(c, "cC");

    // Stack all three the way BlockStrip drag-to-stack does.
    p.stackBlocks(b->id, a->id);   // drag B onto A
    p.stackBlocks(c->id, a->id);   // drag C onto A

    dumpModel(p, "after stacking (default playCount)");

    juce::Random rng(12345);
    ArrangementResolver r;

    runResolves(p, r, rng, "SEQ playCount=1 (default)");

    a->stackPlayMode = StackPlayMode::Simultaneous;
    p.propagateStackSettings(a->stackGroup, a);
    runResolves(p, r, rng, "SIM playCount=1");

    // Inspector-equivalent: bump play count to 2 then back to 1
    a->stackPlayCount.values.set(0, 2);
    p.propagateStackSettings(a->stackGroup, a);
    runResolves(p, r, rng, "SIM playCount=2");

    a->stackPlayCount.values.set(0, 1);
    p.propagateStackSettings(a->stackGroup, a);
    runResolves(p, r, rng, "SIM playCount back to 1");

    // Undo-path equivalent: full JSON round-trip through resetAndLoad
    auto snap = p.toJSON();
    p.resetAndLoad(snap);
    for (auto* blk : p.blocks)
        for (auto* cl : blk->clips) {
            cl->audioBuffer = makeBuf();
            cl->startMark = 0;
            cl->endMark = 1000;
        }
    dumpModel(p, "after JSON round-trip (resetAndLoad)");
    runResolves(p, r, rng, "after JSON round-trip, playCount=1 SIM");

    // STEP2 DIAG (temporary — remove in MASTER_PROMPT Step 7).
    // Simultaneous layering + cursor advance: 3-block SIM stack, playCount=3,
    // DISTINCT body lengths (1000/2500/1800) so "cursor += longest body" is
    // observable, plus a trailing unstacked block D exposing the cursor.
    // Expect: A/B/C all at the SAME timelinePos, D at that pos + 2500.
    {
        std::cout << "\n=== STEP2: SIM playCount=3, bodies 1000/2500/1800, trailing D ===\n";
        Project p2;
        auto* a2 = p2.addBlock("A");
        auto* b2 = p2.addBlock("B");
        auto* c2 = p2.addBlock("C");
        auto* d2 = p2.addBlock("D");   // unstacked follower — reveals cursor advance
        addClipTo(a2, "cA", 1000);
        addClipTo(b2, "cB", 2500);     // longest body
        addClipTo(c2, "cC", 1800);
        addClipTo(d2, "cD", 700);
        p2.stackBlocks(b2->id, a2->id);
        p2.stackBlocks(c2->id, a2->id);
        a2->stackPlayMode = StackPlayMode::Simultaneous;
        a2->stackPlayCount.values.set(0, 3);
        p2.propagateStackSettings(a2->stackGroup, a2);
        dumpModel(p2, "STEP2 SIM playCount=3");

        juce::Random rng2(777);
        ArrangementResolver r2;
        for (int i = 0; i < 10; ++i) {
            auto arr = r2.resolve(p2, rng2);
            std::cout << "resolve#" << i << " entries=" << arr.entries.size() << " [";
            for (const auto& e : arr.entries) {
                auto* blk = p2.getBlockById(e.blockId);
                std::cout << (blk ? blk->name : juce::String("?"))
                          << "@" << e.timelinePos
                          << "(body=" << (e.endMark - e.startMark) << ") ";
            }
            std::cout << "]\n";
        }
    }

    // STEP3B DIAG (temporary — remove in MASTER_PROMPT Step 7).
    // alwaysPlayBase serialization round-trip + missing-key default.
    {
        std::cout << "\n=== STEP3B: alwaysPlayBase serialization ===\n";
        Project p3;
        auto* a3 = p3.addBlock("A");
        auto* b3 = p3.addBlock("B");
        addClipTo(a3, "cA");
        addClipTo(b3, "cB");
        p3.stackBlocks(b3->id, a3->id);
        a3->alwaysPlayBase = true;
        p3.propagateStackSettings(a3->stackGroup, a3);

        auto snap = p3.toJSON();
        p3.resetAndLoad(snap);
        for (auto* blk : p3.blocks)
            std::cout << "after save->load: " << blk->name
                      << " alwaysPlayBase=" << (blk->alwaysPlayBase ? "true" : "false") << "\n";

        // Strip the key to simulate a pre-3B project file.
        auto stripped = snap.clone();
        if (auto* blocksArr = stripped.getProperty("blocks", juce::var()).getArray())
            for (auto& bv : *blocksArr)
                if (auto* dobj = bv.getDynamicObject())
                    dobj->removeProperty("alwaysPlayBase");
        std::cout << "stripped JSON has key: "
                  << (juce::JSON::toString(stripped).contains("alwaysPlayBase") ? "YES (bad)" : "no")
                  << "\n";
        p3.resetAndLoad(stripped);
        for (auto* blk : p3.blocks)
            std::cout << "after missing-key load: " << blk->name
                      << " alwaysPlayBase=" << (blk->alwaysPlayBase ? "true" : "false") << "\n";
    }

    // STEP3C DIAG (temporary — remove in MASTER_PROMPT Step 7).
    // "Always play base block" resolver logic. Base = first block of the stack
    // group in project->blocks order (here: A).
    {
        std::cout << "\n=== STEP3C: alwaysPlayBase resolver, 20 resolves per scenario ===\n";
        Project p4;
        auto* A4 = p4.addBlock("A");   // base
        auto* B4 = p4.addBlock("B");
        auto* C4 = p4.addBlock("C");
        addClipTo(A4, "cA", 1000);
        addClipTo(B4, "cB", 1200);
        addClipTo(C4, "cC", 1500);
        p4.stackBlocks(B4->id, A4->id);
        p4.stackBlocks(C4->id, A4->id);

        juce::Random rng4(4242);
        ArrangementResolver r4;
        const juce::String baseId = A4->id;

        auto run20 = [&](const char* label, int expectEntries, bool simMode) {
            std::cout << "--- " << label << " ---\n";
            int okEntries = 0, basePresent = 0, okPos = 0;
            for (int i = 0; i < 20; ++i) {
                auto arr = r4.resolve(p4, rng4);
                bool hasBase = false, identical = true, ascending = true;
                for (int e = 0; e < arr.entries.size(); ++e) {
                    if (arr.entries[e].blockId == baseId) hasBase = true;
                    if (arr.entries[e].timelinePos != arr.entries[0].timelinePos) identical = false;
                    if (e > 0 && arr.entries[e].timelinePos <= arr.entries[e - 1].timelinePos) ascending = false;
                }
                if (arr.entries.size() == expectEntries) ++okEntries;
                if (hasBase) ++basePresent;
                if (simMode ? identical : ascending) ++okPos;
                std::cout << "resolve#" << i << " n=" << arr.entries.size() << " [";
                for (const auto& e : arr.entries) {
                    auto* blk = p4.getBlockById(e.blockId);
                    std::cout << (blk ? blk->name : juce::String("?")) << "@" << e.timelinePos << " ";
                }
                std::cout << "]\n";
            }
            std::cout << "SUMMARY " << label << ": entries==" << expectEntries << ": "
                      << okEntries << "/20, basePresent: " << basePresent << "/20, "
                      << (simMode ? "identicalPos: " : "ascendingPos: ") << okPos << "/20\n";
        };

        auto configure = [&](StackPlayMode mode, bool base, int count) {
            A4->stackPlayMode = mode;
            A4->alwaysPlayBase = base;
            A4->stackPlayCount.values.set(0, count);
            p4.propagateStackSettings(A4->stackGroup, A4);
        };

        configure(StackPlayMode::Simultaneous, true, 2);
        run20("SIM baseON playCount=2", 2, true);

        configure(StackPlayMode::Simultaneous, true, 1);
        run20("SIM baseON playCount=1 (base only)", 1, true);

        configure(StackPlayMode::Simultaneous, true, 3);
        run20("SIM baseON playCount=3", 3, true);

        configure(StackPlayMode::Simultaneous, false, 1);
        run20("REGRESSION SIM baseOFF playCount=1 (block should vary)", 1, true);

        configure(StackPlayMode::Sequential, false, 1);
        run20("REGRESSION SEQ baseOFF playCount=1", 1, false);

        configure(StackPlayMode::Sequential, false, 2);
        run20("REGRESSION SEQ baseOFF playCount=2 (sequential timeline)", 2, false);
    }

    // STEP3E DIAG (temporary — remove in MASTER_PROMPT Step 7).
    // BSF export: model.json must carry alwaysPlayBase per block.
    {
        std::cout << "\n=== STEP3E: BSF model.json alwaysPlayBase ===\n";
        Project p5;
        auto* a5 = p5.addBlock("A");
        auto* b5 = p5.addBlock("B");
        addClipTo(a5, "cA");
        addClipTo(b5, "cB");
        p5.stackBlocks(b5->id, a5->id);
        a5->stackPlayMode  = StackPlayMode::Simultaneous;
        a5->alwaysPlayBase = true;
        p5.propagateStackSettings(a5->stackGroup, a5);

        juce::Random rng5(99);
        ArrangementResolver r5;
        auto arr = r5.resolve(p5, rng5);
        arr.sampleRate = 48000.0;

        auto bsf = juce::File::getCurrentWorkingDirectory()
                       .getChildFile("resolverdiag_step3e.bsf");
        bsf.deleteFile();
        ExportRenderer ex;
        bool ok = ex.renderToBsf(arr, bsf, 16, nullptr, p5.toJSON());
        std::cout << "renderToBsf ok=" << (ok ? 1 : 0) << "\n";

        juce::ZipFile zip(bsf);
        for (int i = 0; i < zip.getNumEntries(); ++i) {
            if (zip.getEntry(i)->filename == "model.json") {
                std::unique_ptr<juce::InputStream> is(zip.createStreamForEntry(i));
                std::cout << (is ? is->readEntireStreamAsString() : juce::String("(no stream)"))
                          << "\n";
            }
        }
        bsf.deleteFile();
    }

    // STEP4A DIAG (temporary — remove in MASTER_PROMPT Step 7).
    // All-zero weights → shared picker's UNIFORM fallback: SEQ playCount=1,
    // 20 resolves → exactly 1 entry every time, and the block VARIES.
    {
        std::cout << "\n=== STEP4A: all-zero weights, SEQ playCount=1, uniform fallback ===\n";
        Project p6;
        auto* a6 = p6.addBlock("A");
        auto* b6 = p6.addBlock("B");
        auto* c6 = p6.addBlock("C");
        addClipTo(a6, "cA");
        addClipTo(b6, "cB");
        addClipTo(c6, "cC");
        p6.stackBlocks(b6->id, a6->id);
        p6.stackBlocks(c6->id, a6->id);
        a6->playChance = 0.0f;
        b6->playChance = 0.0f;
        c6->playChance = 0.0f;
        // playChance is per-block (not propagated); stack settings stay default SEQ pc=1.

        juce::Random rng6(2026);
        ArrangementResolver r6;
        int okEntries = 0;
        std::map<juce::String, int> byBlock;
        for (int i = 0; i < 20; ++i) {
            auto arr = r6.resolve(p6, rng6);
            if (arr.entries.size() == 1) ++okEntries;
            std::cout << "resolve#" << i << " n=" << arr.entries.size() << " [";
            for (const auto& e : arr.entries) {
                auto* blk = p6.getBlockById(e.blockId);
                byBlock[blk ? blk->name : juce::String("?")]++;
                std::cout << (blk ? blk->name : juce::String("?")) << " ";
            }
            std::cout << "]\n";
        }
        std::cout << "SUMMARY STEP4A: entries==1: " << okEntries << "/20, distribution:";
        for (const auto& kv : byBlock) std::cout << " " << kv.first << "=" << kv.second;
        std::cout << " (block must vary)\n";
    }

    // STEP4B DIAG (temporary — remove in MASTER_PROMPT Step 7).
    // Inclusion-probability numbers via the EXACT shared function the inspector
    // calls (StackPicker::inclusionProbabilities → StackPicker::pick).
    // Expected: 3 equal pc=1 → 33±2%; pc=2 → 67±2%; pc=3 → 100% flat;
    // base ON SIM pc=2 → base 100%, others 50±3%; weights 80/10/10 pc=1 → ~80/10/10.
    {
        std::cout << "\n=== STEP4B: inclusion probabilities (shared MC, 2000 trials) ===\n";
        Project p7;
        auto* A7 = p7.addBlock("A");   // base (first in project order)
        auto* B7 = p7.addBlock("B");
        auto* C7 = p7.addBlock("C");
        addClipTo(A7, "cA");
        addClipTo(B7, "cB");
        addClipTo(C7, "cC");
        p7.stackBlocks(B7->id, A7->id);
        p7.stackBlocks(C7->id, A7->id);

        auto report = [&](const char* label) {
            std::vector<Block*> group;
            for (auto* b : p7.blocks)
                if (b->stackGroup == A7->stackGroup) group.push_back(b);
            auto probs = StackPicker::inclusionProbabilities(group, p7.blocks);
            std::cout << "STEP4B " << label << ":";
            for (auto* b : group)
                std::cout << " " << b->name << "="
                          << juce::String(probs[b->id] * 100.0f, 2) << "%"
                          << " (display " << juce::roundToInt(probs[b->id] * 100.0f) << ")";
            std::cout << "\n";
        };

        auto configure = [&](StackPlayMode mode, bool base, int count,
                             float wA, float wB, float wC) {
            A7->stackPlayMode = mode;
            A7->alwaysPlayBase = base;
            A7->stackPlayCount.values.set(0, count);
            p7.propagateStackSettings(A7->stackGroup, A7);
            A7->playChance = wA;
            B7->playChance = wB;
            C7->playChance = wC;
        };

        configure(StackPlayMode::Sequential, false, 1, 1.0f, 1.0f, 1.0f);
        report("3 equal, pc=1 (expect 33±2 each)");

        configure(StackPlayMode::Sequential, false, 2, 1.0f, 1.0f, 1.0f);
        report("3 equal, pc=2 (expect 67±2 each)");

        configure(StackPlayMode::Sequential, false, 3, 1.0f, 1.0f, 1.0f);
        report("pc=3 of 3 (expect 100 flat, shortcut, no simulation)");

        configure(StackPlayMode::Simultaneous, true, 2, 1.0f, 1.0f, 1.0f);
        report("SIM baseON, pc=2 of 3 equal (expect base 100, others 50±3)");

        configure(StackPlayMode::Sequential, false, 1, 0.8f, 0.1f, 0.1f);
        report("weights 80/10/10, pc=1 (expect ~80/10/10)");

        configure(StackPlayMode::Sequential, false, 1, 0.0f, 0.0f, 0.0f);
        report("all-zero weights, pc=1 (uniform fallback, expect ~33 each)");
    }

    // STEP4AMEND DIAG (temporary — remove in MASTER_PROMPT Step 7).
    // Timing: one 50000-trial recompute for a 6-block stack, plus a
    // determinism check (two calls on identical state → identical maps).
    {
        std::cout << "\n=== STEP4AMEND: 6-block stack timing + determinism ===\n";
        Project p8;
        std::vector<Block*> blocks8;
        for (int i = 0; i < 6; ++i) {
            auto* b = p8.addBlock(juce::String::charToString((juce::juce_wchar)('A' + i)));
            addClipTo(b, "c" + b->name);
            blocks8.push_back(b);
        }
        for (int i = 1; i < 6; ++i)
            p8.stackBlocks(blocks8[(size_t)i]->id, blocks8[0]->id);
        blocks8[0]->stackPlayCount.values.set(0, 2);
        p8.propagateStackSettings(blocks8[0]->stackGroup, blocks8[0]);

        std::vector<Block*> group8;
        for (auto* b : p8.blocks)
            if (b->stackGroup == blocks8[0]->stackGroup) group8.push_back(b);

        const double t0 = juce::Time::getMillisecondCounterHiRes();
        auto probsA = StackPicker::inclusionProbabilities(group8, p8.blocks);
        const double t1 = juce::Time::getMillisecondCounterHiRes();
        auto probsB = StackPicker::inclusionProbabilities(group8, p8.blocks);

        std::cout << "STEP4AMEND recompute time (6 blocks, pc=2, default trials): "
                  << juce::String(t1 - t0, 2) << " ms\n";
        std::cout << "STEP4AMEND determinism (two calls, same state): "
                  << (probsA == probsB ? "IDENTICAL" : "DIFFER (BAD)") << "\n";
        std::cout << "STEP4AMEND values:";
        for (auto* b : group8)
            std::cout << " " << b->name << "=" << juce::String(probsA[b->id] * 100.0f, 2) << "%";
        std::cout << "\n";
    }

    std::cout << "DONE\n";
    return 0;
}
