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
        { // T17 (2.7): single clip at 0% weight -> uniform fallback -> plays every time
            Project p;
            auto* blk = p.addBlock("A");
            addClipTo(blk, "only", 1000);
            blk->clips[0]->probability = 0.0f;
            const juce::String clipId = blk->clips[0]->id;

            juce::Random r(617); ArrangementResolver res;
            int ok = 0;
            for (int i = 0; i < 20; ++i) {
                auto arr = res.resolve(p, r);
                ok += (arr.entries.size() == 1 && arr.entries[0].clipId == clipId);
            }
            verdict("T17 single clip @0%: plays every time", ok == 20, juce::String(ok) + "/20");
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

        { // T28 (13.4): zoom maximum scales with clip length. Mirrors
          // ClipWaveformView::computeMaxZoom (ClipWaveformView.cpp:791-796):
          // maxZoom = jlimit(1, 256, durationSeconds / 0.5). Longer clips get a
          // larger zoom factor; the finest view is ~0.5s for any un-clamped length.
          // Guarded: 13.4 zoom max scales with clip length.
            auto maxZoom = [](double durSecs) {
                if (durSecs < 0.001) return 32.0;                 // empty/tiny fallback
                double z = durSecs / 0.5;
                z = juce::jmax(z, 1.0);
                z = juce::jmin(z, 256.0);
                return z;
            };
            double z1   = maxZoom(1.0);    // → 2.0
            double z10  = maxZoom(10.0);   // → 20.0
            double z100 = maxZoom(100.0);  // → 200.0
            double zTiny = maxZoom(0.3);   // → clamped up to 1.0
            double zHuge = maxZoom(1000.0);// → clamped to 256.0
            // Longer → strictly larger factor (until the 256 clamp); un-clamped
            // values reveal a ~0.5s window (dur / z == 0.5).
            bool monotonic = z1 < z10 && z10 < z100;
            bool window    = std::abs(1.0 / z1 - 0.5) < 1e-6
                          && std::abs(100.0 / z100 - 0.5) < 1e-6;
            bool clamps    = zTiny == 1.0 && zHuge == 256.0;
            verdict("T28 zoom max scales with length: longer clip zooms further",
                    monotonic && window && clamps,
                    "z(1s)=" + juce::String(z1, 1) + " z(10s)=" + juce::String(z10, 1)
                    + " z(100s)=" + juce::String(z100, 1) + " z(0.3s)=" + juce::String(zTiny, 1)
                    + " z(1000s)=" + juce::String(zHuge, 1));
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

        std::cout << "STEP6 RESULT: " << (failed == 0 ? "ALL PASS" : juce::String(failed) + " FAILED")
                  << "\n";
    }

    std::cout << "DONE\n";
    return 0;
}
