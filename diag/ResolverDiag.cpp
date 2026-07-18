// STEP1 DIAG (permanent regression suite — Step 7B kept it).
// Headless reproduction of the stack play-count regression:
// builds a 3-block stack through the same model APIs the UI uses
// (Project::addBlock / stackBlocks / propagateStackSettings) and calls the
// same ArrangementResolver::resolve() the transport Play button calls.
#include <iostream>
#include <map>
#include <set>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Model/Project.h"
#include "Audio/ArrangementResolver.h"
#include "Audio/ExportRenderer.h"
#include "Audio/PlaybackEngine.h"
#include "Audio/StackPicker.h"
#include "UI/InspectorPanel.h"
#include "Utils/GridSnap.h"
#include "UI/BlockLinkOverlay.h"
#include "UI/LookAndFeel_BlockShuffler.h"
#include <cmath>

using namespace BlockShuffler;

static std::shared_ptr<juce::AudioBuffer<float>> makeBuf(int len = 1000) {
    auto b = std::make_shared<juce::AudioBuffer<float>>(2, len);
    b->clear();
    return b;
}

// File-backed clip for save/load and undo tests (T11/T12): resetAndLoad clamps
// marks to the reloaded buffer length (Serialization.cpp FIX M7), so those tests
// need a real audio file on disk — exactly the app's condition.
static void addClipWithFile(Block* blk, const juce::String& nm, int bodyLen,
                            const juce::File& dir) {
    auto f = dir.getChildFile(nm + ".wav");
    f.deleteFile();
    juce::WavAudioFormat fmt;
    if (auto os = std::unique_ptr<juce::FileOutputStream>(f.createOutputStream())) {
        if (auto* w = fmt.createWriterFor(os.get(), 44100.0, 2, 16, {}, 0)) {
            os.release();  // writer owns the stream now
            std::unique_ptr<juce::AudioFormatWriter> writer(w);
            juce::AudioBuffer<float> buf(2, bodyLen);
            buf.clear();
            writer->writeFromAudioSampleBuffer(buf, 0, bodyLen);
        }
    }
    auto c = std::make_unique<Clip>();
    c->name = nm;
    c->audioFile = f;
    c->audioBuffer = std::make_shared<juce::AudioBuffer<float>>(2, bodyLen);
    c->audioBuffer->clear();
    c->startMark = 0;
    c->endMark = bodyLen;
    c->probability = 1.0f;
    blk->addClip(std::move(c));
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

// ── Manual-round project generator (invoked as: ResolverDiag --gen-manual <dir>)
// Writes tone WAVs + two .bsp projects for the manual acceptance session:
//   TestProject.bsp   — DETERMINISTIC (SIM stack plays 2 of 2, link at 0%) so any
//                       Play and any export produce identical audio; contains one
//                       tempo-stretched join (Intro tempo 120 tail -> Verse tempo
//                       160 lead-in) for 10.2/10.4 Audacity/ear comparison.
//   StressProject.bsp — 50 blocks (5 stacked pairs) for 12.1.
static void writeToneWav(const juce::File& f, double freqHz, double seconds, bool noise,
                         double sr = 44100.0) {
    const int len = (int)(seconds * sr);
    juce::AudioBuffer<float> buf(2, len);
    juce::Random nz(42);
    for (int i = 0; i < len; ++i) {
        float v = noise ? (nz.nextFloat() * 2.0f - 1.0f) * 0.5f
                        : 0.6f * (float)std::sin(2.0 * juce::MathConstants<double>::pi
                                                 * freqHz * i / sr);
        // 10ms fade in/out so the FILES themselves have no clicks — any click heard
        // at a join is then attributable to the mixer/stretcher, not the material.
        const int fade = (int)(0.010 * sr);
        if (i < fade)       v *= (float)i / fade;
        if (i >= len - fade) v *= (float)(len - 1 - i) / fade;
        buf.setSample(0, i, v);
        buf.setSample(1, i, v);
    }
    f.deleteFile();
    juce::WavAudioFormat fmt;
    if (auto os = std::unique_ptr<juce::FileOutputStream>(f.createOutputStream())) {
        if (auto* w = fmt.createWriterFor(os.get(), sr, 2, 16, {}, 0)) {
            os.release();
            std::unique_ptr<juce::AudioFormatWriter> writer(w);
            writer->writeFromAudioSampleBuffer(buf, 0, len);
        }
    }
}

static void addFileClipMeta(Block* blk, const juce::File& f, const juce::String& nm,
                            juce::int64 startMark, juce::int64 endMark, double tempo) {
    auto c = std::make_unique<Clip>();
    c->name = nm;
    c->audioFile = f;
    c->startMark = startMark;
    c->endMark = endMark;
    c->probability = 1.0f;
    c->tempo = tempo;
    blk->addClip(std::move(c));
}

static int generateManualRound(const juce::File& dir) {
    auto media = dir.getChildFile("media");
    media.createDirectory();
    writeToneWav(media.getChildFile("intro_440.wav"),   440.0, 2.5, false);
    writeToneWav(media.getChildFile("verse_220.wav"),   220.0, 2.5, false);
    writeToneWav(media.getChildFile("chorusA_880.wav"), 880.0, 2.0, false);
    writeToneWav(media.getChildFile("chorusB_noise.wav"),  0.0, 2.0, true);
    const double sr = 44100.0;

    { // TestProject.bsp
        Project p;
        p.name = "ManualRound";
        p.sampleRate = sr;
        auto* intro = p.addBlock("Intro");
        auto* verse = p.addBlock("Verse");
        auto* chA   = p.addBlock("ChorusA");
        auto* chB   = p.addBlock("ChorusB");
        // Intro: 440Hz, tempo 120, body 0..2.0s, TAIL 0.5s (stretch source)
        addFileClipMeta(intro, media.getChildFile("intro_440.wav"), "intro 440",
                        0, (juce::int64)(2.0 * sr), 120.0);
        // Verse: 220Hz, tempo 160, LEAD-IN 0.4s, body to end (stretch target)
        addFileClipMeta(verse, media.getChildFile("verse_220.wav"), "verse 220",
                        (juce::int64)(0.4 * sr), (juce::int64)(2.5 * sr), 160.0);
        // Chorus stack: 880Hz + noise, layered, BOTH always play (deterministic)
        addFileClipMeta(chA, media.getChildFile("chorusA_880.wav"), "chorus 880",
                        0, (juce::int64)(2.0 * sr), 120.0);
        addFileClipMeta(chB, media.getChildFile("chorusB_noise.wav"), "chorus noise",
                        0, (juce::int64)(2.0 * sr), 120.0);
        p.stackBlocks(chB->id, chA->id);
        chA->stackPlayMode = StackPlayMode::Simultaneous;
        chA->stackPlayCount.values.set(0, 2);
        p.propagateStackSettings(chA->stackGroup, chA);
        // Link at 0% — present for the UI/arc, deterministic for export comparison
        // (link behavior itself is covered by T9/T13/T14).
        p.addLink(intro->id, verse->id, 0.0f);
        if (!p.saveToFile(dir.getChildFile("TestProject.bsp"))) {
            std::cout << "GEN FAIL: TestProject.bsp save failed\n";
            return 1;
        }
    }
    { // StressProject.bsp — 50 blocks, 5 stacked pairs (10&11, 20&21, ... 50&49)
        Project p;
        p.name = "Stress50";
        p.sampleRate = sr;
        const char* files[4] = { "intro_440.wav", "verse_220.wav",
                                 "chorusA_880.wav", "chorusB_noise.wav" };
        std::vector<Block*> blocks;
        for (int i = 1; i <= 50; ++i) {
            auto* b = p.addBlock("Block " + juce::String(i));
            auto f = media.getChildFile(files[(i - 1) % 4]);
            addFileClipMeta(b, f, "clip " + juce::String(i),
                            0, (juce::int64)(2.0 * sr), 120.0);
            blocks.push_back(b);
        }
        for (int i = 10; i <= 50; i += 10)
            p.stackBlocks(blocks[(size_t)(i - 1)]->id, blocks[(size_t)(i - 2)]->id);
        if (!p.saveToFile(dir.getChildFile("StressProject.bsp"))) {
            std::cout << "GEN FAIL: StressProject.bsp save failed\n";
            return 1;
        }
    }
    // Verify both load cleanly (files found, blocks intact) and TestProject resolves.
    for (const char* name : { "TestProject.bsp", "StressProject.bsp" }) {
        Project q;
        bool ok = q.loadFromFile(dir.getChildFile(name));
        std::cout << name << ": load=" << (ok ? "ok" : "FAIL")
                  << " blocks=" << q.blocks.size()
                  << " links=" << q.links.size()
                  << " missingFiles=" << q.missingFilesOnLoad.size() << "\n";
        if (!ok || !q.missingFilesOnLoad.isEmpty()) return 1;
        if (juce::String(name) == "TestProject.bsp") {
            juce::Random r(1); ArrangementResolver res;
            auto arr = res.resolve(q, r);
            std::cout << "  resolve: entries=" << arr.entries.size()
                      << " totalSamples=" << arr.totalDurationSamples
                      << " (~" << juce::String(arr.totalDurationSamples / 44100.0, 2)
                      << "s)\n";
        }
    }
    std::cout << "GEN OK: " << dir.getFullPathName() << "\n";
    return 0;
}

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI init;

    if (argc >= 3 && juce::String(argv[1]) == "--gen-manual")
        return generateManualRound(juce::File(juce::String(argv[2])));

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

    // STEP2 DIAG (permanent regression suite — Step 7B kept it).
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

    // STEP3B DIAG (permanent regression suite — Step 7B kept it).
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

    // STEP3C DIAG (permanent regression suite — Step 7B kept it).
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

    // STEP3E DIAG (permanent regression suite — Step 7B kept it).
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

    // STEP4A DIAG (permanent regression suite — Step 7B kept it).
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

    // STEP4B DIAG (permanent regression suite — Step 7B kept it).
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

    // STEP4AMEND DIAG (permanent regression suite — Step 7B kept it).
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

    // STEP6 DIAG (permanent regression suite — Step 7B kept it).
    // Harness half of the Step 6 re-test: T1-T12, PASS/FAIL with evidence.
    {
        std::cout << "\n=== STEP6: harness half T1-T12 ===\n";
        int failed = 0;
        auto verdict = [&](const char* test, bool pass, const juce::String& evidence) {
            std::cout << test << (pass ? "  PASS  " : "  FAIL  ") << evidence << "\n";
            if (!pass) ++failed;
        };

        // Shared 3-block stack builder: A(1000) B(1200) C(1500), A = base.
        auto buildStack3 = [&](Project& p, StackPlayMode mode, int pc, bool base,
                               float wA, float wB, float wC) {
            auto* A = p.addBlock("A");
            auto* B = p.addBlock("B");
            auto* C = p.addBlock("C");
            addClipTo(A, "cA", 1000);
            addClipTo(B, "cB", 1200);
            addClipTo(C, "cC", 1500);
            p.stackBlocks(B->id, A->id);
            p.stackBlocks(C->id, A->id);
            A->stackPlayMode = mode;
            A->stackPlayCount.values.set(0, pc);
            A->alwaysPlayBase = base;
            p.propagateStackSettings(A->stackGroup, A);
            A->playChance = wA; B->playChance = wB; C->playChance = wC;
            return A;
        };

        // Signature of n resolves: "Name@pos;Name@pos;|..." — for exact comparisons.
        auto runSig = [&](Project& p, juce::int64 seed, int n) {
            juce::Random r(seed);
            ArrangementResolver res;
            juce::String sig;
            for (int i = 0; i < n; ++i) {
                auto arr = res.resolve(p, r);
                for (const auto& e : arr.entries) {
                    auto* b = p.getBlockById(e.blockId);
                    sig << (b ? b->name : juce::String("?")) << "@"
                        << juce::String((juce::int64)e.timelinePos) << ";";
                }
                sig << "|";
            }
            return sig;
        };

        { // T1: SEQ pc=1 → exactly 1 entry, 10 runs
            Project p; buildStack3(p, StackPlayMode::Sequential, 1, false, 1, 1, 1);
            juce::Random r(601); ArrangementResolver res; int ok = 0;
            for (int i = 0; i < 10; ++i) ok += (res.resolve(p, r).entries.size() == 1);
            verdict("T1  SEQ pc=1 exactly 1 entry", ok == 10, juce::String(ok) + "/10");
        }
        { // T2: SEQ pc=2 → exactly 2, gapless sequential timeline, random order
            Project p; buildStack3(p, StackPlayMode::Sequential, 2, false, 1, 1, 1);
            juce::Random r(602); ArrangementResolver res;
            int okN = 0, okGap = 0; std::set<juce::String> firstBlocks;
            for (int i = 0; i < 20; ++i) {
                auto arr = res.resolve(p, r);
                if (arr.entries.size() == 2) ++okN;
                if (arr.entries.size() == 2) {
                    const auto& e0 = arr.entries.getReference(0);
                    const auto& e1 = arr.entries.getReference(1);
                    if (e1.timelinePos == e0.timelinePos + (e0.endMark - e0.startMark)) ++okGap;
                    auto* b = p.getBlockById(e0.blockId);
                    firstBlocks.insert(b ? b->name : juce::String("?"));
                }
            }
            verdict("T2  SEQ pc=2 gapless + random order",
                    okN == 20 && okGap == 20 && firstBlocks.size() > 1,
                    "entries==2: " + juce::String(okN) + "/20, zero-gap: " + juce::String(okGap)
                    + "/20, distinct first blocks: " + juce::String((int)firstBlocks.size()));
        }
        { // T3: SIM pc=1 → exactly 1
            Project p; buildStack3(p, StackPlayMode::Simultaneous, 1, false, 1, 1, 1);
            juce::Random r(603); ArrangementResolver res; int ok = 0;
            for (int i = 0; i < 10; ++i) ok += (res.resolve(p, r).entries.size() == 1);
            verdict("T3  SIM pc=1 exactly 1 entry", ok == 10, juce::String(ok) + "/10");
        }
        { // T4: SIM pc=2 → exactly 2, identical timelinePos
            Project p; buildStack3(p, StackPlayMode::Simultaneous, 2, false, 1, 1, 1);
            juce::Random r(604); ArrangementResolver res; int ok = 0;
            for (int i = 0; i < 10; ++i) {
                auto arr = res.resolve(p, r);
                ok += (arr.entries.size() == 2
                       && arr.entries[0].timelinePos == arr.entries[1].timelinePos);
            }
            verdict("T4  SIM pc=2 two entries, identical pos", ok == 10, juce::String(ok) + "/10");
        }
        { // T5: SIM pc=3 → 3 identical pos, next block at cursor + LONGEST body (1500)
            Project p; buildStack3(p, StackPlayMode::Simultaneous, 3, false, 1, 1, 1);
            auto* D = p.addBlock("D"); addClipTo(D, "cD", 700);
            juce::Random r(605); ArrangementResolver res; int ok = 0;
            for (int i = 0; i < 10; ++i) {
                auto arr = res.resolve(p, r);
                bool good = (arr.entries.size() == 4);
                if (good) {
                    for (int e = 0; e < 3; ++e)
                        if (arr.entries[e].timelinePos != arr.entries[0].timelinePos) good = false;
                    if (arr.entries[3].timelinePos != arr.entries[0].timelinePos + 1500) good = false;
                    auto* b = p.getBlockById(arr.entries[3].blockId);
                    if (!b || b->name != "D") good = false;
                }
                ok += good;
            }
            verdict("T5  SIM pc=3 layered + D at +longest(1500)", ok == 10, juce::String(ok) + "/10");
        }
        { // T6: base ON, SIM pc=2 of 3 → base present 20/20 + exactly 1 other
            Project p; auto* A = buildStack3(p, StackPlayMode::Simultaneous, 2, true, 1, 1, 1);
            juce::Random r(606); ArrangementResolver res; int ok = 0;
            for (int i = 0; i < 20; ++i) {
                auto arr = res.resolve(p, r);
                bool hasBase = false;
                for (const auto& e : arr.entries) if (e.blockId == A->id) hasBase = true;
                ok += (arr.entries.size() == 2 && hasBase);
            }
            verdict("T6  SIM baseON pc=2: base + exactly 1 other", ok == 20, juce::String(ok) + "/20");
        }
        { // T7: inclusion % — 33±2 / 67±2 / 100 flat / base 100 + 50±3
            Project p; auto* A = buildStack3(p, StackPlayMode::Sequential, 1, false, 1, 1, 1);
            auto group = [&]() {
                std::vector<Block*> g;
                for (auto* b : p.blocks) if (b->stackGroup == A->stackGroup) g.push_back(b);
                return g;
            };
            auto inRange = [&](std::map<juce::String, float>& m, float target, float tol) {
                for (auto* b : group())
                    if (std::abs(m[b->id] * 100.0f - target) > tol) return false;
                return true;
            };
            auto cfg = [&](StackPlayMode mode, bool base, int pc) {
                A->stackPlayMode = mode; A->alwaysPlayBase = base;
                A->stackPlayCount.values.set(0, pc);
                p.propagateStackSettings(A->stackGroup, A);
            };
            auto fmt = [&](std::map<juce::String, float>& m) {
                juce::String s;
                for (auto* b : group()) s << b->name << "=" << juce::String(m[b->id] * 100.0f, 2) << "% ";
                return s;
            };
            cfg(StackPlayMode::Sequential, false, 1);
            auto m1 = StackPicker::inclusionProbabilities(group(), p.blocks);
            bool ok1 = inRange(m1, 33.33f, 2.0f);
            cfg(StackPlayMode::Sequential, false, 2);
            auto m2 = StackPicker::inclusionProbabilities(group(), p.blocks);
            bool ok2 = inRange(m2, 66.67f, 2.0f);
            cfg(StackPlayMode::Sequential, false, 3);
            auto m3 = StackPicker::inclusionProbabilities(group(), p.blocks);
            bool ok3 = inRange(m3, 100.0f, 0.0f);
            cfg(StackPlayMode::Simultaneous, true, 2);
            auto m4 = StackPicker::inclusionProbabilities(group(), p.blocks);
            bool ok4 = (m4[A->id] == 1.0f);
            for (auto* b : group())
                if (b != A && std::abs(m4[b->id] * 100.0f - 50.0f) > 3.0f) ok4 = false;
            verdict("T7  inclusion % (33/67/100/base-100+50)", ok1 && ok2 && ok3 && ok4,
                    "pc1[" + fmt(m1) + "] pc2[" + fmt(m2) + "] pc3[" + fmt(m3) + "] base[" + fmt(m4) + "]");
        }
        { // T8: weights 80/10/10, pc=1, 20 resolves → the 80 block picked most
            Project p; auto* A = buildStack3(p, StackPlayMode::Sequential, 1, false, 0.8f, 0.1f, 0.1f);
            juce::Random r(608); ArrangementResolver res;
            std::map<juce::String, int> byBlock;
            for (int i = 0; i < 20; ++i) {
                auto arr = res.resolve(p, r);
                for (const auto& e : arr.entries) {
                    auto* b = p.getBlockById(e.blockId);
                    byBlock[b ? b->name : juce::String("?")]++;
                }
            }
            bool ok = byBlock["A"] > byBlock["B"] && byBlock["A"] > byBlock["C"];
            verdict("T8  weights 80/10/10 pc=1: 80-block most", ok,
                    "A=" + juce::String(byBlock["A"]) + " B=" + juce::String(byBlock["B"])
                    + " C=" + juce::String(byBlock["C"]));
        }
        { // T9: link 1<->3 at 100% → 3,2,1 every time; model positions untouched
            Project p;
            auto* X = p.addBlock("X"); auto* Y = p.addBlock("Y"); auto* Z = p.addBlock("Z");
            addClipTo(X, "cX", 1000); addClipTo(Y, "cY", 1000); addClipTo(Z, "cZ", 1000);
            p.addLink(X->id, Z->id, 1.0f);
            juce::String posBefore, posAfter;
            for (auto* b : p.blocks) posBefore << b->name << "=" << juce::String(b->position) << ";";
            juce::Random r(609); ArrangementResolver res; int ok = 0;
            for (int i = 0; i < 10; ++i) {
                auto arr = res.resolve(p, r);
                bool good = (arr.entries.size() == 3);
                if (good) {
                    const char* expect[3] = { "Z", "Y", "X" };
                    for (int e = 0; e < 3; ++e) {
                        auto* b = p.getBlockById(arr.entries[e].blockId);
                        if (!b || b->name != expect[e]) good = false;
                    }
                }
                ok += good;
            }
            for (auto* b : p.blocks) posAfter << b->name << "=" << juce::String(b->position) << ";";
            verdict("T9  link 100%: Z,Y,X every time + positions unmutated",
                    ok == 10 && posBefore == posAfter,
                    "order Z,Y,X: " + juce::String(ok) + "/10, pos before[" + posBefore
                    + "] after[" + posAfter + "]");
        }
        { // T10: isDone must not affect selection — same seed, toggled isDone,
          // 100 resolves each → bit-identical pick sequences (exact, not statistical).
            Project p; buildStack3(p, StackPlayMode::Sequential, 1, false, 1, 1, 1);
            Block* B = nullptr;
            for (auto* b : p.blocks) if (b->name == "B") B = b;
            B->isDone = true;
            auto sigDone = runSig(p, 610, 100);
            B->isDone = false;
            auto sigNotDone = runSig(p, 610, 100);
            int doneCount = 0;
            for (int i = 0; i + 1 < sigDone.length(); ++i)
                if (sigDone[i] == 'B' && sigDone[i + 1] == '@') ++doneCount;
            verdict("T10 isDone ignored: identical sequences + done block plays",
                    sigDone == sigNotDone && doneCount > 0,
                    "sequences identical: " + juce::String(sigDone == sigNotDone ? "yes" : "NO")
                    + ", done-block inclusions: " + juce::String(doneCount) + "/100");
        }
        { // T11: save->load round-trip: fields survive + identical entry structure.
          // File-backed clips: the load path re-reads the audio and restores marks
          // exactly as the app does on open/undo.
            auto tmpDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("resolverdiag_step6");
            tmpDir.createDirectory();
            Project p;
            auto* A = p.addBlock("A");
            auto* B = p.addBlock("B");
            auto* C = p.addBlock("C");
            auto* D = p.addBlock("D");
            addClipWithFile(A, "t11A", 1000, tmpDir);
            addClipWithFile(B, "t11B", 1200, tmpDir);
            addClipWithFile(C, "t11C", 1500, tmpDir);
            addClipWithFile(D, "t11D", 1000, tmpDir);
            p.stackBlocks(B->id, A->id);
            p.stackBlocks(C->id, A->id);
            A->stackPlayMode = StackPlayMode::Simultaneous;
            A->stackPlayCount.values.set(0, 2);
            A->alwaysPlayBase = true;
            p.propagateStackSettings(A->stackGroup, A);
            A->playChance = 0.8f; B->playChance = 0.1f; C->playChance = 0.1f;
            p.addLink(A->id, D->id, 0.5f);
            const juce::String aId = A->id, dId = D->id;
            auto pre = runSig(p, 611, 10);
            auto snap = p.toJSON();
            p.resetAndLoad(snap);
            auto* A2 = p.getBlockById(aId);
            bool fields = (A2 != nullptr
                && A2->stackPlayMode == StackPlayMode::Simultaneous
                && A2->stackPlayCount.values.size() == 1 && A2->stackPlayCount.values[0] == 2
                && A2->alwaysPlayBase
                && std::abs(A2->playChance - 0.8f) < 1e-5f
                && p.links.size() == 1
                && ((p.links[0]->blockA == aId && p.links[0]->blockB == dId)
                    || (p.links[0]->blockA == dId && p.links[0]->blockB == aId))
                && std::abs(p.links[0]->swapProbability - 0.5f) < 1e-5f);
            auto post = runSig(p, 611, 10);
            verdict("T11 save->load: fields + identical entry structure",
                    fields && pre == post,
                    juce::String("fields: ") + (fields ? "ok" : "BAD")
                    + ", entry structure identical: " + (pre == post ? "yes" : "NO"));
        }
        { // T12: undo structural — mutate, resetAndLoad(snapshot), JSON string-identical.
          // File-backed clips for the same reason as T11 (undo path reloads audio).
            auto tmpDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("resolverdiag_step6");
            tmpDir.createDirectory();
            Project p;
            auto* A = p.addBlock("A");
            auto* B = p.addBlock("B");
            auto* C = p.addBlock("C");
            addClipWithFile(A, "t12A", 1000, tmpDir);
            addClipWithFile(B, "t12B", 1200, tmpDir);
            addClipWithFile(C, "t12C", 1500, tmpDir);
            p.stackBlocks(B->id, A->id);
            p.stackBlocks(C->id, A->id);
            A->stackPlayMode = StackPlayMode::Simultaneous;
            A->stackPlayCount.values.set(0, 2);
            p.propagateStackSettings(A->stackGroup, A);
            auto snapVar = p.toJSON();
            auto snapStr = juce::JSON::toString(snapVar);
            auto undoAndCompare = [&]() {
                p.resetAndLoad(snapVar);
                return juce::JSON::toString(p.toJSON()) == snapStr;
            };
            { auto* blk = p.blocks.getFirst();
              blk->alwaysPlayBase = true;
              p.propagateStackSettings(blk->stackGroup, blk); }
            bool okBase = undoAndCompare();
            { auto* blk = p.blocks.getFirst();
              blk->stackPlayCount.values.set(0, 3);
              p.propagateStackSettings(blk->stackGroup, blk); }
            bool okPc = undoAndCompare();
            { auto* blk = p.blocks.getFirst(); blk->playChance = 0.42f; }
            bool okW = undoAndCompare();
            verdict("T12 undo structural: JSON identical after revert", okBase && okPc && okW,
                    juce::String("baseToggle: ") + (okBase ? "ok" : "BAD")
                    + ", playCount: " + (okPc ? "ok" : "BAD")
                    + ", weight: " + (okW ? "ok" : "BAD"));
            juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("resolverdiag_step6").deleteRecursively();
        }

        // ── Round 1 acceptance gaps (2026-07-06, verification only) ──────────
        { // T13 (4.5): link swap rates — 0% never, 50% ≈ half, 100% covered by T9
            Project p;
            auto* X = p.addBlock("X"); auto* Y = p.addBlock("Y"); auto* Z = p.addBlock("Z");
            addClipTo(X, "cX", 1000); addClipTo(Y, "cY", 1000); addClipTo(Z, "cZ", 1000);
            auto* lnk = p.addLink(X->id, Z->id, 0.0f);
            ArrangementResolver res;

            juce::Random r0(6131);
            int okN0 = 0, swaps0 = 0;
            for (int i = 0; i < 50; ++i) {
                auto arr = res.resolve(p, r0);
                if (arr.entries.size() == 3) ++okN0;
                auto* first = p.getBlockById(arr.entries[0].blockId);
                if (first && first->name == "Z") ++swaps0;
            }

            lnk->swapProbability = 0.5f;
            juce::Random r5(6132);
            int okN5 = 0, swaps5 = 0;
            for (int i = 0; i < 200; ++i) {
                auto arr = res.resolve(p, r5);
                if (arr.entries.size() == 3) ++okN5;
                auto* first = p.getBlockById(arr.entries[0].blockId);
                if (first && first->name == "Z") ++swaps5;
            }

            verdict("T13 link rates: 0% never, 50% = 50±10%",
                    okN0 == 50 && swaps0 == 0 && okN5 == 200
                    && swaps5 >= 80 && swaps5 <= 120,
                    "0%: swaps " + juce::String(swaps0) + "/50 (entries==3: " + juce::String(okN0)
                    + "/50); 50%: swaps " + juce::String(swaps5) + "/200 (entries==3: "
                    + juce::String(okN5) + "/200); 100% determinism: T9");
        }
        { // T14 (4.6): link endpoint INSIDE a stack — only the two linked blocks
          // swap; the pulled-out block leaves its stack for that pass, the rest
          // of the stack stays put, nothing is dropped, model positions untouched.
          // Setup: stack {S1,S2,S3} (SEQ, pc=3), follower W, link W<->S2 @100%.
          // Expected slot order per resolve: [{S1,S3} both, in random order], W, S2.
            Project p;
            auto* S1 = p.addBlock("S1"); auto* S2 = p.addBlock("S2"); auto* S3 = p.addBlock("S3");
            auto* W  = p.addBlock("W");
            addClipTo(S1, "c1", 1000); addClipTo(S2, "c2", 1100);
            addClipTo(S3, "c3", 1200); addClipTo(W,  "cW", 700);
            p.stackBlocks(S2->id, S1->id);
            p.stackBlocks(S3->id, S1->id);
            S1->stackPlayCount.values.set(0, 3);
            p.propagateStackSettings(S1->stackGroup, S1);
            p.addLink(W->id, S2->id, 1.0f);

            juce::String posBefore, posAfter;
            for (auto* b : p.blocks) posBefore << b->name << "=" << juce::String(b->position) << ";";

            juce::Random r(614); ArrangementResolver res;
            int ok = 0; juce::String sample;
            for (int i = 0; i < 20; ++i) {
                auto arr = res.resolve(p, r);
                bool good = (arr.entries.size() == 4);
                std::map<juce::String, int> idx;
                if (good) {
                    for (int e = 0; e < 4; ++e) {
                        auto* b = p.getBlockById(arr.entries[e].blockId);
                        if (!b || idx.count(b->name)) { good = false; break; }
                        idx[b->name] = e;
                        if (e > 0 && arr.entries[e].timelinePos <= arr.entries[e - 1].timelinePos)
                            good = false;
                    }
                }
                if (good)
                    good = idx.count("S1") && idx.count("S2") && idx.count("S3") && idx.count("W")
                           && idx["W"] == 2 && idx["S2"] == 3
                           && idx["S1"] < 2 && idx["S3"] < 2;
                ok += good;
                if (i == 0) {
                    for (const auto& e : arr.entries) {
                        auto* b = p.getBlockById(e.blockId);
                        sample << (b ? b->name : juce::String("?")) << " ";
                    }
                }
            }
            for (auto* b : p.blocks) posAfter << b->name << "=" << juce::String(b->position) << ";";
            verdict("T14 link into stack: {S1,S3},W,S2 + positions unmutated",
                    ok == 20 && posBefore == posAfter,
                    juce::String(ok) + "/20, sample [" + sample + "], pos before[" + posBefore
                    + "] after[" + posAfter + "]");
        }
        { // T14b (4.6 Carter correction): link to the stack's BASE block swaps
          // the WHOLE stack (link to a non-base member still extracts only that
          // block -- guarded by T14 above).  Setup: stack {S1,S2,S3} (S1 = base,
          // SEQ, pc=3), follower W, link W<->S1 @100%.
          // Expected per resolve: W takes the stack's slot (entry 0); the INTACT
          // 3-member stack takes W's slot (entries 1-3, all members present,
          // random order); ascending timeline; model positions unmutated; 20/20.
          // Negative control @0%: never swaps -- stack first, W last, 20/20.
            auto buildT14b = [&](Project& p, float linkProb) -> juce::String {
                auto* S1 = p.addBlock("S1"); auto* S2 = p.addBlock("S2"); auto* S3 = p.addBlock("S3");
                auto* W  = p.addBlock("W");
                addClipTo(S1, "c1", 1000); addClipTo(S2, "c2", 1100);
                addClipTo(S3, "c3", 1200); addClipTo(W,  "cW", 700);
                p.stackBlocks(S2->id, S1->id);
                p.stackBlocks(S3->id, S1->id);
                S1->stackPlayCount.values.set(0, 3);
                p.propagateStackSettings(S1->stackGroup, S1);
                p.addLink(W->id, S1->id, linkProb);
                juce::String pos;
                for (auto* b : p.blocks) pos << b->name << "=" << juce::String(b->position) << ";";
                return pos;
            };

            { // 100%: whole stack swaps with W, deterministic slot order 20/20
                Project p; juce::String posBefore = buildT14b(p, 1.0f);
                juce::Random r(6141); ArrangementResolver res;
                int ok = 0; juce::String sample;
                std::set<int> s1Idx;  // INTACT-stack proof: S1 must shuffle WITHIN
                // the stack (varying entry index). The old extract-only behaviour
                // pins S1 into its own trailing solo slot => always entry 3.
                for (int i = 0; i < 20; ++i) {
                    auto arr = res.resolve(p, r);
                    bool good = (arr.entries.size() == 4);
                    std::map<juce::String, int> idx;
                    if (good) {
                        for (int e = 0; e < 4; ++e) {
                            auto* b = p.getBlockById(arr.entries[e].blockId);
                            if (!b || idx.count(b->name)) { good = false; break; }
                            idx[b->name] = e;
                            if (e > 0 && arr.entries[e].timelinePos <= arr.entries[e - 1].timelinePos)
                                good = false;
                        }
                    }
                    if (good)
                        good = idx.count("S1") && idx.count("S2") && idx.count("S3") && idx.count("W")
                               && idx["W"] == 0
                               && idx["S1"] >= 1 && idx["S2"] >= 1 && idx["S3"] >= 1;
                    if (good) s1Idx.insert(idx["S1"]);
                    ok += good;
                    if (i == 0)
                        for (const auto& e : arr.entries) {
                            auto* b = p.getBlockById(e.blockId);
                            sample << (b ? b->name : juce::String("?")) << " ";
                        }
                }
                juce::String posAfter;
                for (auto* b : p.blocks) posAfter << b->name << "=" << juce::String(b->position) << ";";
                verdict("T14b link to BASE swaps WHOLE stack: W,{S1,S2,S3} intact + positions unmutated",
                        ok == 20 && s1Idx.size() >= 2 && posBefore == posAfter,
                        juce::String(ok) + "/20, s1 entry indices seen="
                        + juce::String((int)s1Idx.size()) + ", sample [" + sample
                        + "], pos before[" + posBefore + "] after[" + posAfter + "]");
            }
            { // 0% negative control: never swaps
                Project p; buildT14b(p, 0.0f);
                juce::Random r(6142); ArrangementResolver res;
                int ok = 0;
                for (int i = 0; i < 20; ++i) {
                    auto arr = res.resolve(p, r);
                    bool good = (arr.entries.size() == 4);
                    if (good) {
                        auto* last = p.getBlockById(arr.entries[3].blockId);
                        good = (last != nullptr && last->name == "W");
                        for (int e = 0; e < 3 && good; ++e) {
                            auto* b = p.getBlockById(arr.entries[e].blockId);
                            good = (b != nullptr && b->name != "W");
                        }
                    }
                    ok += good;
                }
                verdict("T14b control link@0%: stack stays first, W last", ok == 20,
                        juce::String(ok) + "/20");
            }
        }
        { // T15 (4.7): undo-of-link structural — create-link and change-% both
          // revert to string-identical JSON via resetAndLoad (the undo path).
            auto tmpDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("resolverdiag_r1");
            tmpDir.createDirectory();
            Project p;
            auto* A = p.addBlock("A"); auto* B = p.addBlock("B");
            addClipWithFile(A, "t15A", 1000, tmpDir);
            addClipWithFile(B, "t15B", 1000, tmpDir);

            auto snap1 = p.toJSON();
            auto str1  = juce::JSON::toString(snap1);
            p.addLink(A->id, B->id, 0.5f);
            p.resetAndLoad(snap1);
            bool okCreate = (juce::JSON::toString(p.toJSON()) == str1) && p.links.isEmpty();

            { auto* a2 = p.blocks.getFirst(); p.addLink(a2->id, p.blocks[1]->id, 0.5f); }
            auto snap2 = p.toJSON();
            auto str2  = juce::JSON::toString(snap2);
            p.links[0]->swapProbability = 0.9f;
            p.resetAndLoad(snap2);
            bool okProb = (juce::JSON::toString(p.toJSON()) == str2)
                          && p.links.size() == 1
                          && std::abs(p.links[0]->swapProbability - 0.5f) < 1e-6f;

            verdict("T15 undo-of-link: create + % change both revert", okCreate && okProb,
                    juce::String("create: ") + (okCreate ? "ok" : "BAD")
                    + ", probChange: " + (okProb ? "ok" : "BAD"));
            tmpDir.deleteRecursively();
        }

        { // T16 (2.6): CLIP-weight distribution within one block — 80/10/10 over
          // 200 picks via the resolver's own pickClip (public static; no hook needed).
            Project p;
            auto* blk = p.addBlock("A");
            addClipTo(blk, "heavy", 1000);
            addClipTo(blk, "mid",   1000);
            addClipTo(blk, "low",   1000);
            blk->clips[0]->probability = 0.8f;
            blk->clips[1]->probability = 0.1f;
            blk->clips[2]->probability = 0.1f;

            juce::Random r(616);
            std::map<juce::String, int> byClip;
            for (int i = 0; i < 200; ++i) {
                auto* c = ArrangementResolver::pickClip(*blk, r);
                byClip[c ? c->name : juce::String("?")]++;
            }
            float pctH = byClip["heavy"] / 2.0f, pctM = byClip["mid"] / 2.0f, pctL = byClip["low"] / 2.0f;
            verdict("T16 clip weights 80/10/10 over 200 picks",
                    std::abs(pctH - 80.0f) <= 10.0f
                    && std::abs(pctM - 10.0f) <= 8.0f
                    && std::abs(pctL - 10.0f) <= 8.0f,
                    "heavy=" + juce::String(pctH, 1) + "% mid=" + juce::String(pctM, 1)
                    + "% low=" + juce::String(pctL, 1) + "%");
        }
        { // T17 (2.7, Carter-corrected 2026-07-15): a clip at 0% weight is
          // NEVER selected; a block whose only clip is at 0% is SKIPPED
          // entirely (zero entries). Inverts the pre-correction assertion.
            Project p;
            auto* blk = p.addBlock("A");
            addClipTo(blk, "only", 1000);
            blk->clips[0]->probability = 0.0f;

            juce::Random r(617); ArrangementResolver res;
            int ok = 0;
            for (int i = 0; i < 20; ++i) {
                auto arr = res.resolve(p, r);
                ok += (arr.entries.size() == 0);
            }
            verdict("T17 single clip @0%: block skipped, zero entries", ok == 20,
                    juce::String(ok) + "/20");
        }
        { // T17b (2.7): mixed weights 50/50/0 -> the 0-weight clip appears 0x
          // across 50 resolves; both positive-weight clips DO appear.
            Project p;
            auto* blk = p.addBlock("A");
            addClipTo(blk, "c50a", 1000);
            addClipTo(blk, "c50b", 1000);
            addClipTo(blk, "c0",   1000);
            blk->clips[0]->probability = 0.5f;
            blk->clips[1]->probability = 0.5f;
            blk->clips[2]->probability = 0.0f;
            const juce::String idA = blk->clips[0]->id;
            const juce::String idB = blk->clips[1]->id;
            const juce::String id0 = blk->clips[2]->id;

            juce::Random r(6171); ArrangementResolver res;
            int zeroCount = 0, aCount = 0, bCount = 0, entriesOk = 0;
            for (int i = 0; i < 50; ++i) {
                auto arr = res.resolve(p, r);
                if (arr.entries.size() == 1) {
                    ++entriesOk;
                    if (arr.entries[0].clipId == id0) ++zeroCount;
                    if (arr.entries[0].clipId == idA) ++aCount;
                    if (arr.entries[0].clipId == idB) ++bCount;
                }
            }
            verdict("T17b weights 50/50/0: 0-weight clip never selected",
                    entriesOk == 50 && zeroCount == 0 && aCount > 0 && bCount > 0,
                    "zero=" + juce::String(zeroCount) + "/50, a=" + juce::String(aCount)
                    + ", b=" + juce::String(bCount));
        }
        { // T18 (5.3): SEQ pc=3 -> all three back-to-back gapless, THEN follower D
          // plays (song continues; nothing swallowed after the stack).
            Project p; buildStack3(p, StackPlayMode::Sequential, 3, false, 1, 1, 1);
            auto* D = p.addBlock("D"); addClipTo(D, "cD", 700);
            juce::Random r(618); ArrangementResolver res;
            int ok = 0; std::set<juce::String> firstBlocks;
            for (int i = 0; i < 10; ++i) {
                auto arr = res.resolve(p, r);
                bool good = (arr.entries.size() == 4);
                if (good) {
                    std::set<juce::String> stackSeen;
                    for (int e = 0; e < 3; ++e) {
                        auto* b = p.getBlockById(arr.entries[e].blockId);
                        if (b) stackSeen.insert(b->name);
                        if (e > 0) {
                            const auto& prev = arr.entries.getReference(e - 1);
                            if (arr.entries[e].timelinePos
                                    != prev.timelinePos + (prev.endMark - prev.startMark))
                                good = false;  // gap or overlap inside the stack run
                        }
                    }
                    if (stackSeen != std::set<juce::String>({"A", "B", "C"})) good = false;
                    auto* last = p.getBlockById(arr.entries[3].blockId);
                    if (!last || last->name != "D") good = false;
                    const auto& e2 = arr.entries.getReference(2);
                    if (arr.entries[3].timelinePos
                            != e2.timelinePos + (e2.endMark - e2.startMark)) good = false;
                    auto* firstB = p.getBlockById(arr.entries[0].blockId);
                    firstBlocks.insert(firstB ? firstB->name : juce::String("?"));
                }
                ok += good;
            }
            verdict("T18 SEQ pc=3 gapless + song continues (D last)",
                    ok == 10 && firstBlocks.size() > 1,
                    juce::String(ok) + "/10, distinct stack orders (first block): "
                    + juce::String((int)firstBlocks.size()));
        }
        { // T19 (8.2): song-ender on a clip of a block INSIDE a stack (SEQ pc=2 of 3).
          // Picked -> that block is the FINAL entry (no D after); not picked -> 2
          // stack entries + D, song continues. Both branches must be observed.
            Project p; buildStack3(p, StackPlayMode::Sequential, 2, false, 1, 1, 1);
            Block* B = nullptr;
            for (auto* b : p.blocks) if (b->name == "B") B = b;
            B->clips[0]->isSongEnder = true;
            auto* D = p.addBlock("D"); addClipTo(D, "cD", 700);

            juce::Random r(619); ArrangementResolver res;
            int okResolves = 0, endedBranch = 0, continuedBranch = 0;
            for (int i = 0; i < 20; ++i) {
                auto arr = res.resolve(p, r);
                int bIdx = -1;
                bool dPresent = false;
                for (int e = 0; e < arr.entries.size(); ++e) {
                    auto* blk = p.getBlockById(arr.entries[e].blockId);
                    if (blk == B) bIdx = e;
                    if (blk == D) dPresent = true;
                }
                bool good;
                if (bIdx >= 0) {  // ender picked: B must be last, nothing after (no D)
                    good = (bIdx == arr.entries.size() - 1) && !dPresent;
                    if (good) ++endedBranch;
                } else {          // ender not picked: 2 stack entries + D, D last
                    good = (arr.entries.size() == 3) && dPresent
                           && p.getBlockById(arr.entries[2].blockId) == D;
                    if (good) ++continuedBranch;
                }
                okResolves += good;
            }
            verdict("T19 song-ender inside stack: truncates when picked, continues when not",
                    okResolves == 20 && endedBranch > 0 && continuedBranch > 0,
                    juce::String(okResolves) + "/20, ended: " + juce::String(endedBranch)
                    + ", continued: " + juce::String(continuedBranch));
        }

        { // T20 (10.5): sample-rate / pitch — a 440 Hz source must stay 440 Hz for
          // every {source rate} x {project rate} combo, through BOTH the load-time
          // resample (Clip::loadFromFile) and the offline export (ExportRenderer).
          // Pitch is measured by positive-going zero crossings interpreted at the
          // relevant sample rate: freq = crossings / (numSamples / rate). This is
          // rate-invariant for correct resampling; a missed resample shows up as
          // freq * (nativeRate/projectRate) (e.g. 440 @48k in a 44.1k project -> 404).
            auto tmpDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("resolverdiag_t20");
            tmpDir.createDirectory();
            juce::AudioFormatManager afm; afm.registerBasicFormats();

            auto measureFreq = [](const juce::AudioBuffer<float>& b, double rate) {
                const float* d = b.getReadPointer(0);
                int n = b.getNumSamples(), crossings = 0;
                for (int i = 1; i < n; ++i)
                    if (d[i - 1] <= 0.0f && d[i] > 0.0f) ++crossings;
                return (double)crossings / ((double)n / rate);
            };
            auto readWav = [&](const juce::File& f, juce::AudioBuffer<float>& out) {
                std::unique_ptr<juce::AudioFormatReader> r(afm.createReaderFor(f));
                if (!r) return 0.0;
                out.setSize((int)r->numChannels, (int)r->lengthInSamples);
                r->read(&out, 0, (int)r->lengthInSamples, 0, true, true);
                return r->sampleRate;
            };

            const double rates[2] = { 44100.0, 48000.0 };
            const double srcFreq  = 440.0;
            const double srcSecs  = 1.0;
            bool allOk = true;
            juce::String detail;

            for (double srcRate : rates) {
                auto srcFile = tmpDir.getChildFile("src_" + juce::String((int)srcRate) + ".wav");
                writeToneWav(srcFile, srcFreq, srcSecs, false, srcRate);

                for (double projRate : rates) {
                    // ── Load-time resample check ──
                    Project p; p.sampleRate = projRate;
                    auto* blk = p.addBlock("A");
                    auto clip = std::make_unique<Clip>();
                    clip->name = "tone";
                    clip->probability = 1.0f;
                    clip->tempo = 120.0;
                    bool loaded = clip->loadFromFile(srcFile, afm, projRate);
                    Clip* rawClip = clip.get();
                    blk->addClip(std::move(clip));

                    double loadFreq = loaded ? measureFreq(*rawClip->audioBuffer, projRate) : 0.0;
                    double loadSecs = loaded ? (double)rawClip->audioBuffer->getNumSamples() / projRate : 0.0;
                    bool loadOk = loaded
                        && std::abs(loadFreq - srcFreq) <= 5.0
                        && std::abs(loadSecs - srcSecs) <= 0.01;

                    // ── Export check ──
                    juce::Random r(2050); ArrangementResolver res;
                    auto arr = res.resolve(p, r);
                    arr.sampleRate = projRate;
                    auto outWav = tmpDir.getChildFile("out_" + juce::String((int)srcRate)
                                       + "_" + juce::String((int)projRate) + ".wav");
                    juce::WavAudioFormat wavFmt;
                    ExportRenderer ex;
                    bool exported = ex.renderToFile(arr, outWav, wavFmt, 16, nullptr);

                    juce::AudioBuffer<float> outBuf;
                    double outRate = exported ? readWav(outWav, outBuf) : 0.0;
                    double outFreq = (outRate > 0.0) ? measureFreq(outBuf, outRate) : 0.0;
                    bool exportOk = exported
                        && std::abs(outRate - projRate) < 1.0
                        && std::abs(outFreq - srcFreq) <= 5.0;

                    allOk = allOk && loadOk && exportOk;
                    detail << "\n      src" << (int)srcRate << "->proj" << (int)projRate
                           << ": load " << juce::String(loadFreq, 1) << "Hz/"
                           << juce::String(loadSecs, 3) << "s " << (loadOk ? "ok" : "BAD")
                           << ", export " << juce::String(outFreq, 1) << "Hz@"
                           << (int)outRate << " " << (exportOk ? "ok" : "BAD");
                }
            }
            verdict("T20 sample-rate/pitch: 440Hz stays 440Hz (load+export, all rate combos)",
                    allOk, detail);
            tmpDir.deleteRecursively();
        }

        // ── PENDING-MANUAL automation batch (2026-07-07, verification only) ───
        // Automates the headless-checkable half of the PENDING-MANUAL queue.
        // Every expected value is derived from CURRENT product code and cited.

        // Shared helpers for the audio tests below.
        // Fill a shared buffer with a full-scale sine so lead-in / body regions
        // are distinguishable from silence (10ms fades to avoid edge clicks).
        auto makeTone = [](int len, double freq, double sr) {
            auto b = std::make_shared<juce::AudioBuffer<float>>(2, len);
            const int fade = juce::jmax(1, (int)(0.010 * sr));
            for (int i = 0; i < len; ++i) {
                // 0.3 peak: two overlapping tones at a crossfade sum to <= ~0.6,
                // safely below 0 dBFS so the 16-bit export never clamps (see
                // compareEngineExport note).
                float v = 0.3f * (float)std::sin(2.0 * juce::MathConstants<double>::pi
                                                 * freq * i / sr);
                if (i < fade)        v *= (float)i / fade;
                if (i >= len - fade) v *= (float)(len - 1 - i) / fade;
                b->setSample(0, i, v);
                b->setSample(1, i, v);
            }
            return b;
        };

        { // T21 (6.1): clip "Mark as Done" is COSMETIC — the resolver's pickClip
          // never references isDone, so toggling it must not change WHICH clip is
          // picked (bit-identical sequence under the same seed) AND a done clip
          // must still be selectable by weight. Invariant guarded: 6.1 / 6.5.
            Project p;
            auto* blk = p.addBlock("A");
            addClipTo(blk, "heavy", 1000);
            addClipTo(blk, "mid",   1000);
            addClipTo(blk, "low",   1000);
            blk->clips[0]->probability = 0.6f;
            blk->clips[1]->probability = 0.3f;
            blk->clips[2]->probability = 0.1f;

            auto pickSeq = [&](juce::int64 seed) {
                juce::Random r(seed);
                juce::String s;
                int heavyHits = 0;
                for (int i = 0; i < 200; ++i) {
                    auto* c = ArrangementResolver::pickClip(*blk, r);
                    s << (c ? c->name : juce::String("?")) << ";";
                    if (c && c->name == "heavy") ++heavyHits;
                }
                return std::make_pair(s, heavyHits);
            };

            auto before = pickSeq(6210);
            blk->clips[0]->isDone = true;   // done on the heaviest clip
            blk->clips[2]->isDone = true;
            auto after = pickSeq(6210);
            verdict("T21 clip isDone cosmetic: identical picks + done clip still selected",
                    before.first == after.first && after.second > 0,
                    juce::String("sequence identical: ") + (before.first == after.first ? "yes" : "NO")
                    + ", done-heavy inclusions: " + juce::String(after.second) + "/200");
        }

        { // T22 (6.4): Export with EVERYTHING marked Done must export normally —
          // isDone is not consulted anywhere on the export path, so an all-Done
          // project renders byte-identical audio to the same project with nothing
          // done, and renderToFile still succeeds. Invariant guarded: 6.4 / 6.5.
            auto buildExportProject = [&](Project& p, bool markDone) {
                p.sampleRate = 44100.0;
                auto* a = p.addBlock("A");
                auto* b = p.addBlock("B");
                addClipTo(a, "cA", 1000);
                addClipTo(b, "cB", 1200);
                a->clips[0]->audioBuffer = makeTone(1000, 330.0, 44100.0);
                b->clips[0]->audioBuffer = makeTone(1200, 550.0, 44100.0);
                a->clips[0]->endMark = 1000;
                b->clips[0]->endMark = 1200;
                if (markDone) {
                    for (auto* blk : p.blocks) {
                        blk->isDone = true;
                        for (auto* cl : blk->clips) cl->isDone = true;
                    }
                }
            };
            auto renderExport = [&](bool markDone, juce::AudioBuffer<float>& out) {
                Project p; buildExportProject(p, markDone);
                juce::Random r(6220); ArrangementResolver res;
                auto arr = res.resolve(p, r);
                arr.sampleRate = p.sampleRate;
                auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("resolverdiag_t22_" + juce::String(markDone ? "done" : "plain") + ".wav");
                juce::WavAudioFormat wavFmt; ExportRenderer ex;
                bool ok = ex.renderToFile(arr, f, wavFmt, 16, nullptr);
                juce::AudioFormatManager afm; afm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> rd(afm.createReaderFor(f));
                if (rd) { out.setSize((int)rd->numChannels, (int)rd->lengthInSamples);
                          rd->read(&out, 0, (int)rd->lengthInSamples, 0, true, true); }
                int entries = arr.entries.size();
                f.deleteFile();
                return std::make_pair(ok && rd != nullptr, entries);
            };

            juce::AudioBuffer<float> plain, allDone;
            auto rp = renderExport(false, plain);
            auto rd = renderExport(true,  allDone);
            bool sameLen = plain.getNumSamples() == allDone.getNumSamples()
                           && plain.getNumSamples() > 0;
            float maxDiff = 0.0f;
            if (sameLen)
                for (int ch = 0; ch < plain.getNumChannels(); ++ch)
                    for (int i = 0; i < plain.getNumSamples(); ++i)
                        maxDiff = juce::jmax(maxDiff,
                            std::abs(plain.getSample(ch, i) - allDone.getSample(ch, i)));
            verdict("T22 export all-Done: succeeds + audio identical to none-Done",
                    rp.first && rd.first && rp.second == 2 && rd.second == 2
                    && sameLen && maxDiff == 0.0f,
                    "exportOk plain/done: " + juce::String(rp.first ? "y" : "n") + "/"
                    + juce::String(rd.first ? "y" : "n") + ", entries: "
                    + juce::String(rp.second) + "/" + juce::String(rd.second)
                    + ", sameLen: " + (sameLen ? "y" : "n")
                    + ", maxSampleDiff: " + juce::String(maxDiff, 8));
        }

        { // T23 (7.7): Play Clip on a clip WITH a lead-in must not cut the lead-in.
          // Reproduces MainComponent::playClip exactly (MainComponent.cpp:394-408):
          // single entry, timelinePos = startMark, totalDurationSamples = endMark,
          // gain 1.0 → the render buffer's [0, startMark) region is the lead-in at
          // full gain (EntryMixer.h:106, entryIndex 0). Invariant guarded: 7.7 lead-in.
            const double sr = 44100.0;
            const int64_t startMark = (int64_t)(0.30 * sr);   // 0.30s lead-in
            const int64_t endMark   = (int64_t)(1.00 * sr);   // body ends at 1.00s
            auto tone = makeTone((int)endMark, 440.0, sr);

            // Build the single-entry arrangement the way playClip does.
            ResolvedEntry entry;
            entry.audioBuffer       = tone;
            entry.startMark         = startMark;
            entry.endMark           = endMark;
            entry.originalStartMark = startMark;
            entry.retainTailTempo   = false;
            entry.clipName          = "tone";
            entry.clipId            = "t23";
            entry.timelinePos       = startMark;   // body starts after lead-in
            entry.gain              = 1.0f;
            entry.blockId           = "b23";
            ResolvedArrangement single;
            single.sampleRate           = sr;
            single.totalDurationSamples = endMark; // includes lead-in + body
            single.entries.add(entry);

            auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("resolverdiag_t23.wav");
            juce::WavAudioFormat wavFmt; ExportRenderer ex;
            bool ok = ex.renderToFile(single, f, wavFmt, 16, nullptr);
            juce::AudioFormatManager afm; afm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> rd(afm.createReaderFor(f));
            juce::AudioBuffer<float> out;
            double outLen = 0.0, leadRms = 0.0, leadMaxDiff = 0.0;
            if (rd) {
                out.setSize((int)rd->numChannels, (int)rd->lengthInSamples);
                rd->read(&out, 0, (int)rd->lengthInSamples, 0, true, true);
                outLen = (double)out.getNumSamples();
                double acc = 0.0;
                for (int i = 0; i < (int)startMark; ++i) {
                    float s = out.getSample(0, i);
                    acc += (double)s * s;
                    leadMaxDiff = juce::jmax(leadMaxDiff,
                        (double)std::abs(s - tone->getSample(0, i)));
                }
                leadRms = std::sqrt(acc / (double)startMark);
            }
            f.deleteFile();
            // Lead-in present (RMS well above silence), matches the source lead-in
            // within 16-bit quantisation, and total length == endMark (not clipped).
            verdict("T23 Play Clip lead-in: [0,startMark) audible + equals source, not cut",
                    ok && rd != nullptr
                    && std::abs(outLen - (double)endMark) < 1.0
                    && leadRms > 0.1 && leadMaxDiff < 1.0e-3,
                    "len=" + juce::String(outLen, 0) + "/" + juce::String((double)endMark, 0)
                    + " leadRms=" + juce::String(leadRms, 4)
                    + " leadMaxDiff=" + juce::String(leadMaxDiff, 6));
        }

        // Shared driver for T24/T25: render a resolved arrangement through the REAL
        // PlaybackEngine block-by-block (the in-editor path) at the project rate,
        // and separately through ExportRenderer (the export path). Both delegate to
        // the single EntryMixer.h::mixEntryToBuffer, so at project rate they must be
        // sample-identical (export file only adds 16-bit quantisation). This is the
        // headless core of "export identical to in-editor playback" (10.2 / 10.4).
        auto renderViaEngine = [](const ResolvedArrangement& arr, int blockSize) {
            juce::AudioBuffer<float> out(2, (int)arr.totalDurationSamples);
            out.clear();
            PlaybackEngine eng;
            eng.prepareToPlay(arr.sampleRate, blockSize);   // hardware rate == project rate
            eng.play(arr);
            juce::AudioBuffer<float> tmp(2, blockSize);
            for (int64_t pos = 0; pos < arr.totalDurationSamples; pos += blockSize) {
                tmp.clear();
                eng.getNextAudioBlock(tmp, blockSize);
                int thisLen = (int)juce::jmin((int64_t)blockSize, arr.totalDurationSamples - pos);
                for (int ch = 0; ch < 2; ++ch)
                    out.copyFrom(ch, (int)pos, tmp, ch, 0, thisLen);
            }
            return out;
        };
        auto renderViaExport = [](const ResolvedArrangement& arr, juce::AudioBuffer<float>& out) {
            auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("resolverdiag_playexport.wav");
            juce::WavAudioFormat wavFmt; ExportRenderer ex;
            bool ok = ex.renderToFile(arr, f, wavFmt, 16, nullptr);
            juce::AudioFormatManager afm; afm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> rd(afm.createReaderFor(f));
            if (rd) { out.setSize((int)rd->numChannels, (int)rd->lengthInSamples);
                      rd->read(&out, 0, (int)rd->lengthInSamples, 0, true, true); }
            double rate = rd ? rd->sampleRate : 0.0;
            f.deleteFile();
            return std::make_pair(ok && rd != nullptr, rate);
        };
        // Compares the REAL in-editor path (PlaybackEngine, rendered block-by-block
        // at 512 samples) against the REAL export path (ExportRenderer → 16-bit WAV
        // read back). Both delegate to the single EntryMixer.h::mixEntryToBuffer, so
        // at project rate they are sample-identical up to the WAV writer's 16-bit
        // quantisation. NOTE: test tones are kept well below 0 dBFS so their sum in
        // the crossfade never exceeds ±1.0 — otherwise the 16-bit writer would clamp
        // (a sink artifact, not a mixer difference) and mask the real comparison.
        auto compareEngineExport = [&](const ResolvedArrangement& arr) {
            auto eng = renderViaEngine(arr, 512);
            juce::AudioBuffer<float> exp;
            auto er = renderViaExport(arr, exp);
            float maxDiff = 1.0f, peak = 0.0f; double engRms = 0.0;
            bool sameLen = er.first && eng.getNumSamples() == exp.getNumSamples()
                           && eng.getNumSamples() > 0;
            if (sameLen) {
                maxDiff = 0.0f; double acc = 0.0; int nCh = juce::jmin(eng.getNumChannels(), exp.getNumChannels());
                for (int ch = 0; ch < nCh; ++ch)
                    for (int i = 0; i < eng.getNumSamples(); ++i) {
                        float e = eng.getSample(ch, i);
                        maxDiff = juce::jmax(maxDiff, std::abs(e - exp.getSample(ch, i)));
                        peak    = juce::jmax(peak, std::abs(e));
                        acc += (double)e * e;
                    }
                engRms = std::sqrt(acc / (double)(eng.getNumSamples() * nCh));
            }
            struct R { bool sameLen; float maxDiff; double engRms; float peak; double exportRate; bool exportRateOk; };
            return R{ sameLen, maxDiff, engRms, peak, er.second, std::abs(er.second - arr.sampleRate) < 1.0 };
        };

        { // T24 (10.2): plain sequential arrangement (no tempo stretch) — export
          // audio must equal in-editor playback sample-for-sample (within 16-bit).
            Project p; p.sampleRate = 44100.0;
            auto* a = p.addBlock("A");
            auto* b = p.addBlock("B");
            addClipTo(a, "cA", 1000);
            addClipTo(b, "cB", 1200);
            a->clips[0]->audioBuffer = makeTone(1000, 330.0, 44100.0);
            b->clips[0]->audioBuffer = makeTone(1200, 550.0, 44100.0);
            a->clips[0]->endMark = 1000; b->clips[0]->endMark = 1200;
            juce::Random r(2400); ArrangementResolver res;
            auto arr = res.resolve(p, r); arr.sampleRate = p.sampleRate;
            auto cr = compareEngineExport(arr);
            verdict("T24 export==playback (plain): engine vs exported WAV identical",
                    cr.sameLen && cr.exportRateOk && cr.engRms > 0.01
                    && cr.peak < 1.0f && cr.maxDiff < 1.0e-3,
                    "sameLen: " + juce::String(cr.sameLen ? "y" : "n")
                    + ", exportRate=" + juce::String(cr.exportRate, 0) + (cr.exportRateOk ? " ok" : " BAD")
                    + ", peak=" + juce::String(cr.peak, 3)
                    + ", engRms=" + juce::String(cr.engRms, 4)
                    + ", maxDiff=" + juce::String(cr.maxDiff, 6));
        }

        { // T25 (10.4): arrangement WITH a tempo-stretched join — Intro tempo 120
          // (tail) into Verse tempo 160 (lead-in), so the resolver pre-stretches
          // the tail/lead-in (ArrangementResolver.cpp:343-376). Export must still
          // equal in-editor playback sample-for-sample through the shared mixer.
            const double sr = 44100.0;
            Project p; p.sampleRate = sr;
            auto* intro = p.addBlock("Intro");
            auto* verse = p.addBlock("Verse");
            // Intro: body 0..0.8s, then a 0.4s TAIL (buffer longer than endMark), tempo 120.
            auto introBuf = makeTone((int)(1.2 * sr), 440.0, sr);
            addClipTo(intro, "intro", introBuf->getNumSamples());
            intro->clips[0]->audioBuffer = introBuf;
            intro->clips[0]->startMark = 0;
            intro->clips[0]->endMark   = (int64_t)(0.8 * sr);
            intro->clips[0]->tempo     = 120.0;
            // Verse: 0.4s LEAD-IN then body to 1.2s, tempo 160.
            auto verseBuf = makeTone((int)(1.2 * sr), 220.0, sr);
            addClipTo(verse, "verse", verseBuf->getNumSamples());
            verse->clips[0]->audioBuffer = verseBuf;
            verse->clips[0]->startMark = (int64_t)(0.4 * sr);
            verse->clips[0]->endMark   = (int64_t)(1.2 * sr);
            verse->clips[0]->tempo     = 160.0;

            juce::Random r(2500); ArrangementResolver res;
            auto arr = res.resolve(p, r); arr.sampleRate = sr;
            // Confirm a stretch actually happened (else the test is vacuous).
            bool stretched = false;
            for (const auto& e : arr.entries)
                if (e.stretchedLeadIn || e.stretchedTail
                    || std::abs(e.leadInStretchRatio - 1.0f) > 0.001f
                    || std::abs(e.tailStretchRatio - 1.0f) > 0.001f)
                    stretched = true;
            auto cr = compareEngineExport(arr);
            verdict("T25 export==playback (tempo-stretched join): identical + stretch present",
                    stretched && cr.sameLen && cr.exportRateOk
                    && cr.engRms > 0.01 && cr.peak < 1.0f && cr.maxDiff < 1.0e-3,
                    "stretchPresent: " + juce::String(stretched ? "y" : "n")
                    + ", sameLen: " + juce::String(cr.sameLen ? "y" : "n")
                    + ", peak=" + juce::String(cr.peak, 3)
                    + ", engRms=" + juce::String(cr.engRms, 4)
                    + ", maxDiff=" + juce::String(cr.maxDiff, 6));
        }

        { // T26 (13.2): nine-block stack visibility. Mirrors the BlockStrip tile
          // layout arithmetic (BlockStrip.cpp:191-193): perH = jmax(16, (areaH -
          // (n-1)*4) / n); neededH = perH*n + (n-1)*4. Strip height areaH = 360
          // (MainComponent.h:70 blockStripHeight, reduced(padding,0) leaves height
          // untouched → BlockStrip.cpp:145 areaH = 360). A stack "fits" (all tiles
          // on screen) when neededH <= areaH; otherwise perH clamps to 16 and the
          // content overflows into the vertical scrollbar (setScrollBarsShown true)
          // → still reachable. Invariant guarded: 13.2 tiles visible/reachable.
            const int areaH = 360;
            auto perH    = [](int n, int aH){ return juce::jmax(16, (aH - (n - 1) * 4) / n); };
            auto neededH = [&](int n, int aH){ return perH(n, aH) * n + (n - 1) * 4; };
            bool fit7 = neededH(7, areaH) <= areaH;
            bool fit8 = neededH(8, areaH) <= areaH;
            bool fit9 = neededH(9, areaH) <= areaH;
            // A very tall stack must clamp to the 16px floor and overflow → scrolls.
            const int big = 30;
            bool clampsAndScrolls = (perH(big, areaH) == 16) && (neededH(big, areaH) > areaH);
            verdict("T26 nine-block stack: n=7/8/9 all fit at 360px; large stacks scroll",
                    fit7 && fit8 && fit9 && clampsAndScrolls,
                    "perH 7/8/9 = " + juce::String(perH(7, areaH)) + "/" + juce::String(perH(8, areaH))
                    + "/" + juce::String(perH(9, areaH)) + ", neededH 7/8/9 = "
                    + juce::String(neededH(7, areaH)) + "/" + juce::String(neededH(8, areaH))
                    + "/" + juce::String(neededH(9, areaH)) + " (<=360), big(" + juce::String(big)
                    + ") perH=" + juce::String(perH(big, areaH)) + " neededH="
                    + juce::String(neededH(big, areaH)));
        }

        { // T27 (13.3): grid lines never obscure the waveform on long clips. Mirrors
          // the adaptive-grid coarsening loop (ClipWaveformView.cpp:174-197): the grid
          // spacing is doubled until pixelsPerLine >= 8, and lines are drawn ONLY when
          // pixelsPerLine >= 8. So for any clip the on-screen grid is never denser than
          // 8px. Verified for a very long clip (5 min) and a short clip. Guarded: 13.3.
            auto finalPixelsPerLine = [](int64_t total, double tempo, double sr, int waWidth) {
                double spb = (sr * 60.0) / tempo;
                if (total <= 0 || waWidth <= 0 || spb <= 0.0) return 0.0;   // no grid
                double pixelsPerSample = (double)waWidth / (double)total;
                double drawSpb = spb;
                double ppl = drawSpb * pixelsPerSample;
                while (ppl < 8.0 && drawSpb < (double)total) { drawSpb *= 2.0; ppl *= 2.0; }
                return (ppl >= 8.0) ? ppl : 0.0;   // 0 → grid suppressed (not drawn)
            };
            const int waWidth = 800;
            double pplLong  = finalPixelsPerLine((int64_t)(300.0 * 44100.0), 120.0, 44100.0, waWidth); // 5 min
            double pplHuge  = finalPixelsPerLine((int64_t)(3600.0 * 44100.0), 174.0, 48000.0, waWidth); // 1 hr, odd tempo
            double pplShort = finalPixelsPerLine((int64_t)(2.0 * 44100.0), 120.0, 44100.0, waWidth);
            // Drawn grids must be >= 8px apart; suppressed grids read as 0.
            bool ok = (pplLong == 0.0 || pplLong >= 8.0)
                   && (pplHuge == 0.0 || pplHuge >= 8.0)
                   && (pplShort == 0.0 || pplShort >= 8.0);
            verdict("T27 adaptive grid: on-screen spacing never < 8px (long clips)",
                    ok,
                    "pixelsPerLine long/huge/short = " + juce::String(pplLong, 2) + "/"
                    + juce::String(pplHuge, 2) + "/" + juce::String(pplShort, 2)
                    + " (0 = grid suppressed)");
        }

        { // T28 (13.4, Carter-corrected 2026-07-15): the DEEPEST zoom window is
          // a CONSTANT ~0.5 s regardless of clip length. Mirrors
          // ClipWaveformView::computeMaxZoom: maxZoom = jlimit(1, 65536, dur/0.5).
          // The old 256 cap (asserted by the pre-correction T28) left clips over
          // 128 s unable to reach the beat window; the cap is now 65536, an
          // arithmetic-safety bound only (0.5 s window preserved up to ~9 h).
            auto maxZoom = [](double durSecs) {
                if (durSecs < 0.001) return 32.0;                 // empty/tiny fallback
                double z = durSecs / 0.5;
                z = juce::jmax(z, 1.0);
                z = juce::jmin(z, 65536.0);
                return z;
            };
            // Constant-window property: dur / maxZoom(dur) == 0.5 +/- eps for
            // short AND long clips (2 s / 30 s / 300 s / 3000 s).
            bool window = true;
            juce::String ev;
            for (double dur : { 2.0, 30.0, 300.0, 3000.0 }) {
                double w = dur / maxZoom(dur);
                window = window && std::abs(w - 0.5) < 1e-6;
                ev << "win(" << juce::String(dur, 0) << "s)=" << juce::String(w, 4) << " ";
            }
            // Tiny-clip lower clamp unchanged: a 0.3 s clip pins at zoom 1 and
            // just shows itself whole; safety cap engages only at 65536.
            double zTiny = maxZoom(0.3);
            double zVast = maxZoom(100000.0);
            bool clamps  = zTiny == 1.0 && zVast == 65536.0;
            verdict("T28 zoom-in window constant ~0.5s at any clip length",
                    window && clamps,
                    ev + "z(0.3s)=" + juce::String(zTiny, 1)
                    + " z(100000s)=" + juce::String(zVast, 1));
        }
        { // T47 (13.4 perf backstop): the clip-region-aware waveform paint must
          // keep its column->sample mapping BIT-IDENTICAL to the full-width
          // math. Mirrors ClipRowComponent::renderWaveform: full path computes
          // every column px in [0,w); clipped path computes px in
          // [pxFirst, pxLast] with results stored at (px - pxFirst). Same
          // absolute px and same full width w in both -> s0/s1/min/max must
          // match exactly for every column, at several zooms and clip offsets.
            juce::Random tr(4747);
            const int numSamples = 262144;                    // ~5.9s @44.1k
            juce::AudioBuffer<float> buf(2, numSamples);
            for (int ch = 0; ch < 2; ++ch) {
                float* d = buf.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    d[i] = tr.nextFloat() * 2.0f - 1.0f;      // deterministic noise
            }
            auto colMinMax = [&](int px, int w, float& mn, float& mx) {
                int s0 = (int)((int64_t)px * numSamples / w);
                int s1 = juce::jmin((int)((int64_t)(px + 1) * numSamples / w), numSamples - 1);
                if (s1 < s0) s1 = s0;
                mn = 0.0f; mx = 0.0f;
                for (int ch = 0; ch < 2; ++ch) {
                    const float* data = buf.getReadPointer(ch);
                    for (int q = s0; q <= s1; ++q) {
                        mn = juce::jmin(mn, data[q]);
                        mx = juce::jmax(mx, data[q]);
                    }
                }
                return std::pair<int,int>(s0, s1);
            };
            bool allEqual = true; int checked = 0; juce::String ev2;
            const int visW = 1100;
            for (float zoom : { 1.0f, 4.0f, 60.0f, 1200.0f, 65536.0f }) {
                const int w = (int)((double)visW * zoom);     // full row width
                // three dirty regions: start, middle, near right edge
                for (int visX : { 0, w / 2, juce::jmax(0, w - visW - 7) }) {
                    const int pxFirst = juce::jmax(0, visX - 1);
                    const int pxLast  = juce::jmin(w - 1, visX + visW);
                    // FULL path: absolute px. CLIPPED path: same px, offset store.
                    // Compare first, middle, last columns of the region.
                    for (int px : { pxFirst, (pxFirst + pxLast) / 2, pxLast }) {
                        float mnF, mxF, mnC, mxC;
                        auto rF = colMinMax(px, w, mnF, mxF);           // full-width math
                        auto rC = colMinMax((pxFirst + (px - pxFirst)), w, mnC, mxC); // clipped indexing round-trip
                        allEqual = allEqual && rF == rC && mnF == mnC && mxF == mxC;
                        ++checked;
                    }
                }
                if (zoom == 1200.0f) {  // print measured values at the 10-min-like depth
                    float mn, mx; auto r = colMinMax(visW * 600, w, mn, mx);
                    ev2 << "z1200 px=" << juce::String(visW * 600) << " s0=" << juce::String(r.first)
                        << " s1=" << juce::String(r.second)
                        << " mn=" << juce::String(mn, 6) << " mx=" << juce::String(mx, 6) << "; ";
                }
            }
            verdict("T47 clipped==full column mapping (all zooms/regions)",
                    allEqual && checked == 45,
                    ev2 + juce::String(checked) + "/45 columns identical");
        }

        { // T29 (6.3): the "DONE" badge must not cover the block name. Mirrors the
          // BlockComponent geometry: tinyTile = height <= 26 (BlockComponent.cpp:62);
          // the badge is drawn ONLY for non-tiny tiles at localBounds.removeFromBottom(16)
          // .removeFromRight(40) (BlockComponent.cpp:188-189); the name label for a
          // full tile is localBounds.reduced(4).withTrimmedTop(20).withTrimmedBottom(16)
          // (BlockComponent.cpp:205) — its bottom is trimmed by exactly the 16px badge
          // band, so name and badge are always disjoint. Tiny tiles draw no badge at all
          // (BlockComponent.cpp:202 name spans reduced(2)). Guarded: 6.3 badge vs name.
            auto nameBounds = [](int w, int h) {
                if (h <= 26) return juce::Rectangle<int>(0, 0, w, h).reduced(2);       // tiny path
                return juce::Rectangle<int>(0, 0, w, h).reduced(4)
                           .withTrimmedTop(20).withTrimmedBottom(16);                  // full path
            };
            auto badgeBounds = [](int w, int h) {
                return juce::Rectangle<int>(0, 0, w, h).removeFromBottom(16).removeFromRight(40);
            };
            bool ok = true; juce::String detail;
            const int widths[]  = { 70, 90, 120 };
            const int heights[] = { 16, 20, 26, 28, 40, 60, 120, 360 };
            for (int w : widths) for (int h : heights) {
                bool tiny = (h <= 26);
                auto nb = nameBounds(w, h);
                if (!tiny) {                                    // badge only exists for full tiles
                    auto bb = badgeBounds(w, h);
                    if (nb.intersects(bb)) {
                        ok = false;
                        detail << " OVERLAP@" << w << "x" << h
                               << " name=" << nb.toString() << " badge=" << bb.toString();
                    }
                }
            }
            verdict("T29 DONE badge never covers name: name/badge rects disjoint (full tiles)",
                    ok, ok ? juce::String("all ") + juce::String((int)(3 * 8))
                             + " (w x h) combos disjoint; tiny tiles (<=26px) draw no badge"
                           : detail);
        }

        { // T30 (12.2): cross-platform audio paths. A .bsp must (a) store audio paths
          // RELATIVE with forward slashes, and (b) load whether the stored path uses
          // '/' (macOS/Linux/current) or '\' (a Windows-written .bsp). Save side:
          // Serialization.cpp:53 getRelativePathFrom(...).replaceCharacter('\\','/');
          // load side normalises '\'→'/' so getChildFile resolves on every platform.
            auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                           .getChildFile("resolverdiag_t30");
            tmp.deleteRecursively(); tmp.createDirectory();
            auto media = tmp.getChildFile("media"); media.createDirectory();

            Project p; p.sampleRate = 44100.0;
            auto* blk = p.addBlock("A");
            addClipWithFile(blk, "tone", 1000, media);   // writes media/tone.wav, sets audioFile
            auto bsp = tmp.getChildFile("proj.bsp");
            bool saved = p.saveToFile(bsp);

            // (a) read back the serialized audioFile string
            auto parsed = juce::JSON::parse(bsp.loadFileAsString());
            juce::String storedPath;
            if (auto* barr = parsed.getProperty("blocks", {}).getArray())
                if (barr->size() > 0)
                    if (auto* carr = (*barr)[0].getProperty("clips", {}).getArray())
                        if (carr->size() > 0)
                            storedPath = (*carr)[0].getProperty("audioFile", "").toString();
            bool relOk = saved && storedPath.isNotEmpty()
                         && !storedPath.startsWithChar('/')                       // not POSIX-absolute
                         && !(storedPath.length() >= 2 && storedPath[1] == ':')   // not Windows-absolute
                         && storedPath.contains("/")                              // has a forward slash
                         && !storedPath.contains("\\");                           // and no backslash

            // (b1) load the forward-slash .bsp → clip resolves
            Project pf; bool lf = pf.loadFromFile(bsp);
            bool fwdOk = lf && pf.missingFilesOnLoad.isEmpty()
                         && pf.blocks.size() == 1 && pf.blocks[0]->clips.size() == 1
                         && pf.blocks[0]->clips[0]->audioBuffer != nullptr;

            // (b2) write a BACKSLASH variant of the same .bsp → must still resolve.
            // A Windows-written .bsp stores the backslash JSON-escaped ("\\"), which
            // juce::JSON::parse decodes to a single '\' in the path value — so we
            // replace '/' with the two-char escape "\\" in the raw JSON text.
            auto bspBk = tmp.getChildFile("proj_backslash.bsp");
            bspBk.replaceWithText(bsp.loadFileAsString()
                                     .replace(storedPath, storedPath.replace("/", "\\\\")));
            Project pb; bool lb = pb.loadFromFile(bspBk);
            bool bkOk = lb && pb.missingFilesOnLoad.isEmpty()
                        && pb.blocks.size() == 1 && pb.blocks[0]->clips.size() == 1
                        && pb.blocks[0]->clips[0]->audioBuffer != nullptr;

            verdict("T30 cross-platform paths: relative+forward-slash; loads '/' and '\\'",
                    relOk && fwdOk && bkOk,
                    "stored=\"" + storedPath + "\" relOk=" + (relOk ? "y" : "N")
                    + " fwdLoad=" + (fwdOk ? "y" : "N") + " backslashLoad=" + (bkOk ? "y" : "N")
                    + " (missing fwd/bk: " + juce::String(pf.missingFilesOnLoad.size())
                    + "/" + juce::String(pb.missingFilesOnLoad.size()) + ")");
            tmp.deleteRecursively();
        }

        { // T31 (12.1 crash DIAGNOSIS): replay the MODEL-level mutations of the four
          // drag ops (reorder / stack-in / unstack / shift-drag-whole-stack) in a
          // randomized loop on a 50-block, 5-stacked-pair project (== StressProject),
          // interleaved with undo/redo (resetAndLoad). Purpose: prove whether the
          // OwnedArray<Block> ownership is double-freed at the MODEL layer. Runs under
          // ASan when built with -fsanitize=address. Each op re-fetches Block* by id
          // (undo's resetAndLoad invalidates raw pointers) and calls toJSON() after,
          // mimicking a repaint reading the model.
            Project p; p.sampleRate = 44100.0;
            std::vector<juce::String> ids;
            for (int i = 1; i <= 50; ++i) {
                auto* b = p.addBlock("Block " + juce::String(i));
                addClipTo(b, "c" + juce::String(i), 1000);
                ids.push_back(b->id);
            }
            for (int i = 10; i <= 50; i += 10)     // 5 stacked pairs: (10,9)(20,19)...
                p.stackBlocks(ids[(size_t)(i - 1)], ids[(size_t)(i - 2)]);

            juce::Random r(31031);
            int ops = 0;
            for (int iter = 0; iter < 3000; ++iter) {
                int op = r.nextInt(6);
                if (op == 0 && p.blocks.size() >= 2) {          // (a) reorder
                    p.moveBlock(r.nextInt(p.blocks.size()), r.nextInt(p.blocks.size()));
                } else if (op == 1 && p.blocks.size() >= 2) {   // (b) stack-in
                    int i = r.nextInt(p.blocks.size()), j = r.nextInt(p.blocks.size());
                    if (i != j) p.stackBlocks(p.blocks[i]->id, p.blocks[j]->id);
                } else if (op == 2) {                            // (c) unstack + dissolve-solo
                    Block* dragged = nullptr;
                    for (auto* b : p.blocks) if (b->stackGroup >= 0) { dragged = b; break; }
                    if (dragged) {
                        auto pre = p.toJSON();
                        int oldGroup = dragged->stackGroup;
                        dragged->stackGroup = -1;
                        int remaining = 0; Block* last = nullptr;
                        for (auto* b : p.blocks) if (b->stackGroup == oldGroup) { ++remaining; last = b; }
                        if (remaining == 1 && last) last->stackGroup = -1;
                        else if (remaining > 1) p.propagateStackSettings(oldGroup);
                        int from = p.blocks.indexOf(dragged);
                        int dest = juce::jlimit(0, p.blocks.size() - 1, r.nextInt(p.blocks.size()));
                        if (from >= 0 && from != dest) p.blocks.move(from, dest);
                        for (int k = 0; k < p.blocks.size(); ++k) p.blocks[k]->position = k;
                        p.applyExternalMutation(pre);
                    }
                } else if (op == 3) {                            // (d) shift-drag whole stack
                    int sg = -1;
                    for (auto* b : p.blocks) if (b->stackGroup >= 0) { sg = b->stackGroup; break; }
                    if (sg >= 0) {
                        auto pre = p.toJSON();
                        juce::Array<int> stackIndices;
                        for (int k = 0; k < p.blocks.size(); ++k)
                            if (p.blocks[k]->stackGroup == sg) stackIndices.add(k);
                        juce::OwnedArray<Block> extracted;           // <-- transient co-owner
                        for (int k = stackIndices.size() - 1; k >= 0; --k)
                            extracted.insert(0, p.blocks.removeAndReturn(stackIndices[k]));
                        int insertPos = juce::jlimit(0, p.blocks.size(), r.nextInt(p.blocks.size() + 1));
                        int count = extracted.size();
                        for (int k = 0; k < count; ++k)
                            p.blocks.insert(insertPos + k, extracted.removeAndReturn(0));
                        for (int k = 0; k < p.blocks.size(); ++k) p.blocks[k]->position = k;
                        p.applyExternalMutation(pre);
                    }
                } else if (op == 4) {                            // undo (resetAndLoad(before))
                    if (p.undoManager.canUndo()) p.undoManager.undo();
                } else {                                          // redo (resetAndLoad(after))
                    if (p.undoManager.canRedo()) p.undoManager.redo();
                }
                auto snap = p.toJSON();  // simulate a repaint reading the model
                if (juce::JSON::toString(snap).length() < 0) std::cout << "x";  // force use
                ++ops;
            }
            verdict("T31 drag-op MODEL replay (50 blocks/5 stacks, 3000 ops, ASan-ready)",
                    p.blocks.size() > 0,
                    juce::String(ops) + " ops applied, final blocks=" + juce::String(p.blocks.size())
                    + " (no model-level double-free if this line prints)");
        }

        { // T32 (1c.1): a STACKED block dropped onto a STANDALONE block forms a
          // FRESH two-block stack {dragged, target}. The old mate auto-unstacks
          // (last-remaining, settings reset), the target is NOT absorbed into the
          // old stack, and no third block joins the new pair.
            Project p;
            auto* A = p.addBlock("A"); auto* B = p.addBlock("B");
            auto* C = p.addBlock("C"); auto* D = p.addBlock("D");
            addClipTo(A, "cA"); addClipTo(B, "cB"); addClipTo(C, "cC"); addClipTo(D, "cD");
            const juce::String aId = A->id, bId = B->id, cId = C->id, dId = D->id;
            p.stackBlocks(aId, bId);                       // S = {A,B}

            p.restackBlockOnto(aId, cId);                  // drop A onto standalone C

            auto* a = p.getBlockById(aId); auto* b = p.getBlockById(bId);
            auto* c = p.getBlockById(cId); auto* d = p.getBlockById(dId);
            // "Fresh pair, no absorption" = the new group's members are EXACTLY
            // {A,C} by identity. (Group IDs are recycled — maxGroup+1 — so the
            // dissolved S's id may be reused; identity, not id, is the invariant.)
            int newGroup = a->stackGroup;
            juce::StringArray memberNames;
            for (auto* blk : p.blocks)
                if (blk->stackGroup == newGroup) memberNames.add(blk->name);
            bool pairOk   = newGroup >= 0 && c->stackGroup == newGroup
                         && memberNames.size() == 2
                         && memberNames.contains("A") && memberNames.contains("C");
            bool mateSolo = b->stackGroup == -1
                         && b->stackPlayCount.values.size() == 1
                         && b->stackPlayCount.values[0] == 1
                         && b->stackPlayMode == StackPlayMode::Sequential;
            bool noThird  = d->stackGroup == -1 && p.blocks.size() == 4;
            verdict("T32 stacked->standalone drop: fresh {A,C}, mate solo, no absorption",
                    pairOk && mateSolo && noThird,
                    "members={" + memberNames.joinIntoString(",") + "} (want {A,C})"
                    + ", B.sg=" + juce::String(b->stackGroup)
                    + ", D.sg=" + juce::String(d->stackGroup));
        }

        { // T33 (1c.2): a STACKED block dropped onto a block in ANOTHER stack JOINS
          // that stack; its old stack keeps its remaining members; nothing is lost.
            Project p;
            auto* A = p.addBlock("A"); auto* B = p.addBlock("B"); auto* E = p.addBlock("E");
            auto* C = p.addBlock("C"); auto* D = p.addBlock("D");
            addClipTo(A, "cA"); addClipTo(B, "cB"); addClipTo(E, "cE");
            addClipTo(C, "cC"); addClipTo(D, "cD");
            const juce::String aId = A->id, bId = B->id, eId = E->id, cId = C->id, dId = D->id;
            p.stackBlocks(aId, bId); p.stackBlocks(eId, aId);   // S  = {A,B,E}
            p.stackBlocks(cId, dId);                            // S2 = {C,D}
            const int sGroup  = p.getBlockById(aId)->stackGroup;
            const int s2Group = p.getBlockById(cId)->stackGroup;

            p.restackBlockOnto(aId, cId);                       // drop A onto C (in S2)

            auto* a = p.getBlockById(aId); auto* b = p.getBlockById(bId);
            auto* e = p.getBlockById(eId); auto* c = p.getBlockById(cId);
            auto* d = p.getBlockById(dId);
            int s2Members = 0;
            for (auto* blk : p.blocks) if (blk->stackGroup == s2Group) ++s2Members;
            bool joined  = a->stackGroup == s2Group && c->stackGroup == s2Group
                        && d->stackGroup == s2Group && s2Members == 3;
            bool oldKept = b->stackGroup == sGroup && e->stackGroup == sGroup
                        && sGroup != s2Group;
            bool nothingLost = p.blocks.size() == 5;
            verdict("T33 stacked->other-stack drop: A joins S2={A,C,D}, S keeps {B,E}",
                    joined && oldKept && nothingLost,
                    "S2 members=" + juce::String(s2Members)
                    + ", B.sg=" + juce::String(b->stackGroup)
                    + ", E.sg=" + juce::String(e->stackGroup)
                    + ", blocks=" + juce::String(p.blocks.size()));
        }

        { // T34 (1c.3): a NON-stacked block dropped onto a stacked one JOINS that
          // stack. Since the 1c anchor fix ALL drops (standalone or stacked
          // dragged) route through restackBlockOnto; membership/no-absorption is
          // asserted here, slot anchoring in T36.
            Project p;
            auto* X = p.addBlock("X"); auto* C = p.addBlock("C"); auto* D = p.addBlock("D");
            addClipTo(X, "cX"); addClipTo(C, "cC"); addClipTo(D, "cD");
            const juce::String xId = X->id, cId = C->id, dId = D->id;
            p.stackBlocks(cId, dId);                            // S2 = {C,D}
            const int s2Group = p.getBlockById(cId)->stackGroup;

            p.restackBlockOnto(xId, cId);                       // GUI drop dispatch

            auto* x = p.getBlockById(xId); auto* c = p.getBlockById(cId);
            auto* d = p.getBlockById(dId);
            int members = 0;
            for (auto* blk : p.blocks) if (blk->stackGroup == s2Group) ++members;
            bool ok = x->stackGroup == s2Group && c->stackGroup == s2Group
                   && d->stackGroup == s2Group && members == 3 && p.blocks.size() == 3;
            verdict("T34 non-stacked->stack drop: X joins {C,D} via restackBlockOnto",
                    ok, "members=" + juce::String(members)
                    + ", X.sg=" + juce::String(x->stackGroup));
        }

        { // T35 (rapid-undo regression guard): 15 varied mutations (the manual run's
          // edit kinds: weight drags, renames, reorders, stack/unstack,
          // play-count and mode changes), then 15 undos — after EACH undo the model
          // JSON must equal the snapshot taken before the corresponding mutation
          // (per-step exactness, not just the final state), then 15 redos must land
          // back on the final state. File-backed clips as in T11/T12: undo's
          // resetAndLoad reloads audio and clamps marks (FIX M7), so the files must
          // exist on disk — the app's real condition.
            auto tmpDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("resolverdiag_t35");
            tmpDir.createDirectory();
            Project p;
            auto* A = p.addBlock("A");
            auto* B = p.addBlock("B");
            auto* C = p.addBlock("C");
            auto* D = p.addBlock("D");
            addClipWithFile(A, "t35A", 1000, tmpDir);
            addClipWithFile(B, "t35B", 1200, tmpDir);
            addClipWithFile(C, "t35C", 1500, tmpDir);
            addClipWithFile(D, "t35D", 900,  tmpDir);
            const juce::String aId = A->id, bId = B->id, cId = C->id, dId = D->id;
            p.undoManager.clearUndoHistory();   // ledger starts AFTER setup

            auto snap = [&] { return juce::JSON::toString(p.toJSON()); };
            // Inspector-style undoable mutation: pre-snapshot → mutate → applyExternalMutation.
            auto mutApply = [&](std::function<void()> m) {
                auto pre = p.toJSON(); m(); p.applyExternalMutation(pre);
            };
            juce::StringArray before;   // before[i] = state before mutation i
            auto step = [&](std::function<void()> op) { before.add(snap()); op(); };

            step([&]{ mutApply([&]{ p.getBlockById(aId)->playChance = 0.3f; }); });        // 1 weight drag
            step([&]{ mutApply([&]{ p.getBlockById(bId)->name = "Renamed B"; }); });       // 2 rename
            step([&]{ p.moveBlock(0, 2); });                                               // 3 reorder
            step([&]{ p.stackBlocks(bId, aId); });                                         // 4 stack {A,B}
            step([&]{ mutApply([&]{ auto* a = p.getBlockById(aId);                          // 5 play count 2
                                    a->stackPlayCount.values.set(0, 2);
                                    p.propagateStackSettings(a->stackGroup, a); }); });
            step([&]{ p.stackBlocks(aId, cId); });                                         // 6 stack C into A's group
            step([&]{ mutApply([&]{ p.getBlockById(cId)->playChance = 0.7f; }); });        // 7 weight drag
            step([&]{ p.moveBlock(3, 0); });                                               // 8 reorder
            step([&]{ p.stackBlocks(dId, bId); });                                         // 9 stack D with B
            step([&]{ auto pre = p.toJSON();                                                // 10 gap-drop unstack A
                      // Inline replay of the UI gap-drop branch (unstack + dissolve solo).
                      auto* a = p.getBlockById(aId);
                      int oldGroup = a->stackGroup;
                      a->stackGroup = -1;
                      int remaining = 0; Block* last = nullptr;
                      for (auto* b : p.blocks)
                          if (b->stackGroup == oldGroup) { ++remaining; last = b; }
                      if (remaining == 1 && last != nullptr) {
                          last->stackGroup = -1;
                          last->stackPlayCount.values.clearQuick();
                          last->stackPlayCount.values.add(1);
                          last->stackPlayCount.weights.clearQuick();
                          last->stackPlayCount.weights.add(1.0f);
                          last->stackPlayMode = StackPlayMode::Sequential;
                      } else if (remaining > 1) {
                          p.propagateStackSettings(oldGroup);
                      }
                      int from = p.blocks.indexOf(a);
                      if (from >= 0 && from != 0) p.blocks.move(from, 0);
                      for (int i = 0; i < p.blocks.size(); ++i) p.blocks[i]->position = i;
                      p.applyExternalMutation(pre); });
            step([&]{ mutApply([&]{ p.getBlockById(dId)->name = "Renamed D"; }); });       // 11 rename
            step([&]{ p.stackBlocks(bId, aId); });                                         // 12 stack A back with B
            step([&]{ mutApply([&]{ p.getBlockById(bId)->playChance = 0.05f; }); });       // 13 weight drag
            step([&]{ p.moveBlock(1, 3); });                                               // 14 reorder
            step([&]{ mutApply([&]{ auto* a = p.getBlockById(aId);                          // 15 mode toggle
                                    a->stackPlayMode = StackPlayMode::Simultaneous;
                                    p.propagateStackSettings(a->stackGroup, a); }); });

            const juce::String finalState = snap();
            const int n = before.size();   // 15

            // 15 rapid undos: after undo k the state must equal before[n-k] exactly.
            bool undoOk = true; int firstBadUndo = -1;
            for (int i = n - 1; i >= 0; --i) {
                bool did = p.undoManager.undo();
                if (!did || snap() != before[i]) { undoOk = false; firstBadUndo = i; break; }
            }
            // 15 redos must return to the exact final state.
            bool redoOk = true;
            if (undoOk) {
                for (int i = 0; i < n; ++i)
                    if (!p.undoManager.redo()) { redoOk = false; break; }
                redoOk = redoOk && (snap() == finalState);
            }
            verdict("T35 rapid undo x15: per-step model state exact + redo x15 restores",
                    n == 15 && undoOk && redoOk,
                    "steps=" + juce::String(n)
                    + ", perStepUndo: " + (undoOk ? juce::String("15/15 exact")
                                                  : "MISMATCH at step " + juce::String(firstBadUndo + 1))
                    + ", redoToFinal: " + (redoOk ? "exact" : "BAD"));
            tmpDir.deleteRecursively();
        }

        { // T36 (1c position): the stack formed by a drop sits at the DROP TARGET's
          // slot, not the dragged block's old slot. Slots computed exactly as
          // BlockStrip::resized(): scan blocks[] in order; the first member of each
          // stackGroup opens the slot.
            auto slotOf = [](Project& p, const juce::String& blockId) -> int {
                juce::Array<int> seenGroups, groupSlot;   // parallel arrays
                int slot = -1;
                for (auto* b : p.blocks) {
                    int sg = b->stackGroup, mySlot;
                    if (sg < 0) {
                        mySlot = ++slot;                  // standalone opens its own slot
                    } else {
                        int gi = seenGroups.indexOf(sg);
                        if (gi < 0) { mySlot = ++slot; seenGroups.add(sg); groupSlot.add(mySlot); }
                        else        { mySlot = groupSlot[gi]; }   // later member: group's slot
                    }
                    if (b->id == blockId) return mySlot;
                }
                return -1;
            };
            auto arrayOrder = [](Project& p) {
                juce::String s;
                for (auto* b : p.blocks) s << b->name;
                return s;
            };

            // GUI-DISPATCH MIRROR (test-gap fix): T36 originally hardcoded
            // restackBlockOnto, but BlockStrip::blockDropped routed STANDALONE
            // dragged blocks to stackBlocks (no re-anchor) — so the suite was green
            // while the app anchored rightward drops at the dragged block's old
            // slot. This lambda must stay byte-equivalent to the DropAction::Stack
            // dispatch in BlockStrip::blockDropped (and completePendingMode's
            // Stack branch): since the 1c anchor fix, BOTH dragged states route
            // through restackBlockOnto unconditionally.
            auto guiDrop = [](Project& p, const juce::String& draggedId, const juce::String& targetId) {
                p.restackBlockOnto(draggedId, targetId);
            };

            // Case 1: X in stack S (slot 0) dropped onto STANDALONE D (slot 2).
            Project p;
            auto* A = p.addBlock("A"); auto* B = p.addBlock("B"); auto* C = p.addBlock("C");
            auto* D = p.addBlock("D"); auto* E = p.addBlock("E");
            addClipTo(A, "cA"); addClipTo(B, "cB"); addClipTo(C, "cC");
            addClipTo(D, "cD"); addClipTo(E, "cE");
            const juce::String aId = A->id, bId = B->id, dId = D->id;
            p.stackBlocks(aId, bId);              // slots: {A,B}=0, C=1, D=2, E=3
            const int dSlotBefore = slotOf(p, dId);            // 2 = drop location j

            guiDrop(p, aId, dId);
            bool posOk1 = slotOf(p, aId) == dSlotBefore        // {D,A} sits at j
                       && slotOf(p, dId) == dSlotBefore
                       && slotOf(p, bId) == 0                  // mate stays at old slot i
                       && arrayOrder(p) == "BCDAE"             // A moved next to D, no swap to i
                       && p.blocks[0]->position == 0 && p.blocks[4]->position == 4;

            // Case 2: X in S dropped onto a block in ANOTHER stack S2 — S2 keeps its slot.
            Project q;
            auto* F = q.addBlock("F"); auto* G = q.addBlock("G"); auto* H = q.addBlock("H");
            auto* I = q.addBlock("I"); auto* J = q.addBlock("J");
            addClipTo(F, "cF"); addClipTo(G, "cG"); addClipTo(H, "cH");
            addClipTo(I, "cI"); addClipTo(J, "cJ");
            const juce::String fId = F->id, gId = G->id, iId = I->id;
            q.stackBlocks(fId, gId);              // S  = {F,G} slot 0
            q.stackBlocks(iId, J->id);            // S2 = {I,J} slot 2 (H standalone slot 1)
            const int s2SlotBefore = slotOf(q, iId);           // 2

            guiDrop(q, fId, iId);
            bool posOk2 = slotOf(q, fId) == s2SlotBefore       // F joins S2 at S2's slot
                       && slotOf(q, iId) == s2SlotBefore
                       && slotOf(q, gId) == 0                  // old mate keeps slot 0
                       && arrayOrder(q) == "GHIJF";

            // Case 3 (the in-app repro T36 used to miss): STANDALONE dragged block,
            // RIGHTWARD drop. A..E standalone; drop A(idx 0) onto D(idx 3). The
            // stack must render at D's slot — after A's own slot compresses away,
            // that is slot 2 (order BCDAE). The pre-fix dispatch (stackBlocks, no
            // re-anchor) leaves order ABCDE and anchors {A,D} at A's old slot 0.
            Project r3;
            auto* A3 = r3.addBlock("A"); auto* B3 = r3.addBlock("B"); auto* C3 = r3.addBlock("C");
            auto* D3 = r3.addBlock("D"); auto* E3 = r3.addBlock("E");
            addClipTo(A3, "cA"); addClipTo(B3, "cB"); addClipTo(C3, "cC");
            addClipTo(D3, "cD"); addClipTo(E3, "cE");
            const juce::String a3Id = A3->id, d3Id = D3->id;

            guiDrop(r3, a3Id, d3Id);
            bool posOk3 = slotOf(r3, a3Id) == 2
                       && slotOf(r3, d3Id) == 2
                       && arrayOrder(r3) == "BCDAE";

            // Case 4: STANDALONE dragged block, LEFTWARD mirror. Drop D onto A →
            // stack renders at A's slot 0. (Pre-fix this direction was only
            // accidentally correct: anchor = min(index) coincides with the target.)
            Project r4;
            auto* A4 = r4.addBlock("A"); auto* B4 = r4.addBlock("B"); auto* C4 = r4.addBlock("C");
            auto* D4 = r4.addBlock("D"); auto* E4 = r4.addBlock("E");
            addClipTo(A4, "cA"); addClipTo(B4, "cB"); addClipTo(C4, "cC");
            addClipTo(D4, "cD"); addClipTo(E4, "cE");
            const juce::String a4Id = A4->id, d4Id = D4->id;

            guiDrop(r4, d4Id, a4Id);
            bool posOk4 = slotOf(r4, d4Id) == 0
                       && slotOf(r4, a4Id) == 0;

            // Case 5: STANDALONE X dropped onto a member of an EXISTING stack to
            // its right. F..J; {I,J} stacked (slot 3). Drop F onto I → F joins
            // {I,J} which stays anchored on I/J; with F's slot 0 compressed away
            // the stack renders at slot 2 (order GHIJF), NOT at F's old slot 0.
            Project r5;
            auto* F5 = r5.addBlock("F"); auto* G5 = r5.addBlock("G"); auto* H5 = r5.addBlock("H");
            auto* I5 = r5.addBlock("I"); auto* J5 = r5.addBlock("J");
            addClipTo(F5, "cF"); addClipTo(G5, "cG"); addClipTo(H5, "cH");
            addClipTo(I5, "cI"); addClipTo(J5, "cJ");
            const juce::String f5Id = F5->id, i5Id = I5->id, j5Id = J5->id;
            r5.stackBlocks(i5Id, J5->id);          // setup primitive: {I,J} slot 3

            guiDrop(r5, f5Id, i5Id);
            bool posOk5 = slotOf(r5, f5Id) == 2
                       && slotOf(r5, i5Id) == 2
                       && slotOf(r5, j5Id) == 2
                       && arrayOrder(r5) == "GHIJF";

            verdict("T36 drop position: formed/joined stack anchors at target slot (GUI dispatch)",
                    posOk1 && posOk2 && posOk3 && posOk4 && posOk5,
                    "case1 order=" + arrayOrder(p) + " (want BCDAE), {D,A} slot="
                    + juce::String(slotOf(p, aId)) + " (want " + juce::String(dSlotBefore)
                    + "); case2 order=" + arrayOrder(q) + " (want GHIJF), S2 slot="
                    + juce::String(slotOf(q, fId)) + " (want " + juce::String(s2SlotBefore)
                    + "); case3 STANDALONE rightward order=" + arrayOrder(r3)
                    + " (want BCDAE), slot=" + juce::String(slotOf(r3, a3Id)) + " (want 2)"
                    + "; case4 STANDALONE leftward slot=" + juce::String(slotOf(r4, d4Id)) + " (want 0)"
                    + "; case5 onto-stack-right order=" + arrayOrder(r5)
                    + " (want GHIJF), slot=" + juce::String(slotOf(r5, f5Id)) + " (want 2)");
        }

        { // T37 (1d guard): one stack drop records exactly ONE undo entry and undoes
          // in ONE step. File-backed clips (as T35): undo's resetAndLoad reloads
          // audio from disk, matching the app's real condition.
            auto tmpDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("resolverdiag_t37");
            tmpDir.createDirectory();
            Project p;
            auto* A = p.addBlock("A"); auto* B = p.addBlock("B"); auto* C = p.addBlock("C");
            addClipWithFile(A, "t37A", 1000, tmpDir);
            addClipWithFile(B, "t37B", 1200, tmpDir);
            addClipWithFile(C, "t37C", 1500, tmpDir);
            const juce::String aId = A->id, cId = C->id;
            p.stackBlocks(aId, B->id);                       // S = {A,B}
            p.undoManager.clearUndoHistory();                // ledger starts AFTER setup

            auto snap = [&] { return juce::JSON::toString(p.toJSON()); };
            const juce::String preState = snap();
            p.restackBlockOnto(aId, cId);                    // the drop under test
            const juce::String postState = snap();

            bool oneUndo   = p.undoManager.undo();           // one step...
            bool restored  = snap() == preState;             // ...fully reverts the drop
            bool onlyEntry = !p.undoManager.canUndo();       // and there was ONLY one entry
            bool redoOk    = p.undoManager.redo() && snap() == postState;
            verdict("T37 stack drop undo: exactly ONE entry, one-step revert, redo exact",
                    oneUndo && restored && onlyEntry && redoOk,
                    juce::String("undo=") + (restored ? "exact" : "BAD")
                    + ", extraEntries=" + (onlyEntry ? "none" : "YES(BUG)")
                    + ", redo=" + (redoOk ? "exact" : "BAD"));
            tmpDir.deleteRecursively();
        }


        { // ═════ MEASURABLE BACKSTOPS T38–T46 (added 2026-07-13) ═════
          // Convert VALIDATION_PLAN.md judgment rows into harness assertions.
          // Verdicts print MEASURED values so each test is visibly non-vacuous.
            std::cout << "--- T38-T46 measurable backstops ---\n";

            // Fill a buffer with a DC value — rendered output then reads as DC × gain,
            // so gain envelopes can be read directly off the engine output.
            auto setDC = [](juce::AudioBuffer<float>& b, float v) {
                for (int ch = 0; ch < b.getNumChannels(); ++ch) {
                    auto* w = b.getWritePointer(ch);
                    for (int i = 0; i < b.getNumSamples(); ++i) w[i] = v;
                }
            };
            // renderViaEngine with an explicit output length (margin beyond the
            // arrangement end for silence-after assertions) — same REAL engine path.
            auto renderEngineLen = [](const ResolvedArrangement& arr, int blockSize, int64_t outLen) {
                juce::AudioBuffer<float> out(2, (int)outLen);
                out.clear();
                PlaybackEngine eng;
                eng.prepareToPlay(arr.sampleRate, blockSize);
                eng.play(arr);
                juce::AudioBuffer<float> tmp(2, blockSize);
                for (int64_t pos = 0; pos < outLen; pos += blockSize) {
                    tmp.clear();
                    eng.getNextAudioBlock(tmp, blockSize);
                    int thisLen = (int)juce::jmin((int64_t)blockSize, outLen - pos);
                    for (int ch = 0; ch < 2; ++ch)
                        out.copyFrom(ch, (int)pos, tmp, ch, 0, thisLen);
                }
                return out;
            };
            // Ramp checkers: "decreasing" = no sample-to-sample RISE beyond eps AND a
            // total drop > half the start value. A FLAT segment fails the drop clause —
            // that rejection is T38's negative control (proves the assertion bites).
            auto monoDec = [](const juce::AudioBuffer<float>& b, int from, int to) {
                const float* p = b.getReadPointer(0);
                float maxRise = 0.0f;
                for (int i = from + 1; i < to; ++i) maxRise = juce::jmax(maxRise, p[i] - p[i - 1]);
                return maxRise <= 1e-4f && (p[from] - p[to - 1]) > 0.5f * std::abs(p[from]);
            };
            auto monoInc = [](const juce::AudioBuffer<float>& b, int from, int to) {
                const float* p = b.getReadPointer(0);
                float maxFall = 0.0f;
                for (int i = from + 1; i < to; ++i) maxFall = juce::jmax(maxFall, p[i - 1] - p[i]);
                return maxFall <= 1e-4f && (p[to - 1] - p[from]) > 0.5f * std::abs(p[to - 1]);
            };

            { // T38 (3.1/3.3): crossfade gain envelope measured off the real engine.
                const double sr = 44100.0;
                const int L = 4410, body = 17640;
                Project p; p.sampleRate = sr;
                auto* A = p.addBlock("A"); auto* B = p.addBlock("B");
                addClipTo(A, "cA", L + body + L);   // lead-in + body + tail
                addClipTo(B, "cB", L + body);       // lead-in + body
                A->clips[0]->startMark = L; A->clips[0]->endMark = L + body;
                B->clips[0]->startMark = L; B->clips[0]->endMark = L + body;
                // Entries snapshot (trim-copy) the source buffers AT RESOLVE TIME,
                // so each isolation variant sets its DC values BEFORE its own
                // resolve. One clip per block -> resolution is deterministic.
                ArrangementResolver res;
                auto resolveWith = [&](float dcA, float dcB) {
                    setDC(*A->clips[0]->audioBuffer, dcA);
                    setDC(*B->clips[0]->audioBuffer, dcB);
                    juce::Random r(3800);
                    auto a = res.resolve(p, r); a.sampleRate = sr;
                    return a;
                };
                auto arr1 = resolveWith(1.0f, 0.0f);                 // A alone
                auto arr2 = resolveWith(0.0f, 1.0f);                 // B alone
                auto arr3 = resolveWith(0.4f, 0.4f);                 // both live
                bool twoEntries = arr1.entries.size() == 2;
                int64_t bodyEndA = twoEntries ? arr1.entries[0].timelinePos + body : 0;
                bool aligned = twoEntries && arr1.entries[1].timelinePos == bodyEndA;
                auto r1 = renderEngineLen(arr1, 512, arr1.totalDurationSamples);
                auto r2 = renderEngineLen(arr2, 512, arr2.totalDurationSamples);
                auto r3 = renderEngineLen(arr3, 512, arr3.totalDurationSamples);

                // (a) entry-0 lead-in FULL gain from t=0 — flat 1.0, no ramp, exact.
                float headDev = 0.0f;
                for (int i = 0; i < L; ++i)
                    headDev = juce::jmax(headDev, std::abs(r1.getSample(0, i) - 1.0f));
                // (b) recovered envelopes: A tail ramps down over [bodyEndA, +L);
                //     B lead-in ramps up over [bodyEndA - L, bodyEndA).
                int w1a = (int)bodyEndA - L, w1b = (int)bodyEndA;
                int w2a = (int)bodyEndA,     w2b = (int)bodyEndA + L;
                bool tailDown = monoDec(r1, w2a, w2b);
                bool leadUp   = monoInc(r2, w1a, w1b);
                // (c) continuity with both sources live across the whole join.
                float maxStep = 0.0f;
                for (int i = w1a - 99; i < w2b + 100 && i < (int)arr3.totalDurationSamples; ++i)
                    maxStep = juce::jmax(maxStep, std::abs(r3.getSample(0, i) - r3.getSample(0, i - 1)));
                // NEGATIVE CONTROL: checker must REJECT a flat segment (A body ≡ 1.0).
                bool flatRejected = !monoDec(r1, L + 1000, L + 3000);

                verdict("T38 crossfade envelope: head flat@1.0, tail down, lead-in up, no step; neg-control bites",
                        twoEntries && aligned && headDev < 1e-6f && tailDown && leadUp
                            && maxStep < 0.01f && flatRejected,
                        juce::String("headDev=") + juce::String(headDev, 8)
                        + ", tailEnv[" + juce::String(r1.getSample(0, w2a), 3) + "->"
                        + juce::String(r1.getSample(0, w2b - 1), 4) + "]"
                        + ", leadEnv[" + juce::String(r2.getSample(0, w1a), 4) + "->"
                        + juce::String(r2.getSample(0, w1b - 1), 3) + "]"
                        + ", maxStep=" + juce::String(maxStep, 6)
                        + ", flatRejected=" + (flatRejected ? "y" : "NO(test broken)"));
            }

            { // T38b (invariant sweep): sequential timeline WITH tails — dump +
              // numeric adjacency. Tails overlap the NEXT body region by design but
              // must never move bodies apart nor overlap them.
                const double sr = 44100.0;
                Project p; p.sampleRate = sr;
                const int L = 2205, body = 8820, tail = 3308;
                juce::Array<int64_t> bodyLens;
                for (auto n : { "A", "B", "C" }) {
                    auto* blk = p.addBlock(n);
                    addClipTo(blk, juce::String("c") + n, L + body + tail);
                    blk->clips[0]->startMark = L;
                    blk->clips[0]->endMark   = L + body;
                }
                juce::Random r(3850); ArrangementResolver res;
                auto arr = res.resolve(p, r); arr.sampleRate = sr;
                bool adjacency = arr.entries.size() == 3;
                std::cout << "T38b resolved timeline dump (3 entries, L=" << L
                          << " body=" << body << " tail=" << tail << "):\n";
                for (int i = 0; i < arr.entries.size(); ++i) {
                    const auto& e = arr.entries.getReference(i);
                    int64_t bLen = e.endMark - e.startMark;
                    int64_t tLen = (int64_t)e.audioBuffer->getNumSamples() - e.endMark;
                    std::cout << "  entry[" << i << "] " << e.clipName
                              << ": timelinePos=" << e.timelinePos
                              << " leadInLen=" << e.startMark
                              << " bodyLen=" << bLen
                              << " tailLen=" << tLen << "\n";
                    if (i > 0) {
                        const auto& prev = arr.entries.getReference(i - 1);
                        int64_t prevBodyLen = prev.endMark - prev.startMark;
                        adjacency = adjacency
                            && e.timelinePos == prev.timelinePos + prevBodyLen;
                    }
                }
                int64_t lastBodyEnd = arr.entries.getLast().timelinePos
                                    + (arr.entries.getLast().endMark - arr.entries.getLast().startMark);
                bool totalOk = arr.totalDurationSamples == lastBodyEnd + tail;
                verdict("T38b timeline with tails: bodies zero-gap zero-overlap, total = lastBodyEnd + tail",
                        adjacency && totalOk,
                        "adjacency=" + juce::String(adjacency ? "exact" : "BROKEN")
                        + ", total=" + juce::String(arr.totalDurationSamples)
                        + " (want " + juce::String(lastBodyEnd + tail) + ")");
            }

            { // T39 (3.4): pitch through the WSOLA-stretched LEAD-IN — 440 Hz stays
              // 440 Hz. (The lead-in is the stretch path that actually renders; the
              // TAIL path is covered — and currently FAILED — by T38/T40/T41: the
              // resolver's trimBuffer(0, endMark) discards tails entirely.)
                const double sr = 44100.0;
                Project p; p.sampleRate = sr;
                auto* intro = p.addBlock("Intro"); auto* verse = p.addBlock("Verse");
                addClipTo(intro, "intro", (int)(0.8 * sr));          // SILENT: isolates the lead-in
                intro->clips[0]->endMark = (int64_t)(0.8 * sr);
                intro->clips[0]->tempo   = 120.0;
                auto verseBuf = makeTone((int)(1.2 * sr), 440.0, sr);
                addClipTo(verse, "verse", verseBuf->getNumSamples());
                verse->clips[0]->audioBuffer = verseBuf;
                verse->clips[0]->startMark = (int64_t)(0.4 * sr);    // 0.4 s lead-in
                verse->clips[0]->endMark   = (int64_t)(1.2 * sr);
                verse->clips[0]->tempo     = 160.0;
                juce::Random r(3900); ArrangementResolver res;
                auto arr = res.resolve(p, r); arr.sampleRate = sr;
                float ratio = arr.entries[1].leadInStretchRatio;      // 160/120 = 1.333
                int stLen = arr.entries[1].stretchedLeadIn
                                ? arr.entries[1].stretchedLeadIn->getNumSamples()
                                : (int)(0.4 * sr * ratio + 0.5);
                int64_t bodyStartB = arr.entries[1].timelinePos;
                int64_t liStart = bodyStartB - stLen;
                auto out = renderEngineLen(arr, 512, arr.totalDurationSamples);
                auto peakOf = [&](int from, int len) {
                    double best = 0.0, bf = 0.0;
                    for (double f = 380.0; f <= 500.0; f += 0.25) {
                        double w = 2.0 * juce::MathConstants<double>::pi * f / sr, cw = 2.0 * std::cos(w);
                        double s1 = 0.0, s2 = 0.0;
                        const float* q = out.getReadPointer(0);
                        for (int i = 0; i < len; ++i) { double s0 = q[from + i] + cw * s1 - s2; s2 = s1; s1 = s0; }
                        double mag = std::sqrt(juce::jmax(0.0, s1 * s1 + s2 * s2 - cw * s1 * s2));
                        if (mag > best) { best = mag; bf = f; }
                    }
                    return bf;
                };
                int skip = stLen / 10;
                double fLead = peakOf((int)liStart + skip, stLen - 2 * skip);
                double fBody = peakOf((int)bodyStartB + 2000, 30000);  // analyzer sanity, unstretched
                double centsLead = 1200.0 * std::log2(fLead / 440.0);
                double centsBody = 1200.0 * std::log2(fBody / 440.0);
                verdict("T39 pitch through stretched lead-in: 440 Hz within 10 cents",
                        std::abs(ratio - 4.0f / 3.0f) < 0.01f
                            && arr.entries[1].stretchedLeadIn != nullptr
                            && std::abs(centsLead) <= 10.0 && std::abs(centsBody) <= 5.0,
                        "ratio=" + juce::String(ratio, 3) + " (want 1.333), stretchedBuf="
                        + (arr.entries[1].stretchedLeadIn ? "y" : "NULL")
                        + ", leadPeak=" + juce::String(fLead, 2) + "Hz (" + juce::String(centsLead, 1) + " cents)"
                        + ", bodyPeak=" + juce::String(fBody, 2) + "Hz (" + juce::String(centsBody, 1) + " cents)");
            }

            { // T40 (3.5): retain-tail flag — rendered tail length, measured in samples.
                const double sr = 44100.0;
                int lens[2] = { -1, -1 }; int64_t totals[2] = { 0, 0 };
                for (int pass = 0; pass < 2; ++pass) {
                    const bool retain = (pass == 1);
                    Project p; p.sampleRate = sr;
                    auto* intro = p.addBlock("Intro"); auto* verse = p.addBlock("Verse");
                    auto introBuf = makeTone((int)(1.2 * sr), 440.0, sr);
                    addClipTo(intro, "intro", introBuf->getNumSamples());
                    intro->clips[0]->audioBuffer = introBuf;
                    intro->clips[0]->endMark = (int64_t)(0.8 * sr);
                    intro->clips[0]->tempo   = 120.0;
                    intro->clips[0]->retainTailTempo = retain;
                    addClipTo(verse, "verse", (int)(1.2 * sr));      // silent
                    verse->clips[0]->startMark = (int64_t)(0.4 * sr);
                    verse->clips[0]->endMark   = (int64_t)(1.2 * sr);
                    verse->clips[0]->tempo     = 160.0;
                    juce::Random r(4000 + pass); ArrangementResolver res;
                    auto arr = res.resolve(p, r); arr.sampleRate = sr;
                    int64_t bodyEnd = arr.entries[0].timelinePos + intro->clips[0]->endMark;
                    int64_t outLen  = arr.totalDurationSamples + 2205;
                    auto out = renderEngineLen(arr, 512, outLen);
                    int lastLoud = -1;
                    for (int i = (int)bodyEnd; i < (int)outLen; ++i)
                        if (std::abs(out.getSample(0, i)) > 1e-4f) lastLoud = i;
                    lens[pass] = lastLoud - (int)bodyEnd + 1;
                    totals[pass] = arr.totalDurationSamples;
                }
                const int expOff = (int)(0.4 * sr * 0.75 + 0.5);     // 13230 (ratio 120/160)
                const int expOn  = (int)(0.4 * sr);                  // 17640 (original speed)
                verdict("T40 retain-tail: OFF -> stretched length, ON -> original length",
                        std::abs(lens[0] - expOff) <= 64 && std::abs(lens[1] - expOn) <= 64,
                        "tailLen OFF=" + juce::String(lens[0]) + " (want ~" + juce::String(expOff) + ")"
                        + ", ON=" + juce::String(lens[1]) + " (want ~" + juce::String(expOn) + ")"
                        + ", totals=" + juce::String(totals[0]) + "/" + juce::String(totals[1]));
            }

            { // T41 (8.1): song ender truncates after its tail; beyond it EXACT silence.
                const double sr = 44100.0;
                Project p; p.sampleRate = sr;
                auto* A = p.addBlock("A"); auto* B = p.addBlock("B"); auto* C = p.addBlock("C");
                addClipTo(A, "cA", 8820);  A->clips[0]->audioBuffer = makeTone(8820, 330.0, sr);
                addClipTo(B, "cB", 13230); B->clips[0]->audioBuffer = makeTone(13230, 550.0, sr);
                B->clips[0]->endMark = 8820;                          // 4410-sample tail
                B->clips[0]->isSongEnder = true;
                addClipTo(C, "cC", 8820);  C->clips[0]->audioBuffer = makeTone(8820, 700.0, sr);
                juce::Random r(4100); ArrangementResolver res;
                auto arr = res.resolve(p, r); arr.sampleRate = sr;
                bool truncated = arr.entries.size() == 2;
                int64_t expTotal = truncated ? arr.entries[1].timelinePos + 8820 + 4410 : -1;
                auto out = renderEngineLen(arr, 512, expTotal + 4410);
                float tailPeak = 0.0f, beyondPeak = 0.0f;
                for (int i = (int)expTotal - 4410; i < (int)expTotal; ++i)
                    tailPeak = juce::jmax(tailPeak, std::abs(out.getSample(0, i)));
                for (int i = (int)expTotal; i < (int)expTotal + 4410; ++i)
                    beyondPeak = juce::jmax(beyondPeak, std::abs(out.getSample(0, i)));
                verdict("T41 song ender: entries truncated, total==enderEnd+tail, beyond EXACTLY silent",
                        truncated && arr.totalDurationSamples == expTotal
                            && tailPeak > 0.05f && beyondPeak == 0.0f,
                        "entries=" + juce::String(arr.entries.size()) + " (want 2)"
                        + ", total=" + juce::String(arr.totalDurationSamples)
                        + " (want " + juce::String(expTotal) + ")"
                        + ", tailPeak=" + juce::String(tailPeak, 3)
                        + ", beyondPeak=" + juce::String(beyondPeak, 8));
            }

            { // T42 (9.1): tempo changes touch the GRID only — audio buffer untouched.
                Project p;
                auto* A = p.addBlock("A");
                addClipTo(A, "cA", 1000);
                A->clips[0]->audioBuffer = makeTone(44100, 440.0, 44100.0);
                auto* before = A->clips[0]->audioBuffer.get();
                auto hash = [](const juce::AudioBuffer<float>& b) {
                    double acc = 0.0;
                    for (int ch = 0; ch < b.getNumChannels(); ++ch)
                        for (int i = 0; i < b.getNumSamples(); ++i)
                            acc += std::abs(b.getSample(ch, i)) * (1.0 + (i % 7));
                    return acc;
                };
                const double h0 = hash(*A->clips[0]->audioBuffer);
                A->tempo = 93.0;                     // block tempo
                A->clips[0]->tempo = 171.0;          // per-clip override
                p.defaultClipTempo = 80.0;           // project default
                const double h1 = hash(*A->clips[0]->audioBuffer);
                bool samePtr = A->clips[0]->audioBuffer.get() == before;
                verdict("T42 tempo-only-grid: buffer pointer + contents identical after 3 tempo changes",
                        samePtr && h0 == h1,
                        juce::String("ptrSame=") + (samePtr ? "y" : "NO")
                        + ", hashBefore=" + juce::String(h0, 2)
                        + ", hashAfter=" + juce::String(h1, 2));
            }

            { // T43 (1.5): gap-drop (Reorder) positioning — mirror of the OTHER
              // blockDropped branch (T36 pinned only DropAction::Stack).
              // GUI-DISPATCH MIRROR: must stay byte-equivalent to the plain-reorder
              // else-branch of BlockStrip::blockDropped (insertBefore/dest math).
                auto gapDrop = [](Project& p, int fromIndex, int dropTargetIndex) {
                    int insertBefore = juce::jlimit(0, p.blocks.size(), dropTargetIndex);
                    int dest;
                    if (insertBefore >= p.blocks.size())
                        dest = p.blocks.size() - 1;
                    else if (fromIndex < insertBefore)
                        dest = insertBefore - 1;
                    else
                        dest = insertBefore;
                    dest = juce::jlimit(0, p.blocks.size() - 1, dest);
                    if (fromIndex != dest) p.moveBlock(fromIndex, dest);
                };
                auto build = [](Project& p) {
                    for (auto n : { "A", "B", "C", "D", "E" })
                        addClipTo(p.addBlock(n), juce::String("c") + n);
                };
                auto order = [](Project& p) {
                    juce::String s;
                    for (auto* b : p.blocks) s << b->name;
                    return s;
                };
                Project p1; build(p1); gapDrop(p1, 0, 3);            // A right: gap before D
                Project p2; build(p2); gapDrop(p2, 0, 5);            // A to far-right end
                Project p3; build(p3); gapDrop(p3, 4, 1);            // E left: gap before B
                bool posOk = true;
                for (int i = 0; i < p1.blocks.size(); ++i)
                    posOk = posOk && p1.blocks[i]->position == i;
                // Stacked dragged block into a gap: MIRROR of the unstack branch
                // (detachBlockFromStack + move + renumber + one applyExternalMutation).
                Project p4; build(p4);
                p4.stackBlocks(p4.blocks[0]->id, p4.blocks[1]->id);  // {A,B}
                {
                    auto* dragged = p4.blocks[0];
                    int fromIndex = 0, dropTargetIndex = 4;
                    auto pre = p4.toJSON();
                    p4.detachBlockFromStack(*dragged);
                    int insertBefore = juce::jlimit(0, p4.blocks.size(), dropTargetIndex);
                    int dest;
                    if (insertBefore >= p4.blocks.size())      dest = p4.blocks.size() - 1;
                    else if (fromIndex < insertBefore)         dest = insertBefore - 1;
                    else                                       dest = insertBefore;
                    dest = juce::jlimit(0, p4.blocks.size() - 1, dest);
                    if (fromIndex != dest) p4.blocks.move(fromIndex, dest);
                    for (int i = 0; i < p4.blocks.size(); ++i) p4.blocks[i]->position = i;
                    p4.applyExternalMutation(pre);
                }
                bool unstacked = p4.getBlockById(p4.blocks[2]->id) != nullptr;   // placeholder true
                bool sgOk = true;
                for (auto* b : p4.blocks) sgOk = sgOk && b->stackGroup == -1;    // mate auto-unstacked too
                verdict("T43 gap-drop reorder: right/end/left + stacked-unstack land at the gap",
                        order(p1) == "BCADE" && order(p2) == "BCDEA" && order(p3) == "AEBCD"
                            && posOk && order(p4) == "BCDAE" && sgOk && unstacked,
                        "right=" + order(p1) + " (want BCADE), end=" + order(p2)
                        + " (want BCDEA), left=" + order(p3) + " (want AEBCD)"
                        + ", unstackGap=" + order(p4) + " (want BCDAE), allSg=-1: "
                        + (sgOk ? "y" : "NO"));
            }

            { // T44 (2.8): grid snap lands ON the grid; bypass keeps a non-multiple.
                const double sr = 44100.0;
                const double u120 = gridUnitSamples(120.0, sr, 1);            // 22050 exact
                const int64_t s1  = snapToGrid(30000, 120.0, sr, 1);
                const double rem1 = std::fmod((double)s1, u120);
                const double u170 = gridUnitSamples(170.0, sr, 1);            // 15564.71…
                const int64_t s2  = snapToGrid(20000, 170.0, sr, 1);
                const double dist2 = std::abs((double)s2 - std::round((double)s2 / u170) * u170);
                // Shift bypass = snap simply not applied; raw value stays off-grid.
                const int64_t raw = 20001;
                const double distRaw = std::abs((double)raw - std::round((double)raw / u170) * u170);
                verdict("T44 grid snap: snapped on-grid (<=1 sample), bypassed value stays off-grid",
                        rem1 == 0.0 && s1 == 22050 && dist2 <= 1.0 && distRaw > 1.0,
                        "snap(30000@120)=" + juce::String(s1) + " rem=" + juce::String(rem1, 3)
                        + "; snap(20000@170)=" + juce::String(s2) + " dist=" + juce::String(dist2, 3)
                        + "; bypass raw=" + juce::String(raw) + " dist=" + juce::String(distRaw, 1));
            }

            { // T45 (13.1): identity-colour compositing keeps hue dominant + saturated.
              // Constants MUST MATCH BlockComponent.cpp (neutralBodyBase, 0.85).
                struct Hue { const char* n; juce::uint32 v; };
                const Hue hues[8] = {
                    { "Red",    LookAndFeel_BlockShuffler::paletteRed },
                    { "Orange", LookAndFeel_BlockShuffler::paletteOrange },
                    { "Yellow", LookAndFeel_BlockShuffler::paletteYellow },
                    { "Green",  LookAndFeel_BlockShuffler::paletteGreen },
                    { "Cyan",   LookAndFeel_BlockShuffler::paletteCyan },
                    { "Blue",   LookAndFeel_BlockShuffler::paletteBlue },
                    { "Purple", LookAndFeel_BlockShuffler::palettePurple },
                    { "Pink",   LookAndFeel_BlockShuffler::palettePink },
                };
                const juce::Colour base(0xFF3A3A3A);
                auto dom = [](juce::Colour c) {
                    int r = c.getRed(), g = c.getGreen(), b = c.getBlue();
                    return (r >= g && r >= b) ? 0 : (g >= b ? 1 : 2);
                };
                auto hueDeltaDeg = [](juce::Colour a, juce::Colour b) {
                    float d = std::abs(a.getHue() - b.getHue());
                    return juce::jmin(d, 1.0f - d) * 360.0f;
                };
                bool allOk = true; int oldFails = 0; bool oldYellowFails = false;
                juce::String detail;
                for (auto& h : hues) {
                    juce::Colour pal(h.v);
                    juce::Colour comp = base.interpolatedWith(pal, 0.85f);
                    bool domOk = dom(pal) == dom(comp);
                    bool satOk = comp.getSaturation() >= 0.5f * pal.getSaturation();
                    bool hueOk = hueDeltaDeg(pal, comp) <= 10.0f;
                    // NEGATIVE CONTROL: the pre-13.1 formula (10% colour over the
                    // panel base) must fail sat-retention OR shift hue >10 deg —
                    // "yellow reads as green" was a HUE defect, so Yellow must fail.
                    juce::Colour old = juce::Colour(LookAndFeel_BlockShuffler::bgMedium)
                                           .overlaidWith(pal.withAlpha(0.10f));
                    bool oldBad = old.getSaturation() < 0.5f * pal.getSaturation()
                               || hueDeltaDeg(pal, old) > 10.0f;
                    if (oldBad) ++oldFails;
                    if (oldBad && juce::String(h.n) == "Yellow") oldYellowFails = true;
                    allOk = allOk && domOk && satOk && hueOk;
                    detail << h.n << " s" << juce::String(pal.getSaturation(), 2)
                           << "->" << juce::String(comp.getSaturation(), 2)
                           << " h" << juce::String(hueDeltaDeg(pal, comp), 1)
                           << (domOk && satOk && hueOk ? " " : " BAD ");
                }
                verdict("T45 colour: dominant+sat+hue kept 8/8; old formula rejected (incl. Yellow)",
                        allOk && oldYellowFails && oldFails >= 4,
                        detail + "| oldFormulaFails=" + juce::String(oldFails)
                        + "/8 (want >=4 incl. Yellow: " + (oldYellowFails ? "y)" : "NO)"));
            }

            { // T46 (4.2): link-label collision avoidance — GROUND TRUTH from the real
              // BlockLinkOverlay::paint into an offscreen image. Pills are detected as
              // connected components of the pill fill colour; each label box is the
              // pill's bbox plus the fixed name row above it (nameRowH+rowGap = 16).
                Project p;
                auto* A = p.addBlock("A"); auto* B = p.addBlock("B");
                auto* C = p.addBlock("C"); auto* D = p.addBlock("D");
                for (auto* blk : { A, B, C, D }) addClipTo(blk, "c" + blk->name);
                p.addLink(A->id, B->id, 1.0f); p.addLink(C->id, D->id, 1.0f);
                p.addLink(A->id, C->id, 1.0f); p.addLink(B->id, D->id, 1.0f);
                BlockLinkOverlay ov;
                ov.setProject(&p);
                ov.setBounds(0, 0, 600, 600);
                juce::HashMap<juce::String, int> pos;   // clustered centres force collisions
                pos.set(A->id, 280); pos.set(B->id, 300); pos.set(C->id, 320); pos.set(D->id, 340);
                ov.setBlockPositions(pos);
                juce::Image img(juce::Image::RGB, 600, 600, true);
                { juce::Graphics g(img); g.fillAll(juce::Colours::black); ov.paint(g); }
                // Pill fill = bgLight @ alpha 0.88 over black.
                const juce::Colour pl(LookAndFeel_BlockShuffler::bgLight);
                const int er = (int)std::round(0.88 * pl.getRed());
                const int eg = (int)std::round(0.88 * pl.getGreen());
                const int eb = (int)std::round(0.88 * pl.getBlue());
                auto isPill = [&](int x, int y) {
                    auto c = img.getPixelAt(x, y);
                    return std::abs((int)c.getRed() - er) <= 5
                        && std::abs((int)c.getGreen() - eg) <= 5
                        && std::abs((int)c.getBlue() - eb) <= 5;
                };
                std::vector<int> comp(600 * 600, -1);
                juce::Array<juce::Rectangle<int>> boxes;
                for (int y = 0; y < 600; ++y) for (int x = 0; x < 600; ++x) {
                    if (comp[y * 600 + x] != -1 || !isPill(x, y)) continue;
                    int id = boxes.size(); int minX = x, maxX = x, minY = y, maxY = y, count = 0;
                    std::vector<std::pair<int,int>> stack{{x, y}};
                    comp[y * 600 + x] = id;
                    while (!stack.empty()) {
                        auto [cx, cy] = stack.back(); stack.pop_back(); ++count;
                        minX = juce::jmin(minX, cx); maxX = juce::jmax(maxX, cx);
                        minY = juce::jmin(minY, cy); maxY = juce::jmax(maxY, cy);
                        const int dx[4] = {1,-1,0,0}, dy[4] = {0,0,1,-1};
                        for (int d = 0; d < 4; ++d) {
                            int nx = cx + dx[d], ny = cy + dy[d];
                            if (nx < 0 || ny < 0 || nx >= 600 || ny >= 600) continue;
                            if (comp[ny * 600 + nx] != -1 || !isPill(nx, ny)) continue;
                            comp[ny * 600 + nx] = id;
                            stack.push_back({nx, ny});
                        }
                    }
                    if (count >= 30)
                        boxes.add(juce::Rectangle<int>(minX, minY, maxX - minX + 1, maxY - minY + 1));
                    else
                        boxes.add(juce::Rectangle<int>());   // too small: ignore but keep ids stable
                }
                juce::Array<juce::Rectangle<int>> pills;
                for (auto& b : boxes) if (!b.isEmpty()) pills.add(b);
                // Later links' arcs are drawn OVER earlier pills and can split one
                // pill into fragments — merge components within 6 px of each other.
                // (Genuinely separate label rows are ~40 px apart, so no false merge;
                // truly overlapping labels would merge to <4 and fail the count.)
                for (bool changed = true; changed;) {
                    changed = false;
                    for (int i = 0; i < pills.size() && !changed; ++i)
                        for (int j = i + 1; j < pills.size() && !changed; ++j)
                            if (pills[i].expanded(6).intersects(pills[j].expanded(6))) {
                                pills.set(i, pills[i].getUnion(pills[j]));
                                pills.remove(j);
                                changed = true;
                            }
                }
                // Reconstruct full label boxes: name row (16 px) sits directly above
                // the pill; width = max(nameW, pillW) centred on the pill.
                float labelWMax = 0.0f;
                for (auto* link : p.links) {
                    auto* ba = p.getBlockById(link->blockA); auto* bb = p.getBlockById(link->blockB);
                    labelWMax = juce::jmax(labelWMax,
                        LookAndFeel_BlockShuffler::measureTextWidth(
                            LookAndFeel_BlockShuffler::uiFont(10.0f),
                            ba->name + " <-> " + bb->name) + 10.0f);
                }
                juce::Array<juce::Rectangle<float>> labels;
                for (auto& pb : pills) {
                    float w = juce::jmax((float)pb.getWidth(), labelWMax);
                    labels.add(juce::Rectangle<float>((float)pb.getCentreX() - w * 0.5f,
                                                      (float)pb.getY() - 16.0f,
                                                      w, 16.0f + (float)pb.getHeight()));
                }
                bool disjoint = true; juce::String boxTxt;
                for (int i = 0; i < labels.size(); ++i) {
                    for (int j = i + 1; j < labels.size(); ++j)
                        disjoint = disjoint && !labels[i].intersects(labels[j]);
                    boxTxt << "(" << juce::String((int)labels[i].getX()) << ","
                           << juce::String((int)labels[i].getY()) << " "
                           << juce::String((int)labels[i].getWidth()) << "x"
                           << juce::String((int)labels[i].getHeight()) << ") ";
                }
                verdict("T46 link labels: 4 links, clustered midpoints -> 4 pills found, label boxes pairwise disjoint",
                        pills.size() == 4 && disjoint,
                        "pills=" + juce::String(pills.size()) + " (want 4), boxes: " + boxTxt
                        + (disjoint ? "DISJOINT" : "OVERLAP(BUG)"));
            }
        }

        { // T48 (BUG A): deleting a block prunes every link referencing it, as
          // ONE undo entry — undo restores the block AND the link with its %.
            Project p;
            auto* A = p.addBlock("A"); p.addBlock("B"); auto* C = p.addBlock("C");
            const juce::String aId = A->id, cId = C->id;
            p.addLink(aId, cId, 0.37f);
            const auto preDelete = juce::JSON::toString(p.toJSON());

            p.removeBlock(cId);
            bool pruned = true;
            for (auto* l : p.links)
                if (l->blockA == cId || l->blockB == cId) pruned = false;

            p.undoManager.undo();  // single step must bring back block C + A<->C link
            const bool restored = (juce::JSON::toString(p.toJSON()) == preDelete);
            const bool linkBack = p.links.size() == 1
                && ((p.links[0]->blockA == aId && p.links[0]->blockB == cId) ||
                    (p.links[0]->blockA == cId && p.links[0]->blockB == aId))
                && std::abs(p.links[0]->swapProbability - 0.37f) < 1e-6f;

            verdict("T48 delete-block prunes its links; ONE undo restores block + link + %",
                    pruned && restored && linkBack,
                    juce::String("danglingAfterDelete: ") + (pruned ? "none" : "PRESENT(BUG)")
                    + ", undoRestores: " + (restored ? "exact" : "MISMATCH")
                    + ", linkBack: " + (linkBack ? "ok(0.37)" : "BAD"));
        }

        { // T49 (5.12 BUG B): headless inspector backstop — a Simultaneous stack
          // of 16 blocks must give EVERY per-block chance slider non-zero bounds.
          // The panel starts at the fixed 844px MainComponent used at the default
          // 1200x700 window; content past that height must grow the panel (the
          // viewport scrolls), not get zero-height rects from an exhausted area.
            Project p;
            Block* first = nullptr;
            for (int i = 0; i < 16; ++i) {
                auto* b = p.addBlock("S" + juce::String(i + 1));
                b->stackGroup     = 0;
                b->stackPlayMode  = StackPlayMode::Simultaneous;
                b->stackPlayCount.values  = { 1, 2 };   // two How-Many rows
                b->stackPlayCount.weights = { 0.5f, 0.5f };
                if (first == nullptr) first = b;
            }

            InspectorPanel panel;
            panel.setProject(&p);
            panel.setBounds(0, 0, 210, 844);
            panel.setBlock(first);

            int visSliders = 0, zeroH = 0;
            for (int i = 0; i < panel.getNumChildComponents(); ++i) {
                if (auto* s = dynamic_cast<juce::Slider*>(panel.getChildComponent(i))) {
                    if (!s->isVisible()) continue;
                    ++visSliders;
                    if (s->getHeight() <= 0) ++zeroH;
                }
            }
            verdict("T49 5.12 inspector: 16-block SIM stack -> all chance sliders have non-zero bounds",
                    visSliders >= 16 && zeroH == 0,
                    "visibleSliders=" + juce::String(visSliders) + " (want >=16), zeroHeight="
                    + juce::String(zeroH) + (zeroH > 0 ? " (BUG: clipped by fixed panel height)" : "")
                    + ", panelH=" + juce::String(panel.getHeight()));
        }

        { // T50: new blocks adopt the PROJECT DEFAULT tempo on creation
          // (Project.cpp addBlock), using the same >0-else-120 fallback 9.4 uses
          // (MainComponent.cpp:202). New clips then inherit it through the
          // block-tempo-priority expression both clip-add paths use
          // (MainComponent.cpp:200-202, ClipWaveformView.cpp:594-596) — mirrored
          // here verbatim. Explicitly-set tempos and serialized tempos unchanged.
            Project p;
            p.defaultClipTempo = 137.5;
            auto* nb = p.addBlock("NewB");
            bool blockAdopts = nb && std::abs(nb->tempo - 137.5) < 1e-9;

            // clip inheritance expression as used by both clip-add paths
            double blockT = nb ? nb->tempo : 0.0;
            double clipTempo = (blockT > 0.0) ? blockT
                             : (p.defaultClipTempo > 0.0 ? p.defaultClipTempo : 120.0);
            bool clipInherits = std::abs(clipTempo - 137.5) < 1e-9;

            // explicitly-set tempo (via the setter, which flags the override)
            // survives later block creation; new blocks keep flag=false
            p.setBlockTempo(*nb, 90.0);
            auto* nb2 = p.addBlock("NewB2");
            bool explicitKept = std::abs(nb->tempo - 90.0) < 1e-9
                                && nb->tempoOverridden
                                && nb2 && std::abs(nb2->tempo - 137.5) < 1e-9
                                && !nb2->tempoOverridden;

            // save/load round-trip preserves both tempos AND both flags
            // (load path must NOT re-default; flags present -> no inference)
            auto snap = p.toJSON();
            Project q;
            q.defaultClipTempo = 99.0;   // different default; must not leak into loaded blocks
            bool loaded = q.fromJSON(snap);
            bool roundTrip = loaded && q.blocks.size() == 2
                             && std::abs(q.blocks[0]->tempo - 90.0)  < 1e-9
                             && q.blocks[0]->tempoOverridden
                             && std::abs(q.blocks[1]->tempo - 137.5) < 1e-9
                             && !q.blocks[1]->tempoOverridden;

            // zero/unset default falls back to 120 (same guard as 9.4)
            Project z;
            z.defaultClipTempo = 0.0;
            auto* zb = z.addBlock("Z");
            bool zeroFallback = zb && std::abs(zb->tempo - 120.0) < 1e-9;

            verdict("T50 new block adopts project default tempo; clips inherit; explicit/loaded kept",
                    blockAdopts && clipInherits && explicitKept && roundTrip && zeroFallback,
                    juce::String("blockTempo=") + juce::String(nb ? nb->tempo : -1.0, 1)
                    + " (adopt " + (blockAdopts ? "ok" : "BAD")
                    + "), clipInherit=" + (clipInherits ? "ok" : "BAD")
                    + ", explicitKept=" + (explicitKept ? "ok" : "BAD")
                    + ", loadRoundTrip=" + (roundTrip ? "ok" : "BAD")
                    + ", zeroDefault->120=" + (zeroFallback ? "ok" : "BAD"));
        }

        { // T52: tempo inherit/override write-paths (design A+B). Tempos stay
          // MATERIALIZED — read paths untouched; only the Project setters, the
          // serialization flags (+ legacy inference) and the clip-drag retarget
          // gate on tempoOverridden. Sub-tests (a)-(f).
            auto near = [](double a, double b) { return std::abs(a - b) < 1e-9; };

            // (a) setBlockTempo: overridden clip kept, inheriting clip updated
            Project p;
            p.defaultClipTempo = 120.0;
            auto* A = p.addBlock("A");
            addClipTo(A, "a1"); addClipTo(A, "a2");
            p.setClipTempo(*A->clips[0], 150.0);            // override a1
            p.setBlockTempo(*A, 100.0);
            bool aOk = near(A->clips[0]->tempo, 150.0) &&  A->clips[0]->tempoOverridden
                    && near(A->clips[1]->tempo, 100.0) && !A->clips[1]->tempoOverridden
                    && near(A->tempo, 100.0)           &&  A->tempoOverridden;

            // (b) setDefaultTempo: overridden block (incl. its inheriting clips)
            //     skipped entirely; inheriting block + its inheriting clips
            //     updated; overridden clip inside the inheriting block kept
            auto* B = p.addBlock("B");                       // inherits 120
            addClipTo(B, "b1"); addClipTo(B, "b2");          // clips default 120
            p.setClipTempo(*B->clips[1], 77.0);              // override b2
            p.setDefaultTempo(140.0);
            bool bOk = near(A->tempo, 100.0)                 // overridden block skipped
                    && near(A->clips[1]->tempo, 100.0)       // its inheriting clip shielded
                    && near(B->tempo, 140.0)           && !B->tempoOverridden
                    && near(B->clips[0]->tempo, 140.0) && !B->clips[0]->tempoOverridden
                    && near(B->clips[1]->tempo, 77.0)  &&  B->clips[1]->tempoOverridden
                    && near(p.defaultClipTempo, 140.0);

            // (c) flags survive save/load AND an undo snapshot (resetAndLoad path)
            auto snap = p.toJSON();
            Project q; bool loaded = q.fromJSON(snap);
            bool cLoadOk = loaded && q.blocks.size() == 2
                &&  q.blocks[0]->tempoOverridden
                &&  q.blocks[0]->clips[0]->tempoOverridden
                && !q.blocks[0]->clips[1]->tempoOverridden
                && !q.blocks[1]->tempoOverridden
                &&  q.blocks[1]->clips[1]->tempoOverridden;
            p.setBlockTempo(*B, 99.0);                       // then revert it
            p.undoManager.undo();                            // resetAndLoad(pre)
            // undo rebuilds all model objects — re-fetch, old pointers dangle
            auto* A2 = p.blocks[0]; auto* B2 = p.blocks[1];
            bool cUndoOk =  A2->tempoOverridden && near(A2->tempo, 100.0)
                &&  A2->clips[0]->tempoOverridden && near(A2->clips[0]->tempo, 150.0)
                && !A2->clips[1]->tempoOverridden
                && !B2->tempoOverridden && near(B2->tempo, 140.0)
                &&  B2->clips[1]->tempoOverridden && near(B2->clips[1]->tempo, 77.0);

            // (d) legacy JSON (flags stripped) -> inference:
            //     block flag = |block - default| > eps, clip flag = |clip - block| > eps
            Project l;
            l.defaultClipTempo = 120.0;
            auto* L = l.addBlock("L");
            addClipTo(L, "l1"); addClipTo(L, "l2");
            L->tempo = 100.0;                                // direct writes = legacy divergence
            L->clips[0]->tempo = 100.0;                      // matches block -> inheriting
            L->clips[1]->tempo = 91.0;                       // diverges -> overridden
            auto* M = l.addBlock("M");                       // stays at default 120
            addClipTo(M, "m1");                              // clip default 120 == block
            auto legacy = l.toJSON();
            if (auto* blocksArr = legacy.getProperty("blocks", juce::var()).getArray())
                for (auto& bv : *blocksArr) {
                    bv.getDynamicObject()->removeProperty("tempoOverridden");
                    if (auto* clipsArr = bv.getProperty("clips", juce::var()).getArray())
                        for (auto& cv : *clipsArr)
                            cv.getDynamicObject()->removeProperty("tempoOverridden");
                }
            Project l2; bool lLoaded = l2.fromJSON(legacy);
            bool dOk = lLoaded && l2.blocks.size() == 2
                &&  l2.blocks[0]->tempoOverridden            // 100 vs default 120
                && !l2.blocks[0]->clips[0]->tempoOverridden  // 100 == block 100
                &&  l2.blocks[0]->clips[1]->tempoOverridden  //  91 != block 100
                && !l2.blocks[1]->tempoOverridden            // 120 == default
                && !l2.blocks[1]->clips[0]->tempoOverridden; // 120 == block

            // (e) reset clears the override AND re-inherits (clip <- block,
            //     block <- project default incl. its inheriting clips)
            Project r;
            r.defaultClipTempo = 120.0;
            auto* R = r.addBlock("R"); addClipTo(R, "r1"); addClipTo(R, "r2");
            r.setBlockTempo(*R, 100.0);
            r.setClipTempo(*R->clips[0], 150.0);
            r.resetClipTempoToInherited(*R->clips[0], *R);
            bool eClipOk = !R->clips[0]->tempoOverridden && near(R->clips[0]->tempo, 100.0);
            r.setClipTempo(*R->clips[1], 88.0);              // survives the block reset
            r.resetBlockTempoToInherited(*R);
            bool eBlockOk = !R->tempoOverridden && near(R->tempo, 120.0)
                && near(R->clips[0]->tempo, 120.0) && !R->clips[0]->tempoOverridden
                && near(R->clips[1]->tempo,  88.0) &&  R->clips[1]->tempoOverridden;

            // (f) clip drag between blocks: overridden clip KEEPS its tempo,
            //     inheriting clip adopts the target block's tempo.
            // GUI-DISPATCH MIRROR: must stay equivalent to the tempo-retarget in
            // MainComponent.cpp onClipDropped (removeAndReturn + gated adopt).
            Project d;
            d.defaultClipTempo = 120.0;
            auto* S = d.addBlock("S"); auto* T = d.addBlock("T");
            d.setBlockTempo(*T, 160.0);
            addClipTo(S, "keep"); addClipTo(S, "adopt");
            Clip* keepC  = S->clips[0];
            Clip* adoptC = S->clips[1];
            d.setClipTempo(*keepC, 150.0);
            auto dragClip = [](Project& pr, Block* src, Block* dst, Clip* mc) {
                auto pre = pr.toJSON();
                for (int i = 0; i < src->clips.size(); ++i) {
                    if (src->clips[i] == mc) {
                        Clip* rawClip = src->clips.removeAndReturn(i);
                        if (!rawClip->tempoOverridden)
                            rawClip->tempo = dst->tempo > 0.0 ? dst->tempo : rawClip->tempo;
                        dst->clips.add(rawClip);
                        break;
                    }
                }
                pr.applyExternalMutation(pre);
            };
            dragClip(d, S, T, keepC);
            dragClip(d, S, T, adoptC);
            bool fOk =  keepC->tempoOverridden && near(keepC->tempo, 150.0)
                    && !adoptC->tempoOverridden && near(adoptC->tempo, 160.0)
                    && T->clips.size() == 2 && S->clips.isEmpty();

            verdict("T52 tempo override: setters gate on flags; flags survive save/undo; legacy inferred; reset re-inherits; drag keeps override",
                    aOk && bOk && cLoadOk && cUndoOk && dOk && eClipOk && eBlockOk && fOk,
                    juce::String("a(setBlockTempo)=") + (aOk ? "ok" : "BAD")
                    + ", b(setDefaultTempo)=" + (bOk ? "ok" : "BAD")
                    + ", c(load)=" + (cLoadOk ? "ok" : "BAD")
                    + ", c(undo)=" + (cUndoOk ? "ok" : "BAD")
                    + ", d(legacyInfer)=" + (dOk ? "ok" : "BAD")
                    + ", e(resetClip)=" + (eClipOk ? "ok" : "BAD")
                    + ", e(resetBlock)=" + (eBlockOk ? "ok" : "BAD")
                    + ", f(dragKeeps)=" + (fOk ? "ok" : "BAD"));
        }

        std::cout << "STEP6 RESULT: " << (failed == 0 ? "ALL PASS" : juce::String(failed) + " FAILED")
                  << "\n";
    }

    std::cout << "DONE\n";
    return 0;
}
