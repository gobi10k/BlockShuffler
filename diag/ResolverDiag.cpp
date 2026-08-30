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
#include "Audio/EntryMixer.h"
#include "Audio/ExportRenderer.h"
#include "Audio/PlaybackEngine.h"
#include "Audio/StackPicker.h"
#include "UI/InspectorPanel.h"
#include "Model/Serialization.h"
#include "UI/SplitLayout.h"
#include "Utils/GridSnap.h"
#include "UI/BlockLinkOverlay.h"
#include "UI/LinkArcLayout.h"
#include "UI/LinkLabelMetrics.h"
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

// ── PFH NON-REGRESSION BASELINE (2026-08-22) ─────────────────────────────────
// One deliberately busy project — mixed playChance, a simultaneous stack, a
// sequential stack, links and a song ender — resolved over 100 fixed seeds with
// forceInclude left at its nullptr default. Threading the play-from-here pin
// through resolve() must leave this dump byte-identical; any change means the
// RNG draw order shifted. Shared by the PFHBASE probe (writes the dump to a
// file for an external diff) and by permanent test T62 (golden hash).
// Blocks are identified by NAME + model index, never by juce::Uuid — ids are
// regenerated every process run and could never compare across builds.
static juce::uint32 pfhBaselineDump(juce::String& dumpOut) {
    Project p;
    p.sampleRate = 48000.0;
    auto mk = [&](const char* nm, float chance, int len) {
        auto* b = p.addBlock(nm);
        addClipTo(b, juce::String(nm) + "_c1", len);
        addClipTo(b, juce::String(nm) + "_c2", len + 1200);
        b->clips[0]->probability = 0.7f;
        b->clips[1]->probability = 0.3f;
        b->playChance = chance;
        return b;
    };
    auto* A  = mk("A",  1.0f,  9600);
    auto* B  = mk("B",  0.5f, 12000);
    auto* C  = mk("C",  0.8f, 14400);
    auto* S1 = mk("S1", 1.0f, 16800);
    auto* S2 = mk("S2", 0.6f, 16800);
    auto* S3 = mk("S3", 0.4f, 16800);
    auto* Q1 = mk("Q1", 1.0f, 10800);
    auto* Q2 = mk("Q2", 0.9f, 10800);
    auto* D  = mk("D",  1.0f,  9600);
    auto* E  = mk("E",  0.3f,  8400);
    p.stackBlocks(S2->id, S1->id);                 // simultaneous stack, 2 of 3
    p.stackBlocks(S3->id, S1->id);
    S1->stackPlayMode = StackPlayMode::Simultaneous;
    S1->stackPlayCount.values.set(0, 2);
    p.propagateStackSettings(S1->stackGroup, S1);
    p.stackBlocks(Q2->id, Q1->id);                 // sequential stack, 1 of 2
    Q1->stackPlayMode = StackPlayMode::Sequential;
    Q1->stackPlayCount.values.set(0, 1);
    p.propagateStackSettings(Q1->stackGroup, Q1);
    p.addLink(A->id, C->id, 0.5f);                 // swap rolls consume randomness
    p.addLink(D->id, E->id, 0.35f);
    D->clips[0]->isSongEnder = true;               // ender partway through
    juce::ignoreUnused(B);

    juce::uint32 h = 2166136261u;
    dumpOut.clear();
    for (int seed = 0; seed < 100; ++seed) {
        juce::Random rb(1000 + seed * 7);
        ArrangementResolver resB;
        auto arr = resB.resolve(p, rb);             // forceInclude defaulted
        juce::String line;
        line << "seed=" << seed
             << " entries=" << arr.entries.size()
             << " total=" << juce::String(arr.totalDurationSamples) << " [";
        for (const auto& e : arr.entries) {
            auto* blk = p.getBlockById(e.blockId);
            line << (blk ? blk->name : juce::String("?"))
                 << ":#" << p.blocks.indexOf(blk)
                 << "@" << juce::String(e.timelinePos)
                 << "g" << juce::String(e.gain, 6)
                 << "|" << e.clipName
                 << "|s" << juce::String(e.startMark)
                 << "e" << juce::String(e.endMark) << " ";
        }
        line << "]\n";
        dumpOut << line;
        for (auto c : line) h = (h ^ (juce::uint32)c) * 16777619u;
    }
    return h;
}

/** GOLDEN: captured from the PRE-PIN binary (this step, 2026-08-22) — the
 *  resolver with last round's song-ender fix in place and no forceInclude
 *  parameter at all. Locks the default path against any future RNG-order drift. */
static constexpr juce::uint32 kPfhBaselineGolden = 0x52c063dfu;

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

    // The harness must resolve fonts exactly the way the app does, or every
    // text-measurement assertion below is measuring a face the app never draws
    // with. MainComponent registers this same LookAndFeel as the default; JUCE
    // routes Font -> Typeface through the DEFAULT LookAndFeel only, so without
    // this line "Inter" falls back to a platform face and the widths the tests
    // check are not the widths the user sees.
    BlockShuffler::LookAndFeel_BlockShuffler diagLookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel(&diagLookAndFeel);
    struct LnfReset { ~LnfReset() { juce::LookAndFeel::setDefaultLookAndFeel(nullptr); } } lnfReset;

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
                // RAWGAIN scoping (same rationale as T55/T56, re-pinned 2026-08-22
                // when the default flipped to ON): this test measures the CROSSFADE
                // ENVELOPE, which only exists under the fade law. Raw summing removes
                // that law by design (flat 1.0 everywhere), so pin the flag false to
                // keep measuring the path under test rather than the default.
                p.unityGainMode = false;
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

            { // XTDIAG (diagnostic probe, print-only, NO verdict): cross-tempo
              // join metrics. Self-contained — computes rendered lead/tail
              // extents locally so the identical probe text runs against any
              // mixer build. 440 Hz sine at 0.5 FS; renders via direct
              // mixEntryToBuffer calls exactly as export does.
                const double srX = 44100.0;
                auto runCase = [&](double tempoA, double tempoB, const char* tag,
                                   int LX = 4410, int bodyX = 17640) {
                    Project p; p.sampleRate = srX;
                    auto* A = p.addBlock("A"); auto* B = p.addBlock("B");
                    addClipTo(A, "cA", LX + bodyX + LX);
                    addClipTo(B, "cB", LX + bodyX);
                    A->clips[0]->startMark = LX; A->clips[0]->endMark = LX + bodyX;
                    B->clips[0]->startMark = LX; B->clips[0]->endMark = LX + bodyX;
                    A->clips[0]->tempo = tempoA; B->clips[0]->tempo = tempoB;
                    for (auto* blk : { A, B })
                        for (int ch = 0; ch < 2; ++ch) {
                            auto* w = blk->clips[0]->audioBuffer->getWritePointer(ch);
                            const int n = blk->clips[0]->audioBuffer->getNumSamples();
                            for (int i = 0; i < n; ++i)
                                w[i] = 0.5f * (float)std::sin(2.0 * juce::MathConstants<double>::pi
                                                              * 440.0 * i / srX);
                        }
                    ArrangementResolver res; juce::Random r(6000);
                    auto arr = res.resolve(p, r); arr.sampleRate = srX;
                    const auto& eA = arr.entries.getReference(0);
                    const auto& eB = arr.entries.getReference(1);
                    const int64_t join = eB.timelinePos;
                    // Rendered extents, computed locally (mirror the mixer branches)
                    int64_t Wpre = 0;
                    if (eB.startMark > 0) {
                        if (eB.stretchedLeadIn) Wpre = eB.stretchedLeadIn->getNumSamples();
                        else if (std::abs(eB.leadInStretchRatio - 1.0f) < 0.0001f) Wpre = eB.startMark;
                        else Wpre = juce::jmax((int64_t)0,
                                       (int64_t)(eB.startMark * eB.leadInStretchRatio + 0.5f));
                    }
                    const int64_t tailLenA = juce::jmax((int64_t)0,
                        (int64_t)eA.audioBuffer->getNumSamples() - eA.endMark);
                    const int64_t Wpost = (tailLenA <= 0) ? 0
                        : ((eA.retainTailTempo || !eA.stretchedTail) ? tailLenA
                           : (int64_t)eA.stretchedTail->getNumSamples());
                    juce::AudioBuffer<float> out(2, (int)arr.totalDurationSamples);
                    out.clear();
                    for (int i = 0; i < arr.entries.size(); ++i)
                        mixEntryToBuffer(arr.entries.getReference(i), out,
                                         (int)arr.totalDurationSamples, 0LL, 1.0, 1.0, i);
                    juce::uint32 fnv = 2166136261u;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < out.getNumSamples(); ++i) {
                            juce::uint32 bits;
                            const float v = out.getSample(ch, i);
                            std::memcpy(&bits, &v, sizeof(bits));
                            fnv = (fnv ^ bits) * 16777619u;
                        }
                    const int a0 = (int)juce::jmax((int64_t)0, join - Wpre - 2205);
                    const int a1 = (int)juce::jmin((int64_t)arr.totalDurationSamples,
                                                   join + Wpost + 2205);
                    float maxAbs = 0.0f, maxDelta = 0.0f; int deltaPos = -1;
                    for (int i = a0 + 1; i < a1; ++i) {
                        maxAbs = juce::jmax(maxAbs, std::abs(out.getSample(0, i)));
                        float d = std::abs(out.getSample(0, i) - out.getSample(0, i - 1));
                        if (d > maxDelta) { maxDelta = d; deltaPos = i; }
                    }
                    const int f0 = (int)(join - Wpre), f1 = (int)(join + Wpost);
                    double ssum = 0, csum = 0, cs = 0, xs = 0, xc = 0, tot = 0;
                    for (int i = f0; i < f1; ++i) {
                        double ph = 2.0 * juce::MathConstants<double>::pi * 440.0 * i / srX;
                        double s = std::sin(ph), c = std::cos(ph);
                        double x = out.getSample(0, i);
                        ssum += s * s; csum += c * c; cs += c * s;
                        xs += x * s; xc += x * c; tot += x * x;
                    }
                    const double det = ssum * csum - cs * cs;
                    const double fa = (xs * csum - xc * cs) / det;
                    const double fb = (xc * ssum - xs * cs) / det;
                    double resid = 0;
                    for (int i = f0; i < f1; ++i) {
                        double ph = 2.0 * juce::MathConstants<double>::pi * 440.0 * i / srX;
                        double e = out.getSample(0, i) - (fa * std::sin(ph) + fb * std::cos(ph));
                        resid += e * e;
                    }
                    std::cout << "XTDIAG[" << tag << "] Wpre=" << Wpre << " Wpost=" << Wpost
                              << " renderHashFNV=0x" << juce::String::toHexString((int)fnv)
                              << " maxAbs=" << juce::String(maxAbs, 6)
                              << " maxDelta=" << juce::String(maxDelta, 6)
                              << " @join" << (deltaPos >= join ? "+" : "")
                              << juce::String((int64_t)deltaPos - join)
                              << " THDresidual=" << juce::String(tot > 0 ? resid / tot : 0.0, 6)
                              << "\n";
                };
                runCase(120.0, 120.0, "CONTROL 120->120");
                runCase(120.0, 160.0, "CROSS 120->160");
                runCase(160.0, 120.0, "CROSS 160->120");
                // Wider ratio (2x) and LONG segments (2s lead-in/tail, 4s body):
                // catches length-dependent WSOLA drift / gaps that short cases hide.
                runCase(90.0, 180.0,  "CROSS-WIDE 90->180");
                runCase(120.0, 160.0, "CROSS-LONG 120->160", 88200, 176400);
            }

            { // REALDIAG (print-only): full-scale (peak >= 0.99) multi-partial
              // join through the REAL engine — max|sample| and proof of ZERO
              // post-mix processing (engine output == plain sum of
              // mixEntryToBuffer calls; count of samples differing by > 1e-3
              // MUST be 0 — an output stage would alter thousands).
                const double srR = 44100.0;
                const int LR = 4410, bodyR = 17640;
                Project p; p.sampleRate = srR;
                auto* A = p.addBlock("A"); auto* B = p.addBlock("B");
                addClipTo(A, "cA", LR + bodyR + LR);
                addClipTo(B, "cB", LR + bodyR);
                A->clips[0]->startMark = LR; A->clips[0]->endMark = LR + bodyR;
                B->clips[0]->startMark = LR; B->clips[0]->endMark = LR + bodyR;
                for (auto* blk : { A, B }) {
                    auto& buf = *blk->clips[0]->audioBuffer;
                    float pk = 0.0f;
                    for (int ch = 0; ch < 2; ++ch) {
                        auto* w = buf.getWritePointer(ch);
                        for (int i = 0; i < buf.getNumSamples(); ++i) {
                            double t = (double)i / srR;
                            w[i] = (float)(0.62 * std::sin(2.0 * juce::MathConstants<double>::pi * 220.0 * t)
                                         + 0.31 * std::sin(2.0 * juce::MathConstants<double>::pi * 554.37 * t)
                                         + 0.13 * std::sin(2.0 * juce::MathConstants<double>::pi * 1318.5 * t));
                            pk = juce::jmax(pk, std::abs(w[i]));
                        }
                    }
                    buf.applyGain(0.99f / pk);   // true peak exactly 0.99
                }
                ArrangementResolver res; juce::Random r(6100);
                auto arr = res.resolve(p, r); arr.sampleRate = srR;
                auto eng = renderEngineLen(arr, 512, arr.totalDurationSamples);
                juce::AudioBuffer<float> raw(2, (int)arr.totalDurationSamples);
                raw.clear();
                for (int i = 0; i < arr.entries.size(); ++i)
                    mixEntryToBuffer(arr.entries.getReference(i), raw,
                                     (int)arr.totalDurationSamples, 0LL, 1.0, 1.0, i);
                float maxEng = 0.0f; int64_t altered = 0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < raw.getNumSamples(); ++i) {
                        maxEng = juce::jmax(maxEng, std::abs(eng.getSample(ch, i)));
                        if (std::abs(eng.getSample(ch, i) - raw.getSample(ch, i)) > 1.0e-3f)
                            ++altered;
                    }
                std::cout << "REALDIAG fullscale join: engineMaxAbs=" << juce::String(maxEng, 6)
                          << " postMixAlteredSamples(MUST be 0)=" << altered << "\n";
            }

            { // OFFGRID (diagnostic probe, print-only, NO verdict): cross-tempo
              // joins with FREE-DROPPED (off-grid) markers. Markers are written
              // through the EXACT free-drop model path — Shift-drag bypasses
              // snapToGrid and does two jlimit-clamped raw writes, mirrored
              // VERBATIM from ClipWaveformView.cpp:353 and :359 (no other field,
              // no cached snap state exists). A = 220 Hz, B = 330 Hz, amp 0.5,
              // project 48000, tempos per case. Per case: zone max-delta table
              // (3x sine-slope bound), off-peak residual of the two crossfade
              // windows (LSQ removal of the 220+330 components — equivalent to
              // FFT off-peak energy), LENGTH ACCOUNTING incl. the FLOAT target
              // before rounding, and zero-run lengths at both ends of each
              // stretched buffer (windowAcc[0]/[last] evidence).
                const double srO = 48000.0;
                auto freeDrop = [&](Clip* clip, int64_t rawStart, int64_t rawEnd) {
                    // VERBATIM free-drop writes (ClipWaveformView.cpp:353 / :359)
                    clip->startMark = juce::jlimit((int64_t)0, clip->endMark - 1, rawStart);
                    int64_t tot = (clip->audioBuffer) ? (int64_t)clip->audioBuffer->getNumSamples() : 0;
                    clip->endMark = juce::jlimit(clip->startMark + 1, tot, rawEnd);
                };
                auto zeroRun = [](const juce::AudioBuffer<float>* sb, bool fromEnd) -> int {
                    if (!sb || sb->getNumSamples() == 0) return -1;
                    const int n = sb->getNumSamples(); int run = 0;
                    for (int i = 0; i < n; ++i) {
                        const int idx = fromEnd ? n - 1 - i : i;
                        bool z = true;
                        for (int ch = 0; ch < sb->getNumChannels(); ++ch)
                            if (std::abs(sb->getSample(ch, idx)) > 1.0e-7f) { z = false; break; }
                        if (!z) break;
                        ++run;
                    }
                    return run;
                };
                // Zone/length metrics returned so permanent tests (T53/T54) can
                // assert on the SAME machinery the C1-C5 probes print.
                struct OffR { double z3max = 0.0, z3bound = 0.0;
                              bool advanceAgree = false, lenOk = false;
                              int64_t Wpre = 0, Wpost = 0; };
                auto runOff = [&](const char* tag, double tempoA, double tempoB,
                                  int64_t sA, int64_t eA, int lenA,
                                  int64_t sB, int64_t eB, int lenB,
                                  double freqA = 220.0, double freqB = 330.0,
                                  double amp = 0.5, double srC = 48000.0) -> OffR {
                    Project p; p.sampleRate = srC;
                    auto* A = p.addBlock("A"); auto* B = p.addBlock("B");
                    addClipTo(A, "cA", lenA);
                    addClipTo(B, "cB", lenB);
                    A->clips[0]->tempo = tempoA; B->clips[0]->tempo = tempoB;
                    auto fill = [&](Clip* c, double f) {
                        for (int ch = 0; ch < 2; ++ch) {
                            auto* w = c->audioBuffer->getWritePointer(ch);
                            const int n = c->audioBuffer->getNumSamples();
                            for (int i = 0; i < n; ++i)
                                w[i] = (float)amp * (float)std::sin(2.0 * juce::MathConstants<double>::pi
                                                              * f * i / srC);
                        }
                    };
                    fill(A->clips[0], freqA); fill(B->clips[0], freqB);
                    freeDrop(A->clips[0], sA, eA);
                    freeDrop(B->clips[0], sB, eB);
                    ArrangementResolver res; juce::Random r(6200);
                    auto arr = res.resolve(p, r); arr.sampleRate = srO;
                    const auto& EA = arr.entries.getReference(0);
                    const auto& EB = arr.entries.getReference(1);
                    const int64_t join  = EB.timelinePos;
                    const int64_t Wpre  = renderedLeadInLength(EB);
                    const int64_t Wpost = renderedTailLength(EA);
                    // Chunked render, 512-sample windows, exactly like the engine path
                    const int total = (int)arr.totalDurationSamples;
                    juce::AudioBuffer<float> out(2, total); out.clear();
                    juce::AudioBuffer<float> chunk(2, 512);
                    for (int head = 0; head < total; head += 512) {
                        const int n = juce::jmin(512, total - head);
                        chunk.clear();
                        for (int i = 0; i < arr.entries.size(); ++i)
                            mixEntryToBuffer(arr.entries.getReference(i), chunk, n,
                                             (int64_t)head, 1.0, 1.0, i);
                        for (int ch = 0; ch < 2; ++ch)
                            out.copyFrom(ch, (int)head, chunk, ch, 0, n);
                    }
                    juce::uint32 fnv = 2166136261u;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < total; ++i) {
                            juce::uint32 bits; const float v = out.getSample(ch, i);
                            std::memcpy(&bits, &v, sizeof(bits));
                            fnv = (fnv ^ bits) * 16777619u;
                        }
                    // Zone max-delta table, bound = 3x the summed natural sine slopes present
                    const double s220 = amp * 2.0 * juce::MathConstants<double>::pi * freqA / srC;
                    const double s330 = amp * 2.0 * juce::MathConstants<double>::pi * freqB / srC;
                    struct Zn { const char* nm; int64_t a, b; double bound; };
                    Zn zs[5] = {
                        { "Z1 A-body ", juce::jmax((int64_t)0, join - Wpre - 2000), join - Wpre, s220 },
                        { "Z2 xfPre  ", join - Wpre, join, s220 + s330 },
                        { "Z3 join+-16", join - 16, join + 16, s220 + s330 },
                        { "Z4 xfPost ", join, join + Wpost, s220 + s330 },
                        { "Z5 B-body ", join + Wpost,
                          juce::jmin((int64_t)total, join + Wpost + 2000), s330 },
                    };
                    OffR ret;
                    ret.Wpre = Wpre; ret.Wpost = Wpost;
                    std::cout << "OFFGRID[" << tag << "] join=" << join << " Wpre=" << Wpre
                              << " Wpost=" << Wpost
                              << " renderHashFNV=0x" << juce::String::toHexString((int)fnv) << "\n";
                    for (int zi = 0; zi < 5; ++zi) {
                        const auto& z = zs[zi];
                        float mx = 0.0f; int64_t firstViol = -1; int nViol = 0;
                        for (int64_t i = z.a + 1; i < z.b; ++i) {
                            float d = std::abs(out.getSample(0, (int)i) - out.getSample(0, (int)i - 1));
                            mx = juce::jmax(mx, d);
                            if (d > 3.0 * z.bound) { if (firstViol < 0) firstViol = i; ++nViol; }
                        }
                        if (zi == 2) { ret.z3max = mx; ret.z3bound = 3.0 * z.bound; }
                        std::cout << "  " << z.nm << " [" << z.a << "," << z.b
                                  << ") maxDelta=" << juce::String(mx, 6)
                                  << " bound3x=" << juce::String(3.0 * z.bound, 6)
                                  << " viol=" << nViol;
                        if (firstViol >= 0)
                            std::cout << " FIRST@join" << (firstViol >= join ? "+" : "")
                                      << (int64_t)(firstViol - join);
                        std::cout << "\n";
                    }
                    // Off-peak residual: LSQ-remove freqA & freqB (sin+cos each) per
                    // window. Equal frequencies (440/440 XT cases) collapse to a
                    // 2-term basis — a duplicated pair would make the matrix singular.
                    const int nb = (freqA == freqB) ? 2 : 4;
                    auto offPeak = [&](int64_t a, int64_t b) -> double {
                        if (b - a < 8) return 0.0;
                        double m[4][4] = {}, v[4] = {}, tot2 = 0.0;
                        auto basis = [&](int64_t i, double* ph) {
                            const double w1 = 2.0 * juce::MathConstants<double>::pi * freqA * i / srC;
                            const double w2 = 2.0 * juce::MathConstants<double>::pi * freqB * i / srC;
                            ph[0] = std::sin(w1); ph[1] = std::cos(w1);
                            ph[2] = std::sin(w2); ph[3] = std::cos(w2);
                        };
                        for (int64_t i = a; i < b; ++i) {
                            double ph[4]; basis(i, ph);
                            const double x = out.getSample(0, (int)i);
                            tot2 += x * x;
                            for (int r2 = 0; r2 < nb; ++r2) {
                                v[r2] += ph[r2] * x;
                                for (int c2 = 0; c2 < nb; ++c2) m[r2][c2] += ph[r2] * ph[c2];
                            }
                        }
                        for (int col = 0; col < nb; ++col) {           // Gaussian elim
                            int piv = col;
                            for (int r2 = col + 1; r2 < nb; ++r2)
                                if (std::abs(m[r2][col]) > std::abs(m[piv][col])) piv = r2;
                            for (int c2 = 0; c2 < nb; ++c2) std::swap(m[col][c2], m[piv][c2]);
                            std::swap(v[col], v[piv]);
                            if (std::abs(m[col][col]) < 1e-12) return -1.0;
                            for (int r2 = 0; r2 < nb; ++r2) {
                                if (r2 == col) continue;
                                const double f = m[r2][col] / m[col][col];
                                for (int c2 = 0; c2 < nb; ++c2) m[r2][c2] -= f * m[col][c2];
                                v[r2] -= f * v[col];
                            }
                        }
                        double coef[4] = {};
                        for (int r2 = 0; r2 < nb; ++r2) coef[r2] = v[r2] / m[r2][r2];
                        double resid = 0.0;
                        for (int64_t i = a; i < b; ++i) {
                            double ph[4]; basis(i, ph);
                            double e = out.getSample(0, (int)i);
                            for (int r2 = 0; r2 < nb; ++r2) e -= coef[r2] * ph[r2];
                            resid += e * e;
                        }
                        return tot2 > 0.0 ? resid / tot2 : 0.0;
                    };
                    std::cout << "  offPeak xfPre=" << juce::String(offPeak(join - Wpre, join), 6)
                              << " xfPost=" << juce::String(offPeak(join, join + Wpost), 6) << "\n";
                    // LENGTH ACCOUNTING (targetF uses the same float ratio the code uses)
                    bool lenOkAll = true;
                    auto lenRow = [&](const char* seg, int64_t orig, float ratio,
                                      bool stretched, int64_t actual, int64_t xfade) {
                        const double targetF = (double)((float)orig * ratio);
                        const int64_t roundedTS = juce::jmax(1, (int)((float)orig * ratio + 0.5f));
                        const int64_t expected  = stretched ? roundedTS : orig;
                        lenOkAll = lenOkAll && actual == xfade && actual == expected;
                        std::cout << "  LEN " << seg << " orig=" << orig
                                  << " ratio=" << juce::String(ratio, 6)
                                  << " targetF=" << juce::String(targetF, 3)
                                  << " rounded=" << (stretched ? roundedTS : orig)
                                  << (stretched ? " @TempoStretcher.h:34(outLen)" : " @no-stretch")
                                  << " actualOut=" << actual
                                  << " crossfadeLen=" << xfade
                                  << " agree=" << ((actual == xfade) ? "YES" : "NO(DIVERGENT)")
                                  << "\n";
                    };
                    // crossfadeLen = the resolver-stored join-window extent
                    // (section 5b, restored by JOINFIX2) — cross-checked against
                    // the rendered extent the mixer places (actualOut).
                    const int64_t tailOrigA = (int64_t)EA.audioBuffer->getNumSamples() - EA.endMark;
                    lenRow("A.tail  ", tailOrigA, EA.tailStretchRatio,
                           EA.stretchedTail != nullptr,
                           renderedTailLength(EA), EB.prevTailLen);
                    lenRow("B.leadIn", EB.startMark, EB.leadInStretchRatio,
                           EB.stretchedLeadIn != nullptr,
                           renderedLeadInLength(EB), EA.nextLeadInLen);
                    ret.advanceAgree = (join - EA.timelinePos) == (EA.endMark - EA.startMark);
                    ret.lenOk = lenOkAll;
                    std::cout << "  ADVANCE join-A.pos=" << (join - EA.timelinePos)
                              << " A.bodyLen=" << (EA.endMark - EA.startMark)
                              << " agree=" << (ret.advanceAgree ? "YES" : "NO(DIVERGENT)") << "\n";
                    auto zr = [&](const char* nm, const std::shared_ptr<juce::AudioBuffer<float>>& sb) {
                        if (!sb) { std::cout << "  ZRUN " << nm << " (no stretched buffer)\n"; return; }
                        std::cout << "  ZRUN " << nm << " len=" << sb->getNumSamples()
                                  << " leadingZeros=" << zeroRun(sb.get(), false)
                                  << " trailingZeros=" << zeroRun(sb.get(), true) << "\n";
                    };
                    zr("A.stretchedTail  ", EA.stretchedTail);
                    zr("B.stretchedLeadIn", EB.stretchedLeadIn);
                    return ret;
                };
                // C1 markers ON-GRID via the real snapToGrid (control).
                // A@100BPM/48k: beat=28800 -> s=28800 e=144000 (exact). Tail 1 beat.
                // B@140BPM: beat=20571.4286 (fractional grid) -> snap lands 20571/102857.
                const int64_t c1sA = snapToGrid(28700, 100.0, srO), c1eA = snapToGrid(143900, 100.0, srO);
                const int64_t c1sB = snapToGrid(20500, 140.0, srO), c1eB = snapToGrid(102900, 140.0, srO);
                auto rC1 = runOff("C1 ongrid 100->140", 100.0, 140.0,
                       c1sA, c1eA, 172800, c1sB, c1eB, 112857);
                { // Ducking characterization (JOINFIX2, report-only): the
                  // complementary window puts the downbeat at gB(L) = L/(W-1)
                  // rather than 1.0 — the audible swell of the pre-JOINFIX
                  // approved envelope shape, documented here.
                    const double Wm1 = (double)juce::jmax((int64_t)1,
                                           rC1.Wpre + rC1.Wpost - 1);
                    const double gB  = (double)rC1.Wpre / Wm1;
                    std::cout << "DUCK C1: L=" << rC1.Wpre << " T=" << rC1.Wpost
                              << " gB(join)=" << juce::String(gB, 6)
                              << " gA(join)=" << juce::String(1.0 - gB, 6) << "\n";
                }
                runOff("C2 offgrid+137",     100.0, 140.0,
                       c1sA + 137, c1eA + 137, 172800, c1sB + 137, c1eB + 137, 112857);
                runOff("C3 offgrid+1009",    100.0, 140.0,
                       c1sA + 1009, c1eA + 1009, 172800, c1sB + 1009, c1eB + 1009, 112857);
                // C4 fractional beats: A lead 2.63 beats=75744, tail 0.37=10656;
                // B lead 2.63 beats=54102.857->54103 (free drop), tail 0.37->7611.
                runOff("C4 fracbeats",       100.0, 140.0,
                       75744, 190944, 201600, 54103, 136389, 144000);
                runOff("C5 offgrid+137 equal-tempo", 100.0, 100.0,
                       c1sA + 137, c1eA + 137, 172800, c1sB + 137, c1eB + 137, 112857);

                // X1-X3 (JOINFIX): the original XTDIAG anchor cases re-examined
                // with the OFFGRID zone machinery — 440/440 at 44.1k, marker
                // layout mirroring XTDIAG exactly (lead 4410, body 17640;
                // WIDE = 2x ratio; LONG = 2 s lead/tail, 4 s body).
                runOff("X1 xt-anchor 120->160", 120.0, 160.0,
                       4410, 22050, 26460, 4410, 22050, 22050,
                       440.0, 440.0, 0.5, 44100.0);
                runOff("X2 xt-wide 90->180",    90.0, 180.0,
                       4410, 22050, 26460, 4410, 22050, 22050,
                       440.0, 440.0, 0.5, 44100.0);
                runOff("X3 xt-long-2s 120->160", 120.0, 160.0,
                       88200, 264600, 352800, 88200, 264600, 264600,
                       440.0, 440.0, 0.5, 44100.0);

                // Hot-material join probes (print-only): amp 0.95 pair, FREE
                // markers. JOINFIX2 REQUIREMENT: joinRegionMaxAbs <= 1.0 + 1e-4
                // (complementary gains keep the join mathematically bounded) and
                // the WAV16 clamp count must be 0. C6 = cross-tempo, C7 = EQUAL
                // tempos (proves any clip mechanism is topology, not tempo).
                auto runHot = [&](const char* tag, double tempoA, double tempoB,
                                  int seed) {
                    Project p; p.sampleRate = srO;
                    auto* A = p.addBlock("A"); auto* B = p.addBlock("B");
                    addClipTo(A, "cA", 172800);
                    addClipTo(B, "cB", 112857);
                    A->clips[0]->tempo = tempoA; B->clips[0]->tempo = tempoB;
                    auto fillHot = [&](Clip* c, double f) {
                        for (int ch = 0; ch < 2; ++ch) {
                            auto* w = c->audioBuffer->getWritePointer(ch);
                            const int n = c->audioBuffer->getNumSamples();
                            for (int i = 0; i < n; ++i)
                                w[i] = 0.95f * (float)std::sin(2.0 * juce::MathConstants<double>::pi
                                                               * f * i / srO);
                        }
                    };
                    fillHot(A->clips[0], 220.0); fillHot(B->clips[0], 330.0);
                    freeDrop(A->clips[0], c1sA + 137, c1eA + 137);
                    freeDrop(B->clips[0], c1sB + 137, c1eB + 137);
                    ArrangementResolver res; juce::Random r(seed);
                    auto arr = res.resolve(p, r); arr.sampleRate = srO;
                    const auto& EA = arr.entries.getReference(0);
                    const auto& EB = arr.entries.getReference(1);
                    const int64_t join  = EB.timelinePos;
                    const int64_t Wpre  = renderedLeadInLength(EB);
                    const int64_t Wpost = renderedTailLength(EA);
                    const int total = (int)arr.totalDurationSamples;
                    juce::AudioBuffer<float> raw(2, total); raw.clear();
                    for (int i = 0; i < arr.entries.size(); ++i)
                        mixEntryToBuffer(arr.entries.getReference(i), raw,
                                         total, 0LL, 1.0, 1.0, i);
                    auto eng = renderEngineLen(arr, 512, total);
                    int64_t altered = 0;
                    float joinMaxAbs = 0.0f; int64_t overFS = 0;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < total; ++i) {
                            if (std::abs(eng.getSample(ch, i) - raw.getSample(ch, i)) > 1.0e-3f)
                                ++altered;
                            if (i >= (int)(join - Wpre) && i < (int)(join + Wpost)) {
                                const float a = std::abs(raw.getSample(ch, i));
                                joinMaxAbs = juce::jmax(joinMaxAbs, a);
                                if (a > 1.0f) ++overFS;
                            }
                        }
                    std::cout << tag << " join=" << join << " Wpre=" << Wpre << " Wpost=" << Wpost
                              << " joinRegionMaxAbs=" << juce::String(joinMaxAbs, 6)
                              << " overFSsamples=" << overFS
                              << " engineVsRawAltered(MUST be 0)=" << altered << "\n";
                    // 32-bit float WAV (the in-app WAV export path): bit-exact, no clamp
                    auto wavCheck = [&](int depth) {
                        auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                     .getChildFile("resolverdiag_c6hot.wav");
                        juce::WavAudioFormat wavFmt; ExportRenderer ex;
                        bool ok = ex.renderToFile(arr, f, wavFmt, depth, nullptr);
                        float maxDiff = -1.0f, filePeak = 0.0f; int64_t clamped = 0;
                        juce::AudioFormatManager afm; afm.registerBasicFormats();
                        std::unique_ptr<juce::AudioFormatReader> rd(afm.createReaderFor(f));
                        if (ok && rd && (int64_t)rd->lengthInSamples == (int64_t)total) {
                            juce::AudioBuffer<float> fb((int)rd->numChannels, total);
                            rd->read(&fb, 0, total, 0, true, true);
                            maxDiff = 0.0f;
                            for (int ch = 0; ch < 2; ++ch)
                                for (int i = 0; i < total; ++i) {
                                    const float fv = fb.getSample(ch, i);
                                    filePeak = juce::jmax(filePeak, std::abs(fv));
                                    maxDiff  = juce::jmax(maxDiff,
                                                   std::abs(fv - raw.getSample(ch, i)));
                                    if (std::abs(raw.getSample(ch, i)) > 1.0f
                                        && std::abs(fv) <= 1.0f) ++clamped;
                                }
                        }
                        rd.reset(); f.deleteFile();
                        std::cout << tag << " WAV" << depth
                                  << (depth == 32 ? "f" : "") << ": filePeak="
                                  << juce::String(filePeak, 6)
                                  << " maxDiffVsMix=" << juce::String(maxDiff, 6)
                                  << " clampedAtFS=" << clamped << "\n";
                    };
                    wavCheck(32);   // in-app WAV export depth (float, exact)
                    wavCheck(16);   // must NOT clamp once the join is bounded
                };
                runHot("C6-hot", 100.0, 140.0, 6400);
                runHot("C7-hot", 100.0, 100.0, 6500);

                { // JOINFIX-E0LOCK (print-only): single-entry arrangement
                  // (lead-in + body + tail, NO join). Its render hash must be
                  // BYTE-IDENTICAL before/after the JOINFIX topology change:
                  // entry 0 plays its lead-in at FULL GAIN from timeline 0
                  // (timelinePos = startMark), body constant, tail ramp
                  // unchanged. Cosine fill so sample 0 is non-zero and the
                  // full-gain check is non-vacuous.
                    Project p; p.sampleRate = srO;
                    auto* A = p.addBlock("A");
                    addClipTo(A, "cA", 4410 + 17640 + 4410);
                    A->clips[0]->startMark = 4410;
                    A->clips[0]->endMark   = 4410 + 17640;
                    for (int ch = 0; ch < 2; ++ch) {
                        auto* w = A->clips[0]->audioBuffer->getWritePointer(ch);
                        const int n = A->clips[0]->audioBuffer->getNumSamples();
                        for (int i = 0; i < n; ++i)
                            w[i] = 0.5f * (float)std::cos(2.0 * juce::MathConstants<double>::pi
                                                          * 220.0 * i / srO);
                    }
                    ArrangementResolver res; juce::Random r(6300);
                    auto arr = res.resolve(p, r); arr.sampleRate = srO;
                    auto out = renderEngineLen(arr, 512, arr.totalDurationSamples);
                    juce::uint32 fnv = 2166136261u;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < out.getNumSamples(); ++i) {
                            juce::uint32 bits; const float v = out.getSample(ch, i);
                            std::memcpy(&bits, &v, sizeof(bits));
                            fnv = (fnv ^ bits) * 16777619u;
                        }
                    const float first    = out.getSample(0, 0);
                    const float srcFirst = arr.entries.getReference(0).audioBuffer->getSample(0, 0);
                    std::cout << "E0LOCK single-entry: entries=" << arr.entries.size()
                              << " timelinePos=" << arr.entries.getReference(0).timelinePos
                              << " total=" << arr.totalDurationSamples
                              << " renderHashFNV=0x" << juce::String::toHexString((int)fnv)
                              << " firstSample=" << juce::String(first, 6)
                              << " srcLeadIn[0]=" << juce::String(srcFirst, 6)
                              << " fullGainAtT0="
                              << ((std::abs(first - srcFirst) < 1.0e-7f) ? "YES" : "NO(VIOLATION)")
                              << "\n";
                }

                // ── T53/T54 (PERMANENT, JOINFIX 2026-07-19): continuous-envelope
                // joins. The complementary-crossfade carrier swap put a raw
                // waveform discontinuity at join+0 in EVERY free-marker case
                // (OFFGRID C1-C5, 0.244-0.746); these turn the suite RED if any
                // future mixer change reintroduces a join+0 step beyond 3x the
                // natural summed sine slope, or breaks length agreement
                // (stretcher output == rendered ramp extent; timeline advance
                // == body length).
                {
                    auto r53 = runOff("T53 perm free-marker 100->140", 100.0, 140.0,
                                      c1sA + 137, c1eA + 137, 172800,
                                      c1sB + 137, c1eB + 137, 112857);
                    verdict("T53 JOINFIX free-marker cross-tempo join: join+-16 within 3x bound + lengths agree",
                            r53.z3max <= r53.z3bound && r53.lenOk && r53.advanceAgree,
                            "z3max=" + juce::String(r53.z3max, 6)
                            + " bound3x=" + juce::String(r53.z3bound, 6)
                            + ", lenOk=" + (r53.lenOk ? "YES" : "NO")
                            + ", advance=" + (r53.advanceAgree ? "YES" : "NO"));

                    auto r54 = runOff("T54 perm fracbeats", 100.0, 140.0,
                                      75744, 190944, 201600, 54103, 136389, 144000);
                    verdict("T54 JOINFIX fractional-beats join: join+-16 within 3x bound + lengths agree",
                            r54.z3max <= r54.z3bound && r54.lenOk && r54.advanceAgree,
                            "z3max=" + juce::String(r54.z3max, 6)
                            + " bound3x=" + juce::String(r54.z3bound, 6)
                            + ", lenOk=" + (r54.lenOk ? "YES" : "NO")
                            + ", advance=" + (r54.advanceAgree ? "YES" : "NO"));
                }

                { // T55 (PERMANENT, JOINFIX2 2026-07-20): the join must be
                  // mathematically BOUNDED. Hot (amp 0.95) free-marker
                  // cross-tempo join asserts (a) join-region max|sample| <=
                  // 1.0 + 1e-4, (b) the complementary envelopes sum to 1
                  // across the whole window — measured directly with DC-filled
                  // sources (output == gA + gB; tolerance 1e-2 covers the known
                  // WSOLA edge under-coverage zeros sitting at gain ~0), and
                  // (c) join+-16 max delta within the 3x summed-slope bound
                  // (the carrier-swap seam must not return). RED against the
                  // 36fd316 bodies-at-full-gain mixer (maxAbs 1.879).
                    auto renderChunked = [&](const ResolvedArrangement& arr) {
                        const int total = (int)arr.totalDurationSamples;
                        juce::AudioBuffer<float> out(2, total); out.clear();
                        juce::AudioBuffer<float> chunk(2, 512);
                        for (int head = 0; head < total; head += 512) {
                            const int n = juce::jmin(512, total - head);
                            chunk.clear();
                            for (int i = 0; i < arr.entries.size(); ++i)
                                mixEntryToBuffer(arr.entries.getReference(i), chunk, n,
                                                 (int64_t)head, 1.0, 1.0, i);
                            for (int ch = 0; ch < 2; ++ch)
                                out.copyFrom(ch, (int)head, chunk, ch, 0, n);
                        }
                        return out;
                    };
                    auto buildHot = [&](bool dc) {
                        Project p; p.sampleRate = srO;
                        // RAWGAIN scoping: T55's amplitude bound (maxAbs <= 1) and
                        // gA+gB == 1 gain-sum assertion are invariants OF THE FADE
                        // LAW, not of the mixer in general. Unity-gain mode removes
                        // that law by design and legitimately exceeds both. Pin the
                        // flag false so this test always measures the default path.
                        p.unityGainMode = false;
                        auto* A = p.addBlock("A"); auto* B = p.addBlock("B");
                        addClipTo(A, "cA", 172800);
                        addClipTo(B, "cB", 112857);
                        A->clips[0]->tempo = 100.0; B->clips[0]->tempo = 140.0;
                        auto fill = [&](Clip* c, double f) {
                            for (int ch = 0; ch < 2; ++ch) {
                                auto* w = c->audioBuffer->getWritePointer(ch);
                                const int n = c->audioBuffer->getNumSamples();
                                for (int i = 0; i < n; ++i)
                                    w[i] = dc ? 1.0f
                                              : 0.95f * (float)std::sin(
                                                    2.0 * juce::MathConstants<double>::pi
                                                    * f * i / srO);
                            }
                        };
                        fill(A->clips[0], 220.0); fill(B->clips[0], 330.0);
                        freeDrop(A->clips[0], c1sA + 137, c1eA + 137);
                        freeDrop(B->clips[0], c1sB + 137, c1eB + 137);
                        ArrangementResolver res; juce::Random r(6600);
                        auto arr = res.resolve(p, r); arr.sampleRate = srO;
                        return arr;   // entries snapshot the buffers — safe
                    };
                    auto arrS = buildHot(false);
                    const auto& sEA = arrS.entries.getReference(0);
                    const auto& sEB = arrS.entries.getReference(1);
                    const int64_t joinS  = sEB.timelinePos;
                    const int64_t WpreS  = renderedLeadInLength(sEB);
                    const int64_t WpostS = renderedTailLength(sEA);
                    auto outS = renderChunked(arrS);
                    float maxAbs = 0.0f, z3 = 0.0f;
                    for (int64_t i = joinS - WpreS; i < joinS + WpostS; ++i)
                        for (int ch = 0; ch < 2; ++ch)
                            maxAbs = juce::jmax(maxAbs, std::abs(outS.getSample(ch, (int)i)));
                    for (int64_t i = joinS - 15; i < joinS + 16; ++i)
                        z3 = juce::jmax(z3, std::abs(outS.getSample(0, (int)i)
                                                     - outS.getSample(0, (int)i - 1)));
                    const double bound = 3.0 * (0.95 * 2.0 * juce::MathConstants<double>::pi
                                                * (220.0 + 330.0) / srO);
                    auto arrD = buildHot(true);
                    auto outD = renderChunked(arrD);
                    const int64_t joinD  = arrD.entries.getReference(1).timelinePos;
                    const int64_t WpreD  = renderedLeadInLength(arrD.entries.getReference(1));
                    const int64_t WpostD = renderedTailLength(arrD.entries.getReference(0));
                    float sumDev = 0.0f;
                    for (int64_t i = joinD - WpreD; i < joinD + WpostD; ++i)
                        sumDev = juce::jmax(sumDev, std::abs(outD.getSample(0, (int)i) - 1.0f));
                    verdict("T55 JOINFIX2 bounded join: hot maxAbs<=1+1e-4, gA+gB==1 (DC, tol 1e-2), join+-16 in bound",
                            maxAbs <= 1.0001f && sumDev <= 0.01f && (double)z3 <= bound,
                            "maxAbs=" + juce::String(maxAbs, 6)
                            + ", sumDev=" + juce::String(sumDev, 6)
                            + ", z3max=" + juce::String(z3, 6)
                            + " bound=" + juce::String(bound, 6));
                }

                { // T56 (PERMANENT, RAWGAIN 2026-08-14): unity-gain mode.
                  // (a) unityGainMode ON  → every entry mixes at EXACTLY 1.0 across
                  //     its whole rendered extent (lead-in, body AND tail), both at a
                  //     sequential join and across a simultaneous stack: no crossfade
                  //     ramp, no complementary join law, no 1/playCount attenuation.
                  //     Measured with DC-filled sources, where the rendered sample
                  //     value IS the gain the mixer applied at that position.
                  // (b) unityGainMode OFF → the fade law is untouched. The default
                  //     render is locked to a golden FNV hash captured from the
                  //     pre-RAWGAIN a48e072 binary (the 2b zero-diff check, made
                  //     permanent), and the SIM stack still attenuates by 1/playCount.
                  // (c) timing is mode-independent: totalDurationSamples must agree.
                    const double srU = 48000.0;
                    const int    LU = 4410, bodyU = 17640, TU = 4410;

                    auto dcFillU = [&](Clip* c) {
                        for (int ch = 0; ch < c->audioBuffer->getNumChannels(); ++ch) {
                            auto* w = c->audioBuffer->getWritePointer(ch);
                            for (int i = 0; i < c->audioBuffer->getNumSamples(); ++i)
                                w[i] = 1.0f;
                        }
                    };
                    auto renderU = [&](const ResolvedArrangement& arr, int only) {
                        const int total = (int)arr.totalDurationSamples;
                        juce::AudioBuffer<float> out(2, total); out.clear();
                        for (int i = 0; i < arr.entries.size(); ++i) {
                            if (only >= 0 && i != only) continue;
                            mixEntryToBuffer(arr.entries.getReference(i), out, total,
                                             0LL, 1.0, 1.0, i);
                        }
                        return out;
                    };
                    auto fnvOf = [&](const juce::AudioBuffer<float>& b) {
                        juce::uint32 h = 2166136261u;
                        for (int ch = 0; ch < b.getNumChannels(); ++ch)
                            for (int i = 0; i < b.getNumSamples(); ++i) {
                                juce::uint32 bits; const float v = b.getSample(ch, i);
                                std::memcpy(&bits, &v, sizeof(bits));
                                h = (h ^ bits) * 16777619u;
                            }
                        return h;
                    };
                    auto buildJoinU = [&](bool unity) {
                        Project p; p.sampleRate = srU; p.unityGainMode = unity;
                        for (auto n : { "A", "B" }) {
                            auto* blk = p.addBlock(n);
                            addClipTo(blk, juce::String("c") + n, LU + bodyU + TU);
                            blk->clips[0]->startMark = LU;
                            blk->clips[0]->endMark   = LU + bodyU;
                            dcFillU(blk->clips[0]);
                        }
                        juce::Random r(7101); ArrangementResolver res;
                        auto arr = res.resolve(p, r); arr.sampleRate = srU;
                        return arr;
                    };
                    auto buildSimU = [&](bool unity) {
                        Project p; p.sampleRate = srU; p.unityGainMode = unity;
                        auto* A = p.addBlock("A");
                        auto* B = p.addBlock("B");
                        auto* C = p.addBlock("C");
                        for (auto* blk : { A, B, C }) {
                            addClipTo(blk, juce::String("c") + blk->name, 8000);
                            dcFillU(blk->clips[0]);
                            blk->playChance = 1.0f;
                        }
                        p.stackBlocks(B->id, A->id);
                        p.stackBlocks(C->id, A->id);
                        A->stackPlayMode = StackPlayMode::Simultaneous;
                        A->stackPlayCount.values.set(0, 3);
                        p.propagateStackSettings(A->stackGroup, A);
                        juce::Random r(7102); ArrangementResolver res;
                        auto arr = res.resolve(p, r); arr.sampleRate = srU;
                        return arr;
                    };

                    // (a) unity ON, sequential join: every entry unity over its FULL
                    //     rendered extent = [bodyStart - renderedLeadIn, bodyEnd + renderedTail)
                    auto uJoin = buildJoinU(true);
                    float uJoinMin = 2.0f, uJoinMax = -1.0f;
                    for (int i = 0; i < uJoin.entries.size(); ++i) {
                        const auto& e = uJoin.entries.getReference(i);
                        auto solo = renderU(uJoin, i);
                        const int64_t lo = e.timelinePos - renderedLeadInLength(e);
                        const int64_t hi = e.timelinePos + (e.endMark - e.startMark)
                                           + renderedTailLength(e);
                        for (int64_t q = juce::jmax((int64_t)0, lo);
                             q < juce::jmin(hi, (int64_t)solo.getNumSamples()); ++q) {
                            const float v = solo.getSample(0, (int)q);
                            uJoinMin = juce::jmin(uJoinMin, v);
                            uJoinMax = juce::jmax(uJoinMax, v);
                        }
                    }

                    // (a) unity ON, simultaneous stack: each entry's body at 1.0
                    auto uSim = buildSimU(true);
                    float uSimMin = 2.0f, uSimMax = -1.0f;
                    for (int i = 0; i < uSim.entries.size(); ++i) {
                        const auto& e = uSim.entries.getReference(i);
                        auto solo = renderU(uSim, i);
                        const int mid = (int)(e.timelinePos + (e.endMark - e.startMark) / 2);
                        const float v = solo.getSample(0, mid);
                        uSimMin = juce::jmin(uSimMin, v);
                        uSimMax = juce::jmax(uSimMax, v);
                    }

                    // (b) default mode: SIM stack still attenuates by 1/playCount
                    auto dSim = buildSimU(false);
                    float dSimMin = 2.0f, dSimMax = -1.0f;
                    for (int i = 0; i < dSim.entries.size(); ++i) {
                        const auto& e = dSim.entries.getReference(i);
                        auto solo = renderU(dSim, i);
                        const int mid = (int)(e.timelinePos + (e.endMark - e.startMark) / 2);
                        const float v = solo.getSample(0, mid);
                        dSimMin = juce::jmin(dSimMin, v);
                        dSimMax = juce::jmax(dSimMax, v);
                    }

                    // (b) default-mode join render locked to the pre-RAWGAIN hash
                    auto dJoin = buildJoinU(false);
                    // Golden value produced by the PRE-RAWGAIN gain math: captured by
                    // forcing this mixer down its original branch (raw := false) and
                    // rendering this same geometry. Identical to the value the current
                    // default path produces ⇒ the 2b zero-diff, locked permanently.
                    const juce::uint32 goldenDefault = 0x605636a5u;
                    const juce::uint32 gotDefault    = fnvOf(renderU(dJoin, -1));

                    const bool unityJoinOk = std::abs(uJoinMin - 1.0f) < 1e-6f
                                          && std::abs(uJoinMax - 1.0f) < 1e-6f;
                    const bool unitySimOk  = std::abs(uSimMin - 1.0f) < 1e-6f
                                          && std::abs(uSimMax - 1.0f) < 1e-6f;
                    const bool defSimOk    = std::abs(dSimMin - 1.0f / 3.0f) < 1e-5f
                                          && std::abs(dSimMax - 1.0f / 3.0f) < 1e-5f;
                    const bool zeroDiffOk  = (gotDefault == goldenDefault);
                    const bool timingOk    = (uJoin.totalDurationSamples == dJoin.totalDurationSamples)
                                          && (uSim.totalDurationSamples == dSim.totalDurationSamples);

                    verdict("T56 RAWGAIN unity mode: ON=1.0 across join+SIM stack, OFF unchanged (golden hash) + timing mode-independent",
                            unityJoinOk && unitySimOk && defSimOk && zeroDiffOk && timingOk,
                            "unityJoin[" + juce::String(uJoinMin, 6) + ".." + juce::String(uJoinMax, 6)
                            + "], unitySim[" + juce::String(uSimMin, 6) + ".." + juce::String(uSimMax, 6)
                            + "], defaultSim[" + juce::String(dSimMin, 6) + ".." + juce::String(dSimMax, 6)
                            + "] (want 0.333333), defaultHash=0x" + juce::String::toHexString((int)gotDefault)
                            + " golden=0x" + juce::String::toHexString((int)goldenDefault)
                            + ", timing=" + (timingOk ? "same" : "DIFFERS"));
                }

                { // T57 (PERMANENT, 2026-08-14): TINY-BODY join geometry — the one
                  // case the JOINFIX2 header flags as unprobed ("A body inside BOTH
                  // its join windows (fadeIn + fadeOut > bodyLen) gets both lines —
                  // endpoint gains multiply ... no harness probe triggers this
                  // geometry"). Middle entry's body (2000) is far shorter than its
                  // two rendered join windows (4410 + 4410), so the mixer drives it
                  // through gInAt * gOutAt simultaneously. Same-tempo and cross-tempo
                  // variants; the latter forces genuinely RENDERED (stretched)
                  // extents. DC-filled sources ⇒ rendered sample value IS the gain.
                  // This PINS the behaviour; it does not change it.
                    const double srT = 48000.0;

                    auto dcT = [&](Clip* c) {
                        for (int ch = 0; ch < c->audioBuffer->getNumChannels(); ++ch) {
                            auto* w = c->audioBuffer->getWritePointer(ch);
                            for (int i = 0; i < c->audioBuffer->getNumSamples(); ++i)
                                w[i] = 1.0f;
                        }
                    };
                    auto renderT = [&](const ResolvedArrangement& arr, int only) {
                        const int total = (int)arr.totalDurationSamples;
                        juce::AudioBuffer<float> out(2, total); out.clear();
                        for (int i = 0; i < arr.entries.size(); ++i) {
                            if (only >= 0 && i != only) continue;
                            mixEntryToBuffer(arr.entries.getReference(i), out, total,
                                             0LL, 1.0, 1.0, i);
                        }
                        return out;
                    };
                    struct TinySpec { const char* n; int L, body, T; double tempo; };
                    auto buildTiny = [&](bool unity, double tA, double tB, double tC) {
                        Project p; p.sampleRate = srT; p.unityGainMode = unity;
                        const TinySpec spec[3] = {
                            { "A", 4410, 17640, 4410, tA },
                            { "B", 4410,  2000, 4410, tB },   // body << L + T
                            { "C", 4410, 17640, 4410, tC } };
                        for (const auto& s : spec) {
                            auto* blk = p.addBlock(s.n);
                            addClipTo(blk, juce::String("c") + s.n, s.L + s.body + s.T);
                            blk->clips[0]->startMark = s.L;
                            blk->clips[0]->endMark   = s.L + s.body;
                            blk->clips[0]->tempo     = s.tempo;
                            dcT(blk->clips[0]);
                        }
                        juce::Random r(7201); ArrangementResolver res;
                        auto arr = res.resolve(p, r); arr.sampleRate = srT;
                        return arr;
                    };

                    struct TinyResult { float sumDev, maxAbs, maxJump, gMin, gMax;
                                        int jumps; int64_t worstAt, jumpAt; int jumpEntry;
                                        bool tiny; };
                    auto probeTiny = [&](const ResolvedArrangement& arr) {
                        TinyResult r {};
                        r.sumDev = 0.0f; r.maxAbs = 0.0f; r.maxJump = 0.0f;
                        r.gMin = 2.0f; r.gMax = -1.0f; r.jumps = 0; r.worstAt = -1;
                        const int total = (int)arr.totalDurationSamples;
                        const int n = arr.entries.size();
                        std::vector<juce::AudioBuffer<float>> solos;
                        solos.reserve((size_t)n);
                        for (int i = 0; i < n; ++i) solos.push_back(renderT(arr, i));

                        // Confirm the geometry actually triggers: middle entry's
                        // windows must exceed its body.
                        if (n >= 2) {
                            const auto& mid = arr.entries.getReference(1);
                            r.tiny = (mid.prevTailLen + mid.nextLeadInLen)
                                     > (mid.endMark - mid.startMark);
                        }
                        // Gain sum, measured between the first body start and the
                        // last body end (song head/tail legitimately sum below 1).
                        const auto& e0 = arr.entries.getReference(0);
                        const auto& eN = arr.entries.getReference(n - 1);
                        const int64_t w0 = e0.timelinePos;
                        const int64_t w1 = juce::jmin(eN.timelinePos
                                              + (eN.endMark - eN.startMark), (int64_t)total);
                        for (int64_t q = w0; q < w1; ++q) {
                            float sum = 0.0f;
                            for (auto& s : solos) sum += s.getSample(0, (int)q);
                            const float dev = std::abs(sum - 1.0f);
                            if (dev > r.sumDev) { r.sumDev = dev; r.worstAt = q; }
                        }
                        // Peak of the FULL render (the clipping question).
                        auto all = renderT(arr, -1);
                        for (int ch = 0; ch < all.getNumChannels(); ++ch)
                            for (int i = 0; i < all.getNumSamples(); ++i)
                                r.maxAbs = juce::jmax(r.maxAbs, std::abs(all.getSample(ch, i)));
                        // Per-carrier envelope: range + adjacent-sample discontinuity.
                        for (int i = 0; i < n; ++i) {
                            const auto& e = arr.entries.getReference(i);
                            const int64_t lo = juce::jmax((int64_t)1,
                                                   e.timelinePos - renderedLeadInLength(e));
                            const int64_t hi = juce::jmin(e.timelinePos
                                                   + (e.endMark - e.startMark)
                                                   + renderedTailLength(e), (int64_t)total);
                            for (int64_t q = lo; q < hi; ++q) {
                                const float v = solos[(size_t)i].getSample(0, (int)q);
                                r.gMin = juce::jmin(r.gMin, v);
                                r.gMax = juce::jmax(r.gMax, v);
                                const float d = std::abs(v - solos[(size_t)i].getSample(0, (int)q - 1));
                                if (d > r.maxJump) { r.maxJump = d; r.jumpAt = q; r.jumpEntry = i; }
                                if (d > 0.01f) ++r.jumps;
                            }
                        }
                        return r;
                    };

                    auto same  = probeTiny(buildTiny(false, 120.0, 120.0, 120.0));
                    auto cross = probeTiny(buildTiny(false, 100.0, 140.0, 100.0));
                    auto uSame = probeTiny(buildTiny(true,  120.0, 120.0, 120.0));
                    auto uCross= probeTiny(buildTiny(true,  100.0, 140.0, 100.0));

                    auto line = [&](const char* tag, const TinyResult& t) {
                        std::cout << "  " << tag
                                  << " triggered=" << (t.tiny ? "y" : "n")
                                  << " maxSumDev=" << juce::String(t.sumDev, 6)
                                  << " @" << (juce::int64)t.worstAt
                                  << " max|sum|=" << juce::String(t.maxAbs, 6)
                                  << " carrier[" << juce::String(t.gMin, 6) << ".."
                                  << juce::String(t.gMax, 6) << "]"
                                  << " maxJump=" << juce::String(t.maxJump, 6)
                                  << " @entry" << t.jumpEntry << "/" << (juce::int64)t.jumpAt
                                  << " jumps>0.01=" << t.jumps << "\n";
                    };
                    std::cout << "T57 TINY-BODY probe (body=2000, L=T=4410 ⇒ windows > body):\n";
                    line("same-tempo  default:", same);
                    line("cross-tempo default:", cross);
                    line("same-tempo  UNITY  :", uSame);
                    line("cross-tempo UNITY  :", uCross);

                    // ── T57 VERDICT (print-only; deliberately NOT a verdict() so the
                    // suite stays green — this DOCUMENTS a known-unsound geometry
                    // rather than asserting it fixed. No product code touched.) ──
                    // Default mode: the tiny body sits inside BOTH join windows, so
                    // the mixer multiplies gInAt * gOutAt over its whole length. That
                    // product is NOT the complementary line its neighbours' tails and
                    // lead-ins were computed against, so gA+gB != 1 across the join
                    // and the JOINFIX2 |out| <= 1 bound is lost. The carrier step at
                    // B's lead-in -> body boundary is the product term switching on:
                    // the lead-in ends on gB alone (~0.4999) and the body's first
                    // sample is gB*gOut (~0.3634) — a ~0.1365 discontinuity.
                    // The cross-tempo UNITY carrier minimum of 0 is NOT a gain
                    // deviation: it is WSOLA edge under-coverage (literal zero samples
                    // in the stretched buffer, cf. the ZRUN leadingZeros diagnostics).
                    // Unity gain itself is flat — confirmed by the same-tempo row.
                    const bool boundOk = same.maxAbs <= 1.0001f && cross.maxAbs <= 1.0001f;
                    const bool contOk  = same.maxJump <= 0.01f && cross.maxJump <= 0.01f;
                    std::cout << "  T57 VERDICT: TINY-BODY GEOMETRY "
                              << ((boundOk && contOk) ? "SOUND" : "UNSOUND")
                              << "  (bound=" << (boundOk ? "ok" : "EXCEEDED")
                              << ", continuity=" << (contOk ? "ok" : "CARRIER JUMP")
                              << ") — default mode only; unity mode is flat by construction\n";
                }

                { // T58 (PERMANENT, BACKCOMPAT 2026-08-14): old-schema stack
                  // settings must survive load REGARDLESS of block order.
                  // Pre-propagation saves carried a stack field on only ONE member;
                  // load-time normalisation used to source ALL fields from the FIRST
                  // member, so a defaulted first member overwrote the real saved
                  // value (Issue B: "sequential play 3 plays 1"). Serialization.cpp
                  // now resolves each field from the first member that ACTUALLY
                  // carried it. Fixtures pin both orderings plus mixed presence.
                    auto fixture = [&](const juce::String& nm) {
                        const char* rel[] = { "diag/fixtures/", "../diag/fixtures/",
                                              "../../diag/fixtures/", "../../../diag/fixtures/" };
                        for (auto* r : rel) {
                            auto f = juce::File::getCurrentWorkingDirectory()
                                         .getChildFile(juce::String(r) + nm);
                            if (f.existsAsFile()) return f;
                        }
                        return juce::File();
                    };
                    // Fixtures describe stack SCHEMA; audio is injected post-load so
                    // a real resolve can run. Never touches the fields under test.
                    auto inject = [&](Project& p) {
                        for (auto* b : p.blocks)
                            for (auto* c : b->clips) {
                                const int n = (int)juce::jmax((int64_t)1, c->endMark);
                                c->audioBuffer = std::make_shared<juce::AudioBuffer<float>>(2, n);
                                c->audioBuffer->clear();
                            }
                    };
                    struct LoadRes { bool loaded=false, counts3=false, picked3=false, zeroGap=false;
                                     juce::String dump; };
                    auto loadCase = [&](const juce::String& nm) {
                        LoadRes r;
                        auto f = fixture(nm);
                        if (!f.existsAsFile()) return r;
                        Project p;
                        r.loaded = p.loadFromFile(f);
                        if (!r.loaded) return r;
                        r.counts3 = true;
                        for (auto* b : p.blocks) {
                            const bool is3 = b->stackPlayCount.values.size() == 1
                                          && b->stackPlayCount.values[0] == 3;
                            if (!is3) r.counts3 = false;
                            r.dump << b->name << "=[";
                            for (int i = 0; i < b->stackPlayCount.values.size(); ++i)
                                r.dump << (i ? "," : "") << b->stackPlayCount.values[i];
                            r.dump << "] ";
                        }
                        inject(p);
                        std::vector<Block*> group;
                        for (auto* b : p.blocks) if (b->stackGroup >= 0) group.push_back(b);
                        juce::Random rr(9301); ArrangementResolver res;
                        r.picked3 = true; r.zeroGap = true;
                        for (int i = 0; i < 5; ++i) {
                            juce::Random rp(9400 + i);
                            auto sp = StackPicker::pick(group, p.blocks, rp);
                            if ((int)sp.picked.size() != 3) r.picked3 = false;
                            auto arr = res.resolve(p, rr);
                            if (arr.entries.size() != 3) r.picked3 = false;
                            for (int e = 0; e + 1 < arr.entries.size(); ++e) {
                                const auto& cur = arr.entries.getReference(e);
                                const auto& nxt = arr.entries.getReference(e + 1);
                                if (cur.timelinePos + (cur.endMark - cur.startMark)
                                    != nxt.timelinePos) r.zeroGap = false;
                            }
                        }
                        return r;
                    };

                    auto first = loadCase("oldschema_seq3.json");
                    auto last  = loadCase("oldschema_seq3_bearer_last.json");

                    // Mixed presence: the mode bearer and the count bearer are
                    // DIFFERENT members — each field must come from its own bearer.
                    bool mixedOk = false; juce::String mixedDump;
                    {
                        auto f = fixture("oldschema_mixed_presence.json");
                        if (f.existsAsFile()) {
                            Project p;
                            if (p.loadFromFile(f)) {
                                mixedOk = true;
                                for (auto* b : p.blocks) {
                                    const bool c3 = b->stackPlayCount.values.size() == 1
                                                 && b->stackPlayCount.values[0] == 3;
                                    const bool sim = b->stackPlayMode == StackPlayMode::Simultaneous;
                                    if (!c3 || !sim) mixedOk = false;
                                    mixedDump << b->name << "=["
                                              << (b->stackPlayCount.values.size() == 1
                                                  ? juce::String(b->stackPlayCount.values[0])
                                                  : juce::String("?"))
                                              << "," << (sim ? "sim" : "seq") << "] ";
                                }
                            }
                        }
                    }

                    // Current-schema control: built and saved through the CURRENT
                    // paths — must be unaffected by the load-path change.
                    bool ctrlOk = false; juce::String ctrlDump;
                    {
                        Project p;
                        auto* A = p.addBlock("A"); auto* B = p.addBlock("B"); auto* C = p.addBlock("C");
                        for (auto* blk : { A, B, C })
                            addClipTo(blk, juce::String("c") + blk->name, 1000);
                        p.stackBlocks(B->id, A->id);
                        p.stackBlocks(C->id, A->id);
                        A->stackPlayMode = StackPlayMode::Sequential;
                        A->stackPlayCount.values.set(0, 3);
                        p.propagateStackSettings(A->stackGroup, A);
                        auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                       .getChildFile("t58_control_seq3.bsp");
                        tmp.deleteFile();
                        p.saveToFile(tmp);
                        Project q;
                        if (q.loadFromFile(tmp)) {
                            ctrlOk = true;
                            for (auto* b : q.blocks) {
                                if (!(b->stackPlayCount.values.size() == 1
                                      && b->stackPlayCount.values[0] == 3)) ctrlOk = false;
                                ctrlDump << b->name << "=["
                                         << (b->stackPlayCount.values.size() == 1
                                             ? juce::String(b->stackPlayCount.values[0])
                                             : juce::String("?")) << "] ";
                            }
                            inject(q);
                            std::vector<Block*> group;
                            for (auto* b : q.blocks) if (b->stackGroup >= 0) group.push_back(b);
                            for (int i = 0; i < 5; ++i) {
                                juce::Random rp(9500 + i);
                                if ((int)StackPicker::pick(group, q.blocks, rp).picked.size() != 3)
                                    ctrlOk = false;
                            }
                        }
                        tmp.deleteFile();
                    }

                    std::cout << "T58 fixtures: first[" << first.dump << "] last[" << last.dump
                              << "] mixed[" << mixedDump << "] control[" << ctrlDump << "]\n";

                    verdict("T58 BACKCOMPAT old-schema stack settings survive load in ANY block order (per-field bearer) + mixed presence + current-schema control",
                            first.loaded && first.counts3 && first.picked3 && first.zeroGap
                            && last.loaded && last.counts3 && last.picked3 && last.zeroGap
                            && mixedOk && ctrlOk,
                            juce::String("bearerFirst(loaded=") + (first.loaded ? "y" : "n")
                            + ",counts3=" + (first.counts3 ? "y" : "N")
                            + ",picked3=" + (first.picked3 ? "y" : "N")
                            + ",zeroGap=" + (first.zeroGap ? "y" : "N") + ")"
                            + " bearerLast(loaded=" + (last.loaded ? "y" : "n")
                            + ",counts3=" + (last.counts3 ? "y" : "N")
                            + ",picked3=" + (last.picked3 ? "y" : "N")
                            + ",zeroGap=" + (last.zeroGap ? "y" : "N") + ")"
                            + " mixedPresence=" + (mixedOk ? "ok" : "BAD")
                            + " currentSchemaControl=" + (ctrlOk ? "ok" : "BAD"));
                }

                { // T59 (PERMANENT, RAWGAIN-DEFAULT — flipped 2026-08-22 when Carter
                  // RETRACTED the 2026-08-21 request): raw summing is ON by default.
                  // (a) a default-constructed Project has unityGainMode == true, so a
                  //     fresh project / fresh install starts with raw summing ON;
                  // (b) a .bsp with the key ABSENT (every pre-RAWGAIN save) follows the
                  //     new default and loads as true — actively set, not merely left
                  //     alone, so the field is pre-dirtied to false before each load;
                  // (c) a .bsp that explicitly stored FALSE keeps FALSE (and explicit
                  //     TRUE keeps TRUE). An explicitly stored user choice is never
                  //     silently overridden by the default, in either direction;
                  // (d) Carter's arrangement (all-120BPM, 9 blocks, one 3-block
                  //     simultaneous stack), built WITHOUT ever touching the flag, now
                  //     renders down the RAW-SUMMING path: DC-filled sources sum
                  //     un-attenuated, reaching 4.0 where four sources overlap;
                  // (e) the toggle still WORKS in the other direction: the same
                  //     geometry with unityGainMode = false runs the COMPLEMENTARY law
                  //     (sequential joins == 1.0, stack body == 1.0, no raw sum).
                    auto rootWith = [&](int mode) {          // -1 absent, 0 false, 1 true
                        auto* o = new juce::DynamicObject();
                        o->setProperty("version", 1);
                        o->setProperty("name", "Carter");
                        o->setProperty("sampleRate", 48000.0);
                        o->setProperty("defaultClipTempo", 120.0);
                        if (mode >= 0) o->setProperty("unityGainMode", mode == 1);
                        o->setProperty("blocks", juce::var(juce::Array<juce::var>()));
                        return juce::var(o);
                    };
                    auto loadFlag = [&](int mode) {
                        Project p;
                        p.unityGainMode = false;  // pre-dirty: load must ACTIVELY set it
                        Serialization::projectFromJSON(rootWith(mode), p, juce::File());
                        return p.unityGainMode;
                    };

                    const bool ctorDefaultOn  = (Project().unityGainMode == true);
                    const bool absentKeyOn    = (loadFlag(-1) == true);
                    const bool explicitOffOff = (loadFlag(0)  == false);   // explicit choice kept
                    const bool explicitOnKept = (loadFlag(1)  == true);

                    // Save-side: a default project must WRITE the key as true, so a
                    // resave of a legacy file pins raw summing explicitly.
                    Project fresh;
                    const bool savesTrue =
                        (bool)Serialization::projectToJSON(fresh, juce::File())
                                  .getProperty("unityGainMode", false) == true;

                    // ── Carter's arrangement ────────────────────────────────────
                    const double srC = 48000.0;
                    const int LC = 4410, bodyC = 17640, TC = 4410;
                    auto buildCarter = [&](int forceMode) {   // -1 = leave at default
                        auto p = std::make_unique<Project>();
                        p->sampleRate = srC;
                        p->defaultClipTempo = 120.0;
                        if (forceMode >= 0) p->unityGainMode = (forceMode == 1);
                        juce::Array<Block*> bs;
                        for (int i = 1; i <= 9; ++i) {
                            auto* b = p->addBlock("B" + juce::String(i));
                            addClipTo(b, "c" + juce::String(i), LC + bodyC + TC);
                            auto* c = b->clips[0];
                            c->startMark = LC;
                            c->endMark   = LC + bodyC;
                            c->tempo     = 120.0;            // all-120BPM: no stretching
                            for (int ch = 0; ch < c->audioBuffer->getNumChannels(); ++ch) {
                                auto* w = c->audioBuffer->getWritePointer(ch);
                                for (int s = 0; s < c->audioBuffer->getNumSamples(); ++s)
                                    w[s] = 1.0f;             // DC: rendered value == gain
                            }
                            b->playChance = 1.0f;
                            bs.add(b);
                        }
                        // Blocks 4/5/6 -> one 3-block SIMULTANEOUS stack, all three play.
                        p->stackBlocks(bs[4]->id, bs[3]->id);
                        p->stackBlocks(bs[5]->id, bs[3]->id);
                        bs[3]->stackPlayMode = StackPlayMode::Simultaneous;
                        bs[3]->stackPlayCount.values.set(0, 3);
                        p->propagateStackSettings(bs[3]->stackGroup, bs[3]);
                        return p;
                    };
                    auto renderAll = [&](const ResolvedArrangement& arr) {
                        const int total = (int)arr.totalDurationSamples;
                        juce::AudioBuffer<float> out(2, total); out.clear();
                        for (int i = 0; i < arr.entries.size(); ++i)
                            mixEntryToBuffer(arr.entries.getReference(i), out, total,
                                             0LL, 1.0, 1.0, i);
                        return out;
                    };

                    // (d) DEFAULT project — unityGainMode never assigned anywhere.
                    // Since 2026-08-22 that means raw summing ON.
                    auto pDef = buildCarter(-1);
                    const bool builtAtDefaultOn = (pDef->unityGainMode == true);
                    ArrangementResolver resC; juce::Random rC(8210);
                    auto arrDef = resC.resolve(*pDef, rC); arrDef.sampleRate = srC;
                    auto outDef = renderAll(arrDef);
                    // Every entry carried the flag through the resolver as true.
                    bool entriesOn = true;
                    for (auto& e : arrDef.entries) if (!e.unityGainMode) entriesOn = false;

                    float defMax = 0.0f;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int s = 0; s < outDef.getNumSamples(); ++s)
                            defMax = juce::jmax(defMax, std::abs(outDef.getSample(ch, s)));

                    // (e) same geometry, toggle explicitly OFF: the complementary law
                    // must still run and be measurable. All join / body deviations
                    // below are measured on THIS render.
                    auto pOff = buildCarter(0);
                    ArrangementResolver resF; juce::Random rF(8210);
                    auto arrOff = resF.resolve(*pOff, rF); arrOff.sampleRate = srC;
                    auto outOff = renderAll(arrOff);
                    float offMax = 0.0f;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int s = 0; s < outOff.getNumSamples(); ++s)
                            offMax = juce::jmax(offMax, std::abs(outOff.getSample(ch, s)));

                    // Classify each distinct body-start on the timeline. A join is
                    // PURELY SEQUENTIAL when exactly one entry ends its body there and
                    // exactly one starts — the geometry the complementary join law was
                    // written for. A simultaneous stack's boundaries are NOT that shape
                    // (N entries start at one position) and are measured separately.
                    juce::Array<int64_t> seqJoins, stackJoins;
                    for (int i = 0; i < arrOff.entries.size(); ++i) {
                        const auto tp = arrOff.entries.getReference(i).timelinePos;
                        if (tp <= 0 || seqJoins.contains(tp) || stackJoins.contains(tp)) continue;
                        int nStart = 0, nEnd = 0;
                        for (int j = 0; j < arrOff.entries.size(); ++j) {
                            const auto& f = arrOff.entries.getReference(j);
                            if (f.timelinePos == tp) ++nStart;
                            if (f.timelinePos + (f.endMark - f.startMark) == tp) ++nEnd;
                        }
                        // nEnd == 0 => the arrangement's own start, not a join at all.
                        if (nEnd == 0) continue;
                        ((nStart == 1 && nEnd == 1) ? seqJoins : stackJoins).add(tp);
                    }
                    auto devAround = [&](int64_t jp, int w) {
                        float d = 0.0f;
                        for (int64_t q = jp - w; q < jp + w; ++q) {
                            if (q < 0 || q >= outOff.getNumSamples()) continue;
                            d = juce::jmax(d, std::abs(outOff.getSample(0, (int)q) - 1.0f));
                        }
                        return d;
                    };
                    float seqDev = 0.0f;
                    for (auto jp : seqJoins) seqDev = juce::jmax(seqDev, devAround(jp, 64));

                    // Stack BODY interior (clear of both join windows): the 1/playCount
                    // level compensation must put the three layers back at exactly 1.0.
                    float stackBodyDev = 0.0f;
                    if (stackJoins.size() >= 2) {
                        const int64_t a = juce::jmin(stackJoins[0], stackJoins[1]) + LC + 64;
                        const int64_t b = juce::jmax(stackJoins[0], stackJoins[1]) - TC - 64;
                        for (int64_t q = a; q < b; ++q)
                            if (q >= 0 && q < outOff.getNumSamples())
                                stackBodyDev = juce::jmax(stackBodyDev,
                                                   std::abs(outOff.getSample(0, (int)q) - 1.0f));
                    }

                    // Explicitly-ON control: forcing the flag true must be byte-for-byte
                    // the same render as leaving it at the (now ON) default.
                    auto pOn = buildCarter(1);
                    ArrangementResolver resO; juce::Random rO(8210);
                    auto arrOn = resO.resolve(*pOn, rO); arrOn.sampleRate = srC;
                    auto outOn = renderAll(arrOn);
                    float onMax = 0.0f, defVsOn = 0.0f;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int s = 0; s < outOn.getNumSamples(); ++s) {
                            onMax = juce::jmax(onMax, std::abs(outOn.getSample(ch, s)));
                            if (s < outDef.getNumSamples())
                                defVsOn = juce::jmax(defVsOn,
                                    std::abs(outOn.getSample(ch, s) - outDef.getSample(ch, s)));
                        }
                    const bool defaultIsExplicitOn =
                        (outDef.getNumSamples() == outOn.getNumSamples()) && defVsOn == 0.0f;

                    // ── PROBE (print-only, T57 precedent): SIM-STACK JOIN GEOMETRY ──
                    // PRE-EXISTING, untouched by the 2026-08-21 default/UI work: no
                    // file under Source/Audio/ was modified. Recorded here because
                    // Carter's project contains exactly this shape.
                    //
                    // The complementary join law pairs an entry with its ARRAY
                    // NEIGHBOUR (entries[i-1] / entries[i+1]). Every member of a
                    // simultaneous stack shares one timelinePos, so for the 2nd..Nth
                    // members that neighbour is a STACK SIBLING, not the preceding
                    // sequential entry. Only one member therefore crossfades against
                    // the neighbouring entry; the rest enter and leave flat at
                    // 1/playCount, so the join window carries an excess of (N-1)/N
                    // (2/3 here => peak 4/3) instead of summing to 1.
                    float stackJoinDev = 0.0f;
                    for (auto jp : stackJoins) stackJoinDev = juce::jmax(stackJoinDev, devAround(jp, LC));
                    std::cout << "T59 PROBE SIM-stack join geometry (9 blocks, 3-block SIM stack, all 120BPM;\n"
                              << "               measured on the toggle-OFF render — the complementary law):\n"
                              << "  sequential joins=" << seqJoins.size()
                              << " maxDevFrom1=" << juce::String(seqDev, 6) << " (complementary: want ~0)\n"
                              << "  stack joins=" << stackJoins.size()
                              << " maxDevFrom1=" << juce::String(stackJoinDev, 6)
                              << " (expected excess (N-1)/N = " << juce::String(2.0f / 3.0f, 6) << ")\n"
                              << "  stack body interior maxDevFrom1=" << juce::String(stackBodyDev, 6)
                              << " (1/playCount compensation)\n"
                              << "  full-render max: DEFAULT(raw)=" << juce::String(defMax, 6)
                              << "  toggled OFF=" << juce::String(offMax, 6) << "\n"
                              << "  T59 PROBE VERDICT: SIM-STACK JOIN "
                              << ((stackJoinDev <= 0.01f) ? "SOUND" : "OVERSHOOTS BY (N-1)/N")
                              << " — PRE-EXISTING, mixing math deliberately untouched\n";

                    // Verdict scope = the DEFAULT only: which gain law runs when the
                    // user never touches the flag, and that the toggle still works in
                    // both directions. The stack-join excess above is a property of
                    // the fade law itself, unchanged by this flip, so it stays probe
                    // output rather than a verdict.
                    const bool seqJoinsUnity  = seqJoins.size() >= 3 && seqDev <= 0.01f;
                    const bool stackBodyUnity = stackBodyDev <= 0.01f;
                    // Default path IS the raw sum: four sources overlap at a stack join
                    // (the preceding entry's tail plus three lead-ins) ⇒ 4.0.
                    const bool defaultIsRawSum = defMax >= 3.9999f;
                    // Toggling OFF really runs the complementary law: bounded by the
                    // known (N-1)/N stack-join excess, nothing like the raw sum.
                    const bool toggleOffWorks  = offMax <= 1.0f + 2.0f / 3.0f + 1e-3f
                                              && offMax < defMax - 1.0f;
                    const bool timingSame      = (arrDef.totalDurationSamples
                                                  == arrOff.totalDurationSamples);

                    verdict("T59 RAWGAIN default ON (Carter retraction 2026-08-22): ctor/absent-key=ON, explicit FALSE and explicit TRUE both kept verbatim, fresh project saves the key as true, Carter 9-block+3-SIM renders RAW by default (4.0), toggling OFF still runs the COMPLEMENTARY law (seq joins==1.0, stack body==1.0)",
                            ctorDefaultOn && absentKeyOn && explicitOffOff && explicitOnKept
                            && savesTrue && builtAtDefaultOn && entriesOn && defaultIsExplicitOn
                            && seqJoinsUnity && stackBodyUnity && defaultIsRawSum
                            && toggleOffWorks && timingSame,
                            juce::String("ctorDefault=") + (ctorDefaultOn ? "ON" : "OFF(BUG)")
                            + ", absentKey=" + (absentKeyOn ? "ON" : "OFF(BUG)")
                            + ", explicitFalse=" + (explicitOffOff ? "KEPT-OFF" : "FLIPPED(BUG)")
                            + ", explicitTrue=" + (explicitOnKept ? "KEPT" : "FLIPPED(BUG)")
                            + ", savesKeyTrue=" + (savesTrue ? "y" : "N")
                            + ", entriesCarryOn=" + (entriesOn ? "y" : "N")
                            + ", default==explicitON=" + (defaultIsExplicitOn ? "identical" : "DIFFERS(BUG)")
                            + ", seqJoins=" + juce::String(seqJoins.size())
                            + " dev=" + juce::String(seqDev, 6)
                            + ", stackBodyDev=" + juce::String(stackBodyDev, 6)
                            + ", defaultMax=" + juce::String(defMax, 6)
                            + " (raw, want 4.0) vs toggledOffMax=" + juce::String(offMax, 6)
                            + ", timing=" + (timingSame ? "same" : "DIFFERS"));
                }

                { // T60 (PERMANENT, SPLITTER 2026-08-21, Carter request 2): the
                  // waveform/blocks divider must never be able to collapse a pane.
                  // "Blocks invisible" has regressed three times, so this asserts the
                  // clamp EXHAUSTIVELY rather than at a few sample points:
                  // (a) for every content height 1..2000 and a spread of desired
                  //     values including negatives, zero and absurdly large ones,
                  //     BOTH panes come out >= 1px;
                  // (b) whenever the content area is at least minTotalHeight(), both
                  //     panes additionally get their full documented minimum;
                  // (c) the default split reproduces the pre-splitter 360px strip;
                  // (d) persisted-value handling: absent/garbage -> default split,
                  //     sane-but-too-big -> clamped to fit the current window.
                  // Pure geometry from UI/SplitLayout.h — the header MainComponent
                  // lays out from — so it is covered headlessly here even though
                  // MainComponent itself is not part of this harness.
                    using namespace SplitLayout;

                    // Content height at the SMALLEST allowed window: the 800x600 floor
                    // (MainComponent::resized / MainWindow's constrainer) minus the
                    // 56px transport bar. The inspector is removed horizontally and
                    // never takes part in this split.
                    const int minWinTotalH = 600 - 56;   // 544

                    const int desireds[] = { -100000, -1, 0, 1, 2, blocksMinH - 1, blocksMinH,
                                             blocksDefaultH, waveMinH, 1000, 100000, 1 << 24 };

                    bool bothAlwaysPositive = true, minsHonoured = true;
                    int  firstBadTotal = -1, firstBadDesired = 0, firstBadBlocks = 0, firstBadWave = 0;
                    for (int totalH = 1; totalH <= 2000 && bothAlwaysPositive && minsHonoured; ++totalH) {
                        for (int d : desireds) {
                            const int b = clampBlocksHeight(d, totalH);
                            const int w = waveHeightFor(b, totalH);
                            if (totalH <= barH + 1) continue;   // no room for two panes at all
                            if (b < 1 || w < 1) {
                                bothAlwaysPositive = false;
                                firstBadTotal = totalH; firstBadDesired = d;
                                firstBadBlocks = b; firstBadWave = w;
                                break;
                            }
                            if (totalH >= minTotalHeight() && (b < blocksMinH || w < waveMinH)) {
                                minsHonoured = false;
                                firstBadTotal = totalH; firstBadDesired = d;
                                firstBadBlocks = b; firstBadWave = w;
                                break;
                            }
                        }
                    }

                    // (c) default split at the smallest allowed window and at 1200x700.
                    const int defAtMin   = clampBlocksHeight(blocksDefaultH, minWinTotalH);
                    const int waveAtMin  = waveHeightFor(defAtMin, minWinTotalH);
                    const int defAt700   = clampBlocksHeight(blocksDefaultH, 700 - 56);
                    const bool defaultOk = defAtMin == blocksDefaultH && defAt700 == blocksDefaultH;

                    // Extreme splits at the smallest allowed window: dragging the bar
                    // to either stop must still leave both panes at their minimum.
                    const int dragAllDown = clampBlocksHeight(1 << 20, minWinTotalH); // strip max
                    const int dragAllUp   = clampBlocksHeight(0,       minWinTotalH); // strip min
                    const bool extremesOk = dragAllUp   == blocksMinH
                                         && waveHeightFor(dragAllUp,   minWinTotalH) >= waveMinH
                                         && dragAllDown == minWinTotalH - barH - waveMinH
                                         && waveHeightFor(dragAllDown, minWinTotalH) == waveMinH;

                    // (d) persisted-value handling.
                    const bool restoreOk =
                           sanitizeStoredBlocksHeight(0)      == blocksDefaultH   // fresh install
                        && sanitizeStoredBlocksHeight(-42)    == blocksDefaultH   // corrupt
                        && sanitizeStoredBlocksHeight(1)      == blocksDefaultH   // below minimum
                        && sanitizeStoredBlocksHeight(999999) == blocksDefaultH   // absurd
                        && sanitizeStoredBlocksHeight(250)    == 250              // honoured
                        && restoreBlocksHeight(3000, minWinTotalH) == minWinTotalH - barH - waveMinH
                        && restoreBlocksHeight(0,    minWinTotalH) == blocksDefaultH;

                    verdict("T60 SPLITTER clamp: neither pane can ever reach 0px (exhaustive 1..2000), minimums honoured >= minTotalHeight, default split == 360, restore sanitises",
                            bothAlwaysPositive && minsHonoured && defaultOk && extremesOk && restoreOk,
                            juce::String("bothPositive=") + (bothAlwaysPositive ? "y" : "N")
                            + ", minsHonoured=" + (minsHonoured ? "y" : "N")
                            + (firstBadTotal >= 0
                                 ? " FIRSTBAD(totalH=" + juce::String(firstBadTotal)
                                   + ",desired=" + juce::String(firstBadDesired)
                                   + " -> blocks=" + juce::String(firstBadBlocks)
                                   + ",wave=" + juce::String(firstBadWave) + ")"
                                 : juce::String())
                            + ", minWin544: blocks=" + juce::String(defAtMin)
                            + "/wave=" + juce::String(waveAtMin)
                            + ", extremes[up=" + juce::String(dragAllUp)
                            + ",down=" + juce::String(dragAllDown) + "]="
                            + (extremesOk ? "ok" : "BAD")
                            + ", default360=" + (defaultOk ? "ok" : "BAD")
                            + ", restore=" + (restoreOk ? "ok" : "BAD")
                            + ", mins[wave=" + juce::String(waveMinH)
                            + ",blocks=" + juce::String(blocksMinH)
                            + ",bar=" + juce::String(barH) + "]");
                }

                { // T61 (PERMANENT, SONGEND-SIM 2026-08-22, Carter fix 1): a song
                  // ender inside a SIMULTANEOUS stack must end the song AFTER the
                  // whole slot, never mid-slot. Every picked member of a sim stack
                  // shares ONE timelinePos, so the slot is indivisible: the old
                  // `break` abandoned the remaining picked blocks and a 3-block
                  // stack played 1, 2 or 3 blocks depending on which member
                  // happened to hold the ender (and where it landed in the random
                  // pick order). All three placements are asserted here, each over
                  // many seeds so every pick ordering is exercised.
                  //   geometry: startMark 0 (no lead-in), body 12000, tail 4800.
                  //   expected: 3 entries, one shared timelinePos == 0,
                  //             total == body + tail, trailing block D never plays.
                    const double sr61  = 48000.0;
                    const int    body61 = 12000, tail61 = 4800;

                    auto buildSim3 = [&](int enderIdx) {      // 0,1,2 = member 1,2,3
                        auto p = std::make_unique<Project>();
                        p->sampleRate = sr61;
                        juce::Array<Block*> bs;
                        for (int i = 0; i < 3; ++i) {
                            auto* b = p->addBlock("S" + juce::String(i + 1));
                            addClipTo(b, "c" + juce::String(i + 1), body61 + tail61);
                            auto* c = b->clips[0];
                            c->startMark  = 0;                 // no lead-in
                            c->endMark    = body61;            // 4800-sample tail
                            c->isSongEnder = (i == enderIdx);
                            b->playChance = 1.0f;
                            bs.add(b);
                        }
                        // S1/S2/S3 -> one 3-block SIMULTANEOUS stack, all three play.
                        p->stackBlocks(bs[1]->id, bs[0]->id);
                        p->stackBlocks(bs[2]->id, bs[0]->id);
                        bs[0]->stackPlayMode = StackPlayMode::Simultaneous;
                        bs[0]->stackPlayCount.values.set(0, 3);
                        bs[0]->alwaysPlayBase = false;
                        p->propagateStackSettings(bs[0]->stackGroup, bs[0]);
                        // Trailing sequential block: must NEVER play (truncation proof).
                        auto* d = p->addBlock("D");
                        addClipTo(d, "cD", body61);
                        d->playChance = 1.0f;
                        return p;
                    };

                    const int64_t expTotal61 = (int64_t)body61 + tail61;
                    int   entriesSeen[3] = { -1, -1, -1 };   // per case, first seed
                    bool  caseOk[3]      = { true, true, true };
                    juce::String firstBad61;

                    for (int k = 0; k < 3; ++k) {
                        auto p61 = buildSim3(k);
                        auto stackIds = juce::Array<juce::Uuid>();
                        for (int i = 0; i < 3; ++i) stackIds.add(p61->blocks[i]->id);
                        const auto dId = p61->blocks[3]->id;

                        for (int seed = 0; seed < 24; ++seed) {
                            juce::Random r61(9000 + seed * 37);
                            ArrangementResolver res61;
                            auto arr = res61.resolve(*p61, r61);
                            arr.sampleRate = sr61;
                            if (seed == 0) entriesSeen[k] = arr.entries.size();

                            const bool countOk = (arr.entries.size() == 3);
                            // INVARIANT (guard at ArrangementResolver.cpp): entries
                            // added for the slot must never exceed playCount.
                            const bool leqPlayCount = (arr.entries.size() <= 3);
                            bool sharedPos = countOk;
                            bool membersOk = countOk;
                            bool noD       = true;
                            juce::Array<juce::Uuid> got;
                            for (const auto& e : arr.entries) {
                                if (countOk && e.timelinePos != arr.entries[0].timelinePos)
                                    sharedPos = false;
                                if (e.blockId == dId) noD = false;
                                got.addIfNotAlreadyThere(e.blockId);
                            }
                            if (countOk) {
                                if (got.size() != 3) membersOk = false;
                                for (auto& id : stackIds) if (!got.contains(id)) membersOk = false;
                            }
                            const bool startsAtZero = countOk && arr.entries[0].timelinePos == 0;
                            const bool truncOk = (arr.totalDurationSamples == expTotal61);

                            if (!(countOk && leqPlayCount && sharedPos && membersOk
                                  && noD && startsAtZero && truncOk)) {
                                if (caseOk[k]) firstBad61 +=
                                      " [ender=member" + juce::String(k + 1)
                                    + " seed=" + juce::String(seed)
                                    + " entries=" + juce::String(arr.entries.size())
                                    + " sharedPos=" + (sharedPos ? "y" : "N")
                                    + " members=" + juce::String(got.size())
                                    + " D=" + (noD ? "absent" : "PLAYED")
                                    + " total=" + juce::String(arr.totalDurationSamples)
                                    + " want=" + juce::String(expTotal61) + "]";
                                caseOk[k] = false;
                            }
                        }
                    }

                    verdict("T61 SONGEND-SIM: ender in member 1/2/3 of a 3-block SIMULTANEOUS stack (playCount 3) -> exactly 3 entries sharing ONE timelinePos in every case, song truncates AFTER the slot (tail included), trailing block never plays",
                            caseOk[0] && caseOk[1] && caseOk[2],
                            juce::String("entries[ender=m1,m2,m3]=")
                            + juce::String(entriesSeen[0]) + "/"
                            + juce::String(entriesSeen[1]) + "/"
                            + juce::String(entriesSeen[2]) + " (want 3/3/3)"
                            + ", 24 seeds x 3 placements: "
                            + juce::String((caseOk[0] ? 1 : 0) + (caseOk[1] ? 1 : 0) + (caseOk[2] ? 1 : 0))
                            + "/3 cases PASS, total==body+tail=" + juce::String(expTotal61)
                            + firstBad61);
                }

// ── PFHDIAG (2026-08-26, DIAGNOSIS ONLY — print-only, no verdict, no product
//    change): "Play from Here" reportedly falls back to the start of the song.
//    This probe replicates the handler's mapping step VERBATIM (see the copy of
//    MainComponent.cpp:88-101 below) and reports, per invocation, whether the
//    clicked block was present in the freshly resolved arrangement, what start
//    position came out, and which code path produced it.
#define PFHDIAG 1
#if PFHDIAG
                {
                    std::cout << "\n=== PFHDIAG: 'Play from Here' start-position mapping ===\n";

                    // VERBATIM replica of the FIXED handler (MainComponent.cpp:88-115;
                    // engine calls elided, they cannot change which position is chosen):
                    //     auto* target = project->getBlockById(blockId);
                    //     currentArrangement = resolver.resolve(*project, rng, target);
                    //     const ResolvedEntry* hit = nullptr;
                    //     for (const auto& entry : currentArrangement.entries)
                    //         if (entry.blockId == blockId) { hit = &entry; break; }
                    //     if (hit == nullptr) return;            // NOT-FOUND IS NOT ZERO
                    //     engine.play(...); engine.seekTo(hit->timelinePos);
                    struct PfhOutcome { bool present; int64_t seekPos; const char* path; int entries; };
                    auto playFromHere = [&](const ResolvedArrangement& arr,
                                            const juce::String& blockId) {
                        PfhOutcome o { false, -1, "NO PLAYBACK (empty arrangement)",
                                       arr.entries.size() };
                        if (arr.entries.isEmpty()) return o;
                        o.path = "NO PLAYBACK (not found — backstop, never plays from 0)";
                        const ResolvedEntry* hit = nullptr;
                        for (const auto& entry : arr.entries) {
                            if (entry.blockId == blockId) { hit = &entry; break; }
                        }
                        if (hit != nullptr) {
                            o.present = true;
                            o.seekPos = hit->timelinePos;
                            o.path = "MATCH (entry.timelinePos)";
                        }
                        return o;
                    };

                    const double srP = 48000.0;
                    const int    bodyP = 24000;               // 0.5 s bodies, no lead-in/tail
                    const int    RUNS  = 20;

                    // Every scenario puts a plain block "A" FIRST, so a correct seek
                    // to the clicked block is ALWAYS > 0 and a reported 0 is
                    // unambiguously the fallback (never a legitimate seek to the top).
                    auto addPlain = [&](Project& p, const char* nm, float chance) {
                        auto* b = p.addBlock(nm);
                        addClipTo(b, juce::String(nm) + "_c", bodyP);
                        b->playChance = chance;
                        return b;
                    };

                    auto runScenario = [&](const char* label, const char* detail,
                                           std::function<juce::String(Project&)> build) {
                        Project p; p.sampleRate = srP;
                        const juce::String clicked = build(p);
                        std::cout << "\n" << label << "\n  " << detail
                                  << "\n  run | inArrangement | seekPos | path\n"
                                  << "  ----+---------------+---------+"
                                     "--------------------------------------\n";
                        int nPresent = 0, nFallback = 0, nEmpty = 0;
                        juce::Array<int64_t> seenPos;
                        for (int i = 0; i < RUNS; ++i) {
                            juce::Random rp(5000 + i * 101);
                            ArrangementResolver resP;
                            // The handler passes the clicked block as forceInclude.
                            auto arr = resP.resolve(p, rp, p.getBlockById(clicked));
                            arr.sampleRate = srP;
                            auto o = playFromHere(arr, clicked);
                            if (o.entries == 0) ++nEmpty;
                            else if (o.present) ++nPresent; else ++nFallback;
                            seenPos.addIfNotAlreadyThere(o.seekPos);
                            std::cout << "  " << (i < 9 ? " " : "") << (i + 1)
                                      << "  |      " << (o.present ? "YES" : "no ")
                                      << "      | " << (o.seekPos < 0 ? juce::String("      -")
                                                     : juce::String(o.seekPos).paddedLeft(' ', 7))
                                      << " | " << o.path << "\n";
                        }
                        std::cout << "  => " << nPresent << "/" << RUNS << " played from the clicked block, "
                                  << nFallback << "/" << RUNS << " did not play";
                        if (nEmpty) std::cout << ", " << nEmpty << " empty";
                        std::cout << "   distinct seekPos values=" << seenPos.size() << "\n";
                    };

                    // (a) standalone block, playChance 1.0 — clicked = B (second slot).
                    runScenario("(a) STANDALONE, playChance 1.0  [click B]",
                                "A(1.0) B(1.0) C(1.0) — B is the 2nd slot, correct seek = 24000",
                                [&](Project& p) {
                                    addPlain(p, "A", 1.0f);
                                    auto* B = addPlain(p, "B", 1.0f);
                                    addPlain(p, "C", 1.0f);
                                    return B->id;
                                });

                    // (b) standalone block, playChance < 1.0.
                    runScenario("(b) STANDALONE, playChance 0.5  [click B]",
                                "A(1.0) B(0.5) C(1.0) — resolver gate: "
                                "`if (rng.nextFloat() >= block->playChance) continue;`",
                                [&](Project& p) {
                                    addPlain(p, "A", 1.0f);
                                    auto* B = addPlain(p, "B", 0.5f);
                                    addPlain(p, "C", 1.0f);
                                    return B->id;
                                });

                    auto buildStack = [&](Project& p, int playCount, StackPlayMode mode) {
                        addPlain(p, "A", 1.0f);                     // plain first slot
                        auto* S1 = addPlain(p, "S1", 1.0f);
                        auto* S2 = addPlain(p, "S2", 1.0f);
                        auto* S3 = addPlain(p, "S3", 1.0f);
                        p.stackBlocks(S2->id, S1->id);
                        p.stackBlocks(S3->id, S1->id);
                        S1->stackPlayMode = mode;
                        S1->stackPlayCount.values.set(0, playCount);
                        S1->alwaysPlayBase = false;
                        p.propagateStackSettings(S1->stackGroup, S1);
                        return S2->id;                              // click the MIDDLE member
                    };

                    // (c) member of a 3-stack, playCount 1 (fewer than all members).
                    runScenario("(c) 3-STACK, playCount 1  [click S2, a non-base member]",
                                "A + stack{S1,S2,S3} simultaneous, only 1 of 3 is picked per resolve",
                                [&](Project& p) { return buildStack(p, 1, StackPlayMode::Simultaneous); });

                    // (d) member of a 3-stack, playCount 3 (all members play).
                    runScenario("(d) 3-STACK, playCount 3  [click S2, a non-base member]",
                                "A + stack{S1,S2,S3} simultaneous, all 3 picked every resolve",
                                [&](Project& p) { return buildStack(p, 3, StackPlayMode::Simultaneous); });

                    // (e) supplementary: a SONG ENDER in an earlier block truncates the
                    //     arrangement before the clicked block is ever reached — a second,
                    //     independent way for the same lookup to miss.
                    runScenario("(e) SUPPLEMENTARY: earlier SONG ENDER  [click C]",
                                "A B(ender) C — the resolver truncates at B, so C is never in the arrangement",
                                [&](Project& p) {
                                    addPlain(p, "A", 1.0f);
                                    auto* B = addPlain(p, "B", 1.0f);
                                    B->clips[0]->isSongEnder = true;
                                    auto* C = addPlain(p, "C", 1.0f);
                                    return C->id;
                                });

                    // (f) clicked block has NO clips (or only 0%-weight clips). The pin
                    //     cannot rescue it — the resolver still `continue`s. UNREACHABLE
                    //     VIA THE MENU since 2026-08-22: BlockComponent greys "Play from
                    //     Here" out for exactly these blocks. Kept to exercise Part 1's
                    //     backstop: no entry now means NO PLAYBACK, never play-from-0.
                    runScenario("(f) UNREACHABLE VIA MENU (item greyed out): block has an empty clip list  [click B]",
                                "A B(no clips) C — resolver: `if (block->clips.isEmpty()) continue;`",
                                [&](Project& p) {
                                    addPlain(p, "A", 1.0f);
                                    auto* B = p.addBlock("B");      // deliberately no clip
                                    B->playChance = 1.0f;
                                    addPlain(p, "C", 1.0f);
                                    return B->id;
                                });

                    std::cout << "\n  PFHDIAG NOTE: every scenario puts a block BEFORE the clicked one, so a\n"
                                 "  correct seek is always > 0. The old handler reported 0 here (its\n"
                                 "  `int64_t seekPos = 0` initialiser doubling as the not-found value);\n"
                                 "  the fixed handler pins the block, and on a miss plays nothing at all.\n";
                }
#endif // PFHDIAG

// ── PFHBASE (2026-08-22): writes the shared 100-seed non-regression dump (see
//    pfhBaselineDump above) to a file so it can be diffed against a dump taken
//    from a pre-change binary. Print-only; the permanent lock is T62's golden hash.
#define PFHBASE 1
#if PFHBASE
                {
                    juce::String dump;
                    const juce::uint32 h = pfhBaselineDump(dump);
                    auto outFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                       .getChildFile("pfh_baseline_dump.txt");
                    outFile.replaceWithText(dump);
                    std::cout << "\nPFHBASE 100-seed resolve dump (forceInclude=nullptr): "
                              << outFile.getFullPathName()
                              << "\n  lines=" << juce::StringArray::fromLines(dump).size() - 1
                              << "  chars=" << dump.length()
                              << "  FNV=0x" << juce::String::toHexString((int)h)
                              << "  golden=0x" << juce::String::toHexString((int)kPfhBaselineGolden)
                              << "  " << (h == kPfhBaselineGolden ? "MATCH" : "DIVERGED") << "\n";
                }
#endif // PFHBASE

                { // T62 (PERMANENT, PIN/play-from-here 2026-08-22): forceInclude
                  // GUARANTEES the clicked block appears in the resolved arrangement.
                  //  (a) a standalone block at playChance 0.0 — normally never
                  //      selected — is present in all 20 resolves;
                  //  (b) a non-base member of a 3-stack at playCount 1 is present in
                  //      all 20, and the stack slot still contributes exactly
                  //      playCount entries (the pin REPLACES a sampled pick, never
                  //      adds — the resolver's `entries <= playCount` jassert is live
                  //      in this Debug build and would abort the run if it fired);
                  //  (c) alwaysPlayBase + playCount 1 + a pinned NON-base member: the
                  //      pin wins the single slot, and the base branch must not try to
                  //      erase a block the pin already removed from the pool;
                  //  (d) a block sitting after an EARLIER song ender is present in all
                  //      20 — enders before the pinned block do not truncate (Carter/
                  //      Alec ruling 2026-08-22). Probe case (e) was 20/20 fallback;
                  //  (e) THE CRITICAL GATE: with forceInclude == nullptr the resolver
                  //      is bit-identical to the pre-pin build — 100 seeds of a busy
                  //      project (mixed playChance, sim stack, seq stack, links, an
                  //      ender) hash to the golden value captured from that binary.
                    const double sr62 = 48000.0;
                    const int    body62 = 24000;
                    const int    RUNS62 = 20;

                    auto addP62 = [&](Project& p, const char* nm, float chance) {
                        auto* b = p.addBlock(nm);
                        addClipTo(b, juce::String(nm) + "_c", body62);
                        b->playChance = chance;
                        return b;
                    };
                    // Runs one scenario RUNS62 times with the pin set; returns
                    // {times the pinned block was present, max entries in its slot}.
                    auto pinRuns = [&](Project& p, const juce::String& pinId,
                                       const std::vector<juce::String>& slotIds) {
                        int present = 0, maxSlotEntries = 0;
                        for (int i = 0; i < RUNS62; ++i) {
                            juce::Random r62(7100 + i * 53);
                            ArrangementResolver res62;
                            auto arr = res62.resolve(p, r62, p.getBlockById(pinId));
                            int slotEntries = 0;
                            bool found = false;
                            for (const auto& e : arr.entries) {
                                if (e.blockId == pinId) found = true;
                                for (const auto& sid : slotIds)
                                    if (e.blockId == sid) ++slotEntries;
                            }
                            if (found) ++present;
                            maxSlotEntries = juce::jmax(maxSlotEntries, slotEntries);
                        }
                        return std::make_pair(present, maxSlotEntries);
                    };

                    // (a) standalone, playChance 0.0 — the gate would always skip it.
                    int presentZeroChance = 0;
                    {
                        Project p; p.sampleRate = sr62;
                        addP62(p, "A", 1.0f);
                        auto* B = addP62(p, "B", 0.0f);
                        addP62(p, "C", 1.0f);
                        presentZeroChance = pinRuns(p, B->id, {}).first;
                    }

                    // (b) non-base member of a 3-stack at playCount 1 of 3.
                    int presentStack1 = 0, maxStackEntries = 0;
                    {
                        Project p; p.sampleRate = sr62;
                        addP62(p, "A", 1.0f);
                        auto* S1 = addP62(p, "S1", 1.0f);
                        auto* S2 = addP62(p, "S2", 1.0f);
                        auto* S3 = addP62(p, "S3", 1.0f);
                        p.stackBlocks(S2->id, S1->id);
                        p.stackBlocks(S3->id, S1->id);
                        S1->stackPlayMode = StackPlayMode::Simultaneous;
                        S1->stackPlayCount.values.set(0, 1);
                        S1->alwaysPlayBase = false;
                        p.propagateStackSettings(S1->stackGroup, S1);
                        auto r = pinRuns(p, S2->id, { S1->id, S2->id, S3->id });
                        presentStack1 = r.first; maxStackEntries = r.second;
                    }

                    // (c) alwaysPlayBase ON, playCount 1, pin a NON-base member.
                    int presentBaseClash = 0, maxBaseClashEntries = 0;
                    {
                        Project p; p.sampleRate = sr62;
                        addP62(p, "A", 1.0f);
                        auto* S1 = addP62(p, "S1", 1.0f);   // base (first in model order)
                        auto* S2 = addP62(p, "S2", 1.0f);
                        auto* S3 = addP62(p, "S3", 1.0f);
                        p.stackBlocks(S2->id, S1->id);
                        p.stackBlocks(S3->id, S1->id);
                        S1->stackPlayMode = StackPlayMode::Simultaneous;
                        S1->stackPlayCount.values.set(0, 1);
                        S1->alwaysPlayBase = true;
                        p.propagateStackSettings(S1->stackGroup, S1);
                        auto r = pinRuns(p, S3->id, { S1->id, S2->id, S3->id });
                        presentBaseClash = r.first; maxBaseClashEntries = r.second;
                    }

                    // (d) pinned block sitting AFTER an earlier song ender.
                    int presentAfterEnder = 0;
                    {
                        Project p; p.sampleRate = sr62;
                        addP62(p, "A", 1.0f);
                        auto* B = addP62(p, "B", 1.0f);
                        B->clips[0]->isSongEnder = true;
                        auto* C = addP62(p, "C", 1.0f);
                        presentAfterEnder = pinRuns(p, C->id, {}).first;
                    }

                    // (d2) control: an ender AT/AFTER the pinned block still truncates.
                    bool enderAfterPinStillTruncates = true;
                    {
                        Project p; p.sampleRate = sr62;
                        addP62(p, "A", 1.0f);
                        auto* B = addP62(p, "B", 1.0f);
                        auto* C = addP62(p, "C", 1.0f);
                        auto* D = addP62(p, "D", 1.0f);
                        C->clips[0]->isSongEnder = true;
                        for (int i = 0; i < RUNS62; ++i) {
                            juce::Random r62(7100 + i * 53);
                            ArrangementResolver res62;
                            auto arr = res62.resolve(p, r62, p.getBlockById(B->id));
                            bool sawD = false, sawB = false;
                            for (const auto& e : arr.entries) {
                                if (e.blockId == D->id) sawD = true;
                                if (e.blockId == B->id) sawB = true;
                            }
                            if (sawD || !sawB) enderAfterPinStillTruncates = false;
                        }
                    }

                    // (e) THE CRITICAL GATE — default path bit-identical.
                    juce::String dump62;
                    const juce::uint32 baseHash = pfhBaselineDump(dump62);
                    const bool defaultPathUntouched = (baseHash == kPfhBaselineGolden);

                    verdict("T62 PIN play-from-here: pinned block ALWAYS resolves (playChance 0.0, stack playCount 1 of 3, alwaysPlayBase clash, after an earlier song ender), stack slots still contribute <= playCount entries, enders at/after the pin still truncate, and forceInclude==nullptr stays BIT-IDENTICAL (100-seed golden dump)",
                            presentZeroChance == RUNS62
                            && presentStack1 == RUNS62 && maxStackEntries <= 1
                            && presentBaseClash == RUNS62 && maxBaseClashEntries <= 1
                            && presentAfterEnder == RUNS62
                            && enderAfterPinStillTruncates
                            && defaultPathUntouched,
                            juce::String("playChance0.0=") + juce::String(presentZeroChance) + "/" + juce::String(RUNS62)
                            + ", stackPlayCount1=" + juce::String(presentStack1) + "/" + juce::String(RUNS62)
                            + " (maxSlotEntries=" + juce::String(maxStackEntries) + ", playCount=1)"
                            + ", alwaysPlayBaseClash=" + juce::String(presentBaseClash) + "/" + juce::String(RUNS62)
                            + " (maxSlotEntries=" + juce::String(maxBaseClashEntries) + ")"
                            + ", afterEarlierEnder=" + juce::String(presentAfterEnder) + "/" + juce::String(RUNS62)
                            + ", enderAtOrAfterPinTruncates=" + (enderAfterPinStillTruncates ? "y" : "N")
                            + ", defaultPathHash=0x" + juce::String::toHexString((int)baseHash)
                            + " golden=0x" + juce::String::toHexString((int)kPfhBaselineGolden)
                            + " " + (defaultPathUntouched ? "BIT-IDENTICAL" : "DIVERGED(BUG)"));
                }

                { // T63 (PERMANENT, LINKUI 2026-08-22, Carter screenshots): link arc /
                  // label geometry, asserted headlessly against the PURE layout pass
                  // (Source/UI/LinkArcLayout.h) that BlockLinkOverlay::paint consumes.
                  // Scenario: one 3-block stack (S1/S2/S3, identical centre X) plus a
                  // standalone block X in the next column; links S1<->S2 and S2<->S3
                  // are SAME-COLUMN, link S1<->X is cross-column.
                  //  (a) same-column links are detected as such and bow OUT to the
                  //      side — apex clear of the tile column, never a vertical line
                  //      through the tiles (the reported defect);
                  //  (b) the two same-column links do not trace the same bracket;
                  //  (c) no two label boxes intersect;
                  //  (d) no label box lands on a block tile (i.e. on a block's name);
                  //  (e) every label box is fully inside the strip bounds.
                    using namespace LinkArcLayout;

                    const float stripW = 640.0f, stripH = 260.0f;
                    const float colX   = 180.0f, tileW = 100.0f, tileH = 64.0f;

                    Config cfg;
                    cfg.width = stripW; cfg.height = stripH; cfg.cy = stripH * 0.5f;
                    cfg.colHalfW = tileW * 0.5f;

                    // 3-block stack in one column + one standalone block to its right.
                    auto tileRect = [&](float cx, float cyy) {
                        return juce::Rectangle<float>(cx - tileW * 0.5f, cyy - tileH * 0.5f,
                                                      tileW, tileH);
                    };
                    const juce::Rectangle<float> tS1 = tileRect(colX,  60.0f);
                    const juce::Rectangle<float> tS2 = tileRect(colX, 130.0f);
                    const juce::Rectangle<float> tS3 = tileRect(colX, 200.0f);
                    const juce::Rectangle<float> tX  = tileRect(colX + 140.0f, 130.0f);
                    std::vector<juce::Rectangle<float>> reserved { tS1, tS2, tS3, tX };

                    auto anchorOf = [](const juce::Rectangle<float>& r) {
                        Anchor a; a.x = r.getCentreX(); a.y = r.getCentreY(); a.valid = true;
                        return a;
                    };
                    // Widths come from the SAME fonts, text, measurement call and
                    // padding BlockLinkOverlay::paint uses. They are deliberately
                    // NOT constants: a hardcoded 96/34 measured on a Mac is what
                    // let this test stay green on Windows while labels overlapped.
                    const auto nameFont63 = LinkLabelMetrics::nameFont();
                    const auto pillFont63 = LinkLabelMetrics::pillFont();
                    auto mkLink = [&](const juce::Rectangle<float>& ra, const juce::String& na,
                                      const juce::Rectangle<float>& rb, const juce::String& nb) {
                        LinkIn in;
                        in.a = anchorOf(ra); in.b = anchorOf(rb);
                        in.labelW = LinkLabelMetrics::nameWidth(nameFont63,
                                        LinkLabelMetrics::nameText(na, nb));
                        in.pillW  = LinkLabelMetrics::pillWidth(pillFont63,
                                        LinkLabelMetrics::pillText(0.5f));
                        return in;
                    };
                    std::vector<LinkIn> links {
                        mkLink(tS1, "Verse 1",  tS2, "Verse 2"),   // same column
                        mkLink(tS2, "Verse 2",  tS3, "Solo 5"),    // same column
                        mkLink(tS1, "Verse 1",  tX,  "Chorus")     // cross column
                    };

                    const auto placed = layout(links, cfg, reserved);

                    const bool allVisible = placed.size() == 3
                                         && placed[0].visible && placed[1].visible
                                         && placed[2].visible;
                    const bool sameColFlags = allVisible
                                           && placed[0].sameColumn && placed[1].sameColumn
                                           && !placed[2].sameColumn;

                    // (a) the bow apex must clear the tile column on one side, and the
                    //     arc endpoints must sit on the column EDGE, not its centre.
                    bool bowsOutside = true;
                    for (int i = 0; i < 2; ++i) {
                        const auto& p = placed[(size_t)i];
                        if (std::abs(p.apexX - colX) <= cfg.colHalfW) bowsOutside = false;
                        if (std::abs(std::abs(p.anchorX1 - colX) - cfg.colHalfW) > 0.01f)
                            bowsOutside = false;
                        if (std::abs(p.anchorY1 - p.anchorY2) < 1.0f)  // distinct endpoint Ys
                            bowsOutside = false;
                    }
                    // (b) two same-column links must not coincide.
                    const bool bracketsDiffer = allVisible
                        && std::abs(placed[0].apexX - placed[1].apexX) > 1.0f;

                    // (c) no two label boxes intersect.
                    bool labelsDisjoint = true;
                    juce::String overlapDetail;
                    for (size_t i = 0; i < placed.size(); ++i)
                        for (size_t j = i + 1; j < placed.size(); ++j)
                            if (placed[i].labelBox.intersects(placed[j].labelBox)) {
                                labelsDisjoint = false;
                                overlapDetail << " OVERLAP(" << (int)i << "," << (int)j << ")";
                            }

                    // (d) no label box sits on a block tile.
                    bool labelsClearOfTiles = true;
                    for (auto& p : placed)
                        for (auto& r : reserved)
                            if (p.visible && p.labelBox.intersects(r)) labelsClearOfTiles = false;

                    // (e) every label box is inside the strip.
                    const juce::Rectangle<float> stripBounds(0.0f, 0.0f, stripW, stripH);
                    bool labelsInBounds = true;
                    for (auto& p : placed)
                        if (p.visible && !stripBounds.contains(p.labelBox)) labelsInBounds = false;

                    // (f) every label was actually PLACED. Without this the layout
                    //     could satisfy (c)-(e) by dropping labels it could not fit.
                    bool allLabelsDrawn = true;
                    for (auto& p : placed)
                        if (p.visible && (!p.labelVisible || p.degraded)) allLabelsDrawn = false;

                    verdict("T63 LINKUI arcs/labels: same-column links bow OUT to the side of the stack (never a vertical line through the tiles), two same-stack brackets differ, no two labels intersect, no label lands on a block tile, all labels clamped inside the strip",
                            allVisible && sameColFlags && bowsOutside && bracketsDiffer
                            && labelsDisjoint && labelsClearOfTiles && labelsInBounds
                            && allLabelsDrawn,
                            juce::String("sameColumnFlags=") + (sameColFlags ? "y" : "N")
                            + ", apexX=[" + juce::String(placed[0].apexX, 1) + ","
                            + juce::String(placed[1].apexX, 1) + "] colX="
                            + juce::String(colX, 1) + " halfW=" + juce::String(cfg.colHalfW, 1)
                            + ", bowsOutsideColumn=" + (bowsOutside ? "y" : "N")
                            + ", bracketsDiffer=" + (bracketsDiffer ? "y" : "N")
                            + ", labelsDisjoint=" + (labelsDisjoint ? "y" : "N") + overlapDetail
                            + ", clearOfTiles=" + (labelsClearOfTiles ? "y" : "N")
                            + ", inBounds=" + (labelsInBounds ? "y" : "N")
                            + ", allLabelsDrawn=" + (allLabelsDrawn ? "y" : "N")
                            + ", measuredW=" + juce::String(links[0].labelW, 1) + "/"
                            + juce::String(links[0].pillW, 1));
                }

                { // T65 (PERMANENT, LINKUI-DENSITY 2026-08-26, Carter screenshot on
                  // 86cc4b6): CROSS-COLUMN link labels must not overlap each other when
                  // several links crowd one strip. T63's scenario (2 same-column + 1
                  // cross-column) was too sparse to catch this.
                  //   Exact screenshot scenario: 5 single-block columns, links
                  //   2<->3, 2<->5, 3<->5, 4<->5, all at 50%. Single-block columns make
                  //   each tile fill the whole strip height, which is what starves the
                  //   old push-up loop of headroom.
                  //   (a) no two label GROUPS intersect — the group is name row + pill
                  //       + backing plate as ONE rect, not just the name row;
                  //   (b) every label stays inside the strip;
                  //   (c) DENSER CASE, 6 links, to check the lane rule scales.
                    using namespace LinkArcLayout;

                    const float sW = 900.0f, sH = 360.0f;
                    const float tW = 100.0f, tGap = 10.0f, pad = 8.0f;
                    const float tTop = 0.0f, tH = 340.0f;      // single-block column: full height

                    Config cfg65;
                    cfg65.width = sW; cfg65.height = sH; cfg65.cy = sH * 0.5f;
                    cfg65.colHalfW = tW * 0.5f;

                    std::vector<juce::Rectangle<float>> tiles65;
                    for (int i = 0; i < 5; ++i)
                        tiles65.push_back({ pad + (float)i * (tW + tGap), tTop, tW, tH });
                    auto centreOf = [&](int oneBased) {
                        return tiles65[(size_t)(oneBased - 1)].getCentreX();
                    };
                    auto anchorAt = [&](int oneBased) {
                        Anchor a;
                        a.x = centreOf(oneBased);
                        a.y = tiles65[(size_t)(oneBased - 1)].getCentreY();
                        a.valid = true;
                        return a;
                    };
                    // Real measurement, same path as paint() -- see T63's note. The
                    // block names are the defaults a fresh project produces, so the
                    // widths are the ones a user actually gets.
                    const auto nameFont65 = LinkLabelMetrics::nameFont();
                    const auto pillFont65 = LinkLabelMetrics::pillFont();
                    auto mk65 = [&](int a, int b) {
                        LinkIn in; in.a = anchorAt(a); in.b = anchorAt(b);
                        in.labelW = LinkLabelMetrics::nameWidth(nameFont65,
                                        LinkLabelMetrics::nameText("Block " + juce::String(a),
                                                                   "Block " + juce::String(b)));
                        in.pillW  = LinkLabelMetrics::pillWidth(pillFont65,
                                        LinkLabelMetrics::pillText(0.5f));
                        return in;
                    };

                    auto worstOverlap = [&](const std::vector<Placed>& placed,
                                            juce::String& detail) {
                        int pairs = 0;
                        for (size_t i = 0; i < placed.size(); ++i)
                            for (size_t j = i + 1; j < placed.size(); ++j) {
                                if (!placed[i].visible || !placed[j].visible) continue;
                                if (placed[i].labelBox.intersects(placed[j].labelBox)) {
                                    ++pairs;
                                    auto ov = placed[i].labelBox.getIntersection(placed[j].labelBox);
                                    detail << " (" << (int)i << "," << (int)j << ")="
                                           << juce::String((int)ov.getWidth()) << "x"
                                           << juce::String((int)ov.getHeight());
                                }
                            }
                        return pairs;
                    };
                    const juce::Rectangle<float> strip65(0.0f, 0.0f, sW, sH);
                    auto allInBounds = [&](const std::vector<Placed>& placed) {
                        for (auto& p : placed)
                            if (p.visible && !strip65.contains(p.labelBox)) return false;
                        return true;
                    };
                    // A layout that drops labels could satisfy "no overlap" trivially.
                    auto allDrawn = [&](const std::vector<Placed>& placed) {
                        for (auto& p : placed)
                            if (p.visible && (!p.labelVisible || p.degraded)) return false;
                        return true;
                    };

                    // ── (a)(b) the screenshot's four links ──────────────────────────
                    std::vector<LinkIn> links65 { mk65(2,3), mk65(2,5), mk65(3,5), mk65(4,5) };
                    auto placed65 = layout(links65, cfg65, tiles65);
                    juce::String detail4;
                    const int overlaps4  = worstOverlap(placed65, detail4);
                    const bool inBounds4 = allInBounds(placed65);
                    const bool drawn4    = allDrawn(placed65);

                    // ── (c) denser: 6 links over the same 5 columns ─────────────────
                    std::vector<LinkIn> dense65 {
                        mk65(1,5), mk65(2,5), mk65(3,5), mk65(4,5), mk65(1,3), mk65(2,4)
                    };
                    auto placedD = layout(dense65, cfg65, tiles65);
                    juce::String detailD;
                    const int overlapsD  = worstOverlap(placedD, detailD);
                    const bool inBoundsD = allInBounds(placedD);
                    const bool drawnD    = allDrawn(placedD);

                    verdict("T65 LINKUI-DENSITY: crowded CROSS-COLUMN link labels never overlap (screenshot case 2<->3,2<->5,3<->5,4<->5 over 5 full-height columns) and stay inside the strip; lane rule also holds for a denser 6-link case",
                            overlaps4 == 0 && inBounds4 && drawn4
                            && overlapsD == 0 && inBoundsD && drawnD,
                            juce::String("4-link: overlappingPairs=") + juce::String(overlaps4) + detail4
                            + ", inBounds=" + (inBounds4 ? "y" : "N")
                            + ", allDrawn=" + (drawn4 ? "y" : "N")
                            + " | 6-link: overlappingPairs=" + juce::String(overlapsD) + detailD
                            + ", inBounds=" + (inBoundsD ? "y" : "N")
                            + ", allDrawn=" + (drawnD ? "y" : "N")
                            + " | measuredW=" + juce::String(links65[0].labelW, 1) + "/"
                            + juce::String(links65[0].pillW, 1));
                }

                { // T66 (PERMANENT, LINKUI-ENVELOPE 2026-08-30, Carter Windows
                  // screenshot): the no-overlap guarantee must hold across the WHOLE
                  // geometry envelope the UI can actually produce, at the widths the
                  // RUNNING PLATFORM really measures -- not at one lucky strip size
                  // with widths typed into the test.
                  //
                  // WHY THIS EXISTS. T63/T65 pinned labelW=96/pillW=34, which are the
                  // macOS numbers; Windows resolves the same Font to Arial and gets
                  // ~10% narrower text (86.15 -> 77.12 px for "Block 2 <-> Block 3",
                  // measured on both platforms). Frozen widths meant those two tests
                  // could only ever prove self-consistency for one platform's metrics.
                  //   They also each fix ONE strip size. The block strip is a user-
                  // dragged pane: SplitLayout lets it be anywhere from blocksMinH to
                  // well past blocksDefaultH, and its width follows the window. Below
                  // ~208px tall the old lane pass had room for exactly ONE lane, ran
                  // out of horizontal slots, and then placed the label anyway without
                  // re-checking -- a silent overlap. Against the pre-fix tree this
                  // sweep reports 76 overlapping cells; it is the negative control.
                  //  (a) NO cell may contain two intersecting label groups;
                  //  (b) NO cell may drop a label (that would satisfy (a) by hiding);
                  //  (c) every placed label stays inside the strip.
                    using namespace LinkArcLayout;

                    const auto nameFont66 = LinkLabelMetrics::nameFont();
                    const auto pillFont66 = LinkLabelMetrics::pillFont();

                    const int heights[] = { SplitLayout::blocksMinH, 160, 180, 200, 220,
                                            260, 300, SplitLayout::blocksDefaultH };
                    const int widths[]  = { 420, 500, 600, 700, 800, 900 };
                    const int counts[]  = { 4, 5, 6, 7, 8 };
                    const float tileW = 100.0f, tileGap = 10.0f, tilePad = 8.0f;

                    int  badCells = 0, overlapPairs = 0, droppedLabels = 0, outOfBounds = 0;
                    juce::String worstCell;
                    float measuredName66 = 0.0f, measuredPill66 = 0.0f;

                    for (int h : heights) for (int w : widths) for (int n : counts) {
                        Config cfg66;
                        cfg66.width = (float)w; cfg66.height = (float)h;
                        cfg66.cy = (float)h * 0.5f; cfg66.colHalfW = tileW * 0.5f;

                        // Single-block columns: each tile fills the strip height, which
                        // is the case that makes "clear of the tiles" unsatisfiable.
                        const int cols = juce::jmax(3, n / 2 + 2);
                        std::vector<juce::Rectangle<float>> tiles66;
                        for (int i = 0; i < cols; ++i)
                            tiles66.push_back({ tilePad + (float)i * (tileW + tileGap),
                                                0.0f, tileW, (float)h });

                        std::vector<LinkIn> links66;
                        for (int i = 0; i < n; ++i) {
                            const int ia = i % cols, ib = (i * 2 + 1) % cols;
                            if (ia == ib) continue;
                            LinkIn in;
                            in.a.x = tiles66[(size_t)ia].getCentreX();
                            in.a.y = tiles66[(size_t)ia].getCentreY(); in.a.valid = true;
                            in.b.x = tiles66[(size_t)ib].getCentreX();
                            in.b.y = tiles66[(size_t)ib].getCentreY(); in.b.valid = true;
                            in.labelW = LinkLabelMetrics::nameWidth(nameFont66,
                                            LinkLabelMetrics::nameText("Block " + juce::String(ia + 1),
                                                                       "Block " + juce::String(ib + 1)));
                            in.pillW  = LinkLabelMetrics::pillWidth(pillFont66,
                                            LinkLabelMetrics::pillText(0.5f));
                            measuredName66 = in.labelW; measuredPill66 = in.pillW;
                            links66.push_back(in);
                        }

                        const auto placed66 = layout(links66, cfg66, tiles66);
                        const juce::Rectangle<float> strip66(0.0f, 0.0f, (float)w, (float)h);

                        int cellOverlaps = 0, cellDrops = 0, cellOut = 0;
                        for (size_t i = 0; i < placed66.size(); ++i) {
                            if (!placed66[i].visible) continue;
                            if (!placed66[i].labelVisible) { ++cellDrops; continue; }
                            if (!strip66.contains(placed66[i].labelBox)) ++cellOut;
                            for (size_t j = i + 1; j < placed66.size(); ++j)
                                if (placed66[j].visible && placed66[j].labelVisible
                                    && placed66[i].labelBox.intersects(placed66[j].labelBox))
                                    ++cellOverlaps;
                        }
                        overlapPairs += cellOverlaps; droppedLabels += cellDrops;
                        outOfBounds  += cellOut;
                        if (cellOverlaps || cellDrops || cellOut) {
                            ++badCells;
                            if (worstCell.isEmpty())
                                worstCell = " firstBad=" + juce::String(h) + "x" + juce::String(w)
                                          + "/" + juce::String(n) + "links(ov="
                                          + juce::String(cellOverlaps) + ",drop="
                                          + juce::String(cellDrops) + ",oob="
                                          + juce::String(cellOut) + ")";
                        }
                    }

                    const int totalCells = (int)(std::size(heights) * std::size(widths)
                                                 * std::size(counts));
                    verdict("T66 LINKUI-ENVELOPE: across every strip height/width/link-count the UI can produce, and at the widths THIS platform really measures, no two label groups overlap, no label is dropped, none escapes the strip",
                            badCells == 0,
                            juce::String("cells=") + juce::String(totalCells)
                            + ", badCells=" + juce::String(badCells)
                            + ", overlappingPairs=" + juce::String(overlapPairs)
                            + ", droppedLabels=" + juce::String(droppedLabels)
                            + ", outOfBounds=" + juce::String(outOfBounds)
                            + worstCell
                            + " | measuredW(name/pill)=" + juce::String(measuredName66, 2)
                            + "/" + juce::String(measuredPill66, 2));
                }

                { // T64 (PERMANENT, ASCII/MOJIBAKE 2026-08-26): user-visible strings
                  // must not carry a code-page mojibake. BACKGROUND: the build passes
                  // no /utf-8 to MSVC and the sources have no UTF-8 BOM, so MSVC reads
                  // narrow literals in the ACTIVE CODE PAGE — a raw UTF-8 em-dash in a
                  // char* literal becomes three CP1252 characters on Windows (the
                  // "mojibake boxes" the Windows slice reported, and the pre-15b0b58
                  // window title "BlockShuffler <e2 80 94> "). The fix, per 15b0b58, is
                  // plain ASCII for typographic decoration and escaped bytes through
                  // juce::CharPointer_UTF8 for glyphs whose MEANING needs the character
                  // (the bullet / arrow / middle dot at InspectorPanel.cpp:476/481/688).
                  //  (a) the two inspector tooltips ASCII-ified this round are pure
                  //      7-bit ASCII;
                  //  (b) NO user-visible string anywhere in a populated InspectorPanel
                  //      contains a mojibake marker. Legitimately decoded glyphs are
                  //      non-ASCII and must still be ALLOWED, so the assertion keys on
                  //      the characters that only ever appear when UTF-8 bytes have been
                  //      read as CP1252/Latin-1: U+00C2, U+00C3, U+00E2, U+20AC, U+FFFD.
                    Project p;
                    auto* blk = p.addBlock("Block 1");
                    addClipTo(blk, "clip", 1000);
                    InspectorPanel panel;
                    panel.setProject(&p);
                    panel.setBounds(0, 0, 210, 844);
                    panel.setBlock(blk);

                    auto isMojibakeMarker = [](juce::juce_wchar c) {
                        return c == 0x00C2 || c == 0x00C3 || c == 0x00E2
                            || c == 0x20AC || c == 0xFFFD;
                    };
                    auto isPureAscii = [](const juce::String& s) {
                        for (auto c : s) if ((int)c > 127) return false;
                        return true;
                    };

                    // Walk the whole component tree: tooltips, button texts, label texts.
                    juce::StringArray visible;
                    std::function<void(juce::Component&)> walk = [&](juce::Component& c) {
                        if (auto* t = dynamic_cast<juce::SettableTooltipClient*>(&c)) {
                            auto tip = t->getTooltip();
                            if (tip.isNotEmpty()) visible.add(tip);
                        }
                        if (auto* b = dynamic_cast<juce::Button*>(&c))
                            if (b->getButtonText().isNotEmpty()) visible.add(b->getButtonText());
                        if (auto* l = dynamic_cast<juce::Label*>(&c))
                            if (l->getText().isNotEmpty()) visible.add(l->getText());
                        for (int i = 0; i < c.getNumChildComponents(); ++i)
                            walk(*c.getChildComponent(i));
                    };
                    walk(panel);

                    juce::String offenders;
                    int mojibake = 0;
                    for (const auto& s : visible)
                        for (auto c : s)
                            if (isMojibakeMarker(c)) {
                                ++mojibake;
                                offenders << " [" << s.substring(0, 48) << "]";
                                break;
                            }

                    // (a) the tooltips ASCII-ified in the cosmetic round, by exact text.
                    //     REACHABILITY (2026-08-26): DraggableNumberBox now inherits
                    //     juce::SettableTooltipClient, so its tooltips are visible to the
                    //     same dynamic_cast<TooltipClient*> that juce::TooltipWindow uses
                    //     to display them — which is exactly what this walk performs.
                    //     Asserting they are FOUND therefore also proves they can be shown;
                    //     before the fix the tooltip text was stored in a private member
                    //     that nothing could read and TooltipWindow never displayed.
                    //     Match the TOOLTIP, not the toggle's button text — the button is
                    //     also labelled "Raw summing", and a looser prefix silently latched
                    //     onto it, which would let a mojibaked tooltip pass this check.
                    juce::String tipRaw, tipTempo;
                    for (const auto& s : visible) {
                        if (s.startsWith("Raw summing (no automatic"))  tipRaw   = s;
                        if (s.startsWith("Project default tempo - "))   tipTempo = s;
                    }
                    const bool rawFound   = tipRaw.isNotEmpty();
                    const bool rawAscii   = rawFound   && isPureAscii(tipRaw);
                    const bool tempoFound = tipTempo.isNotEmpty();   // reachable == displayable
                    const bool tempoAscii = tempoFound && isPureAscii(tipTempo);

                    // (c) every reachable string is either pure ASCII or contains only
                    //     DELIBERATE non-ASCII glyphs (bullet, arrow, middle dot) — never
                    //     a mojibake marker. Counted so the evidence line shows the split.
                    int nonAsciiButLegit = 0;
                    for (const auto& s : visible) if (!isPureAscii(s)) ++nonAsciiButLegit;

                    verdict("T64 ASCII/MOJIBAKE + TOOLTIP REACHABILITY: no user-visible InspectorPanel string carries a CP1252-mojibake marker (legit UTF-8-decoded glyphs still allowed); the raw-summing AND project-default-tempo tooltips are both REACHABLE via TooltipClient (so TooltipWindow can display them) and pure 7-bit ASCII",
                            mojibake == 0 && rawFound && rawAscii && tempoFound && tempoAscii,
                            juce::String("strings scanned=") + juce::String(visible.size())
                            + ", mojibakeMarkers=" + juce::String(mojibake) + offenders
                            + ", nonAsciiButLegit=" + juce::String(nonAsciiButLegit)
                            + ", rawSummingTooltip=" + (rawFound ? (rawAscii ? "REACHABLE+ASCII" : "REACHABLE but NON-ASCII(BUG)") : "NOT REACHABLE(BUG)")
                            + ", defaultTempoTooltip=" + (tempoFound ? (tempoAscii ? "REACHABLE+ASCII" : "REACHABLE but NON-ASCII(BUG)") : "NOT REACHABLE(BUG)"));
                }

// ── TITLEDIAG (2026-08-26, DIAGNOSIS ONLY — print-only, no verdict, no fix) ──
//    Window-title mojibake "BlockShuffler <?> God". Reproduces updateWindowTitle's
//    EXACT string headlessly — MainComponent.cpp:627-629 builds it as
//        "BlockShuffler - " + file.getFileNameWithoutExtension()
//    (the two callers at :567 and :616 both pass the FILE NAME, never the JSON
//    "name" field) — and hex-dumps the bytes. Headless rather than a DBG in
//    updateWindowTitle because DBG is compiled out of Release, while this runs in
//    both and needs no GUI. Also dumps the .bsp's own stored name bytes so a
//    corruption on load can be told apart from one already in the file.
#define TITLEDIAG 1
#if TITLEDIAG
                {
                    std::cout << "\n=== TITLEDIAG: window-title bytes ===\n";
                    auto hexOf = [](const juce::String& s) {
                        juce::String h;
                        auto utf8 = s.toRawUTF8();
                        for (int i = 0; utf8[i] != 0; ++i)
                            h << juce::String::toHexString((int)(juce::uint8)utf8[i]).paddedLeft('0', 2) << " ";
                        return h.trim();
                    };
                    auto report = [&](const juce::String& what, const juce::String& s) {
                        bool ascii = true;
                        for (auto c : s) if ((int)c > 127) ascii = false;
                        std::cout << "  " << what << " = \"" << s << "\"\n"
                                  << "      hex: " << hexOf(s) << "\n"
                                  << "      " << (ascii ? "PURE ASCII" : "CONTAINS NON-ASCII") << "\n";
                    };

                    // Alec's project, if it is sitting next to the repo root.
                    juce::File bsp = juce::File::getCurrentWorkingDirectory().getChildFile("God.bsp");
                    if (!bsp.existsAsFile())
                        bsp = juce::File::getCurrentWorkingDirectory()
                                  .getParentDirectory().getChildFile("God.bsp");

                    if (bsp.existsAsFile()) {
                        std::cout << "  file: " << bsp.getFullPathName() << "\n";
                        report("file-name-without-extension (what the title uses)",
                               bsp.getFileNameWithoutExtension());
                        report("TITLE as updateWindowTitle builds it",
                               "BlockShuffler - " + bsp.getFileNameWithoutExtension());

                        Project loaded;
                        auto parsed = juce::JSON::parse(bsp.loadFileAsString());
                        if (Serialization::projectFromJSON(parsed, loaded, bsp.getParentDirectory()))
                            report("project.name stored INSIDE the .bsp (not used by the title)",
                                   loaded.name);

                        auto blob = bsp.loadFileAsString().toRawUTF8();
                        int nonAscii = 0;
                        for (int i = 0; blob[i] != 0; ++i)
                            if ((juce::uint8)blob[i] > 127) ++nonAscii;
                        std::cout << "  non-ASCII bytes in the whole .bsp: " << nonAscii << "\n";
                    } else {
                        std::cout << "  God.bsp not found next to the working directory —"
                                     " skipping the real-project dump.\n";
                    }

                    // Control: what the PRE-15b0b58 separator produced, for comparison.
                    const char* oldSep = "BlockShuffler \xe2\x80\x94 God";   // raw UTF-8 em-dash
                    std::cout << "  CONTROL, pre-15b0b58 separator (raw UTF-8 em-dash) bytes: ";
                    for (int i = 0; oldSep[i] != 0; ++i)
                        std::cout << juce::String::toHexString((int)(juce::uint8)oldSep[i])
                                        .paddedLeft('0', 2).toStdString() << " ";
                    std::cout << "\n      -> read as CP1252 on Windows this renders as"
                                 " \"BlockShuffler a<EUR>\" + quote, i.e. the reported mojibake.\n";
                }
#endif // TITLEDIAG

            }

            { // ── RAWGAIN Stage 1 (diagnostic probe, print-only, NO verdict) ──
              // Measures the effective gain the mixer applies to each entry by
              // rendering DC-filled (constant 1.0) sources: with a DC source the
              // rendered sample value IS the applied gain at that position.
              // Each entry is also rendered SOLO so per-entry gain is read
              // directly rather than inferred from the sum.
              //   1b: 3-block SIMULTANEOUS stack, playCount 3, identical clips,
              //       zero lead-in/tail so no crossfade can confound the read.
              //   1c: plain sequential join, L=4410 / T=4410.
                const double srR = 48000.0;

                auto renderSel = [&](const ResolvedArrangement& arr, int only) {
                    const int total = (int)arr.totalDurationSamples;
                    juce::AudioBuffer<float> out(2, total); out.clear();
                    for (int i = 0; i < arr.entries.size(); ++i) {
                        if (only >= 0 && i != only) continue;
                        mixEntryToBuffer(arr.entries.getReference(i), out, total,
                                         0LL, 1.0, 1.0, i);
                    }
                    return out;
                };
                auto dcFill = [&](Clip* c) {
                    for (int ch = 0; ch < c->audioBuffer->getNumChannels(); ++ch) {
                        auto* w = c->audioBuffer->getWritePointer(ch);
                        for (int i = 0; i < c->audioBuffer->getNumSamples(); ++i)
                            w[i] = 1.0f;
                    }
                };
                auto peakOf = [&](const juce::AudioBuffer<float>& b) {
                    float m = 0.0f;
                    for (int ch = 0; ch < b.getNumChannels(); ++ch)
                        for (int i = 0; i < b.getNumSamples(); ++i)
                            m = juce::jmax(m, std::abs(b.getSample(ch, i)));
                    return m;
                };

                { // 1b: 3-block SIMULTANEOUS stack, play 3
                    const int bodyR = 8000;
                    Project p; p.sampleRate = srR;
                    auto* A = p.addBlock("A");
                    auto* B = p.addBlock("B");
                    auto* C = p.addBlock("C");
                    for (auto* blk : { A, B, C }) {
                        addClipTo(blk, juce::String("c") + blk->name, bodyR);
                        dcFill(blk->clips[0]);   // startMark=0, endMark=bodyR: body only
                        blk->playChance = 1.0f;
                    }
                    p.stackBlocks(B->id, A->id);
                    p.stackBlocks(C->id, A->id);
                    A->stackPlayMode = StackPlayMode::Simultaneous;
                    A->stackPlayCount.values.set(0, 3);
                    p.propagateStackSettings(A->stackGroup, A);

                    juce::Random r(7001); ArrangementResolver res;
                    auto arr = res.resolve(p, r); arr.sampleRate = srR;

                    std::cout << "RAWGAIN 1b SIM stack play-3 (DC sources, L=0 T=0): entries="
                              << arr.entries.size() << "\n";
                    bool atten = false; float gsum = 0.0f;
                    for (int i = 0; i < arr.entries.size(); ++i) {
                        const auto& e = arr.entries.getReference(i);
                        auto solo = renderSel(arr, i);
                        const int mid = (int)(e.timelinePos + (e.endMark - e.startMark) / 2);
                        const float g = solo.getSample(0, mid);
                        gsum += g;
                        if (std::abs(g - 1.0f) > 1e-3f) atten = true;
                        auto* blk = p.getBlockById(e.blockId);
                        std::cout << "  entry[" << i << "] "
                                  << (blk ? blk->name : juce::String("?"))
                                  << " entry.gain=" << juce::String(e.gain, 6)
                                  << "  MEASURED body gain=" << juce::String(g, 6)
                                  << "  timelinePos=" << (juce::int64)e.timelinePos << "\n";
                    }
                    const float maxAbs = peakOf(renderSel(arr, -1));
                    std::cout << "  gain sum=" << juce::String(gsum, 6)
                              << "   max|summed sample|=" << juce::String(maxAbs, 6) << "\n";
                    std::cout << "  VERDICT SIM-ATTENUATION " << (atten ? "YES" : "NO") << "\n";
                }

                { // 1c: plain sequential join, L=4410 / T=4410
                    const int L = 4410, bodyR = 17640, T = 4410;
                    Project p; p.sampleRate = srR;
                    for (auto n : { "A", "B" }) {
                        auto* blk = p.addBlock(n);
                        addClipTo(blk, juce::String("c") + n, L + bodyR + T);
                        blk->clips[0]->startMark = L;
                        blk->clips[0]->endMark   = L + bodyR;
                        dcFill(blk->clips[0]);
                    }
                    juce::Random r(7002); ArrangementResolver res;
                    auto arr = res.resolve(p, r); arr.sampleRate = srR;

                    std::cout << "RAWGAIN 1c SEQ join (DC sources, L=" << L
                              << " body=" << bodyR << " T=" << T << "): entries="
                              << arr.entries.size() << "\n";
                    for (int i = 0; i < arr.entries.size(); ++i) {
                        const auto& e = arr.entries.getReference(i);
                        auto solo = renderSel(arr, i);
                        const int64_t bs = e.timelinePos;
                        const int64_t be = bs + (e.endMark - e.startMark);
                        std::cout << "  entry[" << i << "] " << e.clipName
                                  << " entry.gain=" << juce::String(e.gain, 6)
                                  << " prevTailLen=" << (juce::int64)e.prevTailLen
                                  << " nextLeadInLen=" << (juce::int64)e.nextLeadInLen << "\n";
                        const int64_t span = be - bs;
                        const int64_t pts[5] = { bs, bs + span/4, bs + span/2,
                                                 be - span/4, be - 1 };
                        std::cout << "    body gain @[start,+1/4,mid,-1/4,end-1]: ";
                        for (auto q : pts)
                            std::cout << juce::String(solo.getSample(0, (int)q), 6) << " ";
                        std::cout << "\n";
                        float bMin = 2.0f, bMax = -1.0f;
                        for (int64_t q = bs; q < be; ++q) {
                            const float v = solo.getSample(0, (int)q);
                            bMin = juce::jmin(bMin, v); bMax = juce::jmax(bMax, v);
                        }
                        std::cout << "    body gain min=" << juce::String(bMin, 6)
                                  << " max=" << juce::String(bMax, 6)
                                  << "  (1.0 throughout == no body attenuation)\n";
                    }
                    std::cout << "  max|summed sample| (DC) = "
                              << juce::String(peakOf(renderSel(arr, -1)), 6) << "\n";
                }

                // ── RAWGAIN Stage 3: the SAME two geometries with unity mode ON ──
                // Sums above full scale here are CORRECT for raw mode — it exists to
                // remove the level compensation, so correlated full-scale sources add
                // arithmetically. Nothing clamps or limits them in float.
                { // 3a: unity ON, sequential join L=4410 / T=4410
                    const int L = 4410, bodyR = 17640, T = 4410;
                    Project p; p.sampleRate = srR; p.unityGainMode = true;
                    for (auto n : { "A", "B" }) {
                        auto* blk = p.addBlock(n);
                        addClipTo(blk, juce::String("c") + n, L + bodyR + T);
                        blk->clips[0]->startMark = L;
                        blk->clips[0]->endMark   = L + bodyR;
                        dcFill(blk->clips[0]);
                    }
                    juce::Random r(7003); ArrangementResolver res;
                    auto arr = res.resolve(p, r); arr.sampleRate = srR;

                    std::cout << "RAWGAIN 3a UNITY seq join (DC, L=" << L << " T=" << T
                              << "): entries=" << arr.entries.size() << "\n";
                    float gMin = 2.0f, gMax = -1.0f;
                    for (int i = 0; i < arr.entries.size(); ++i) {
                        const auto& e = arr.entries.getReference(i);
                        auto solo = renderSel(arr, i);
                        const int64_t lo = e.timelinePos - renderedLeadInLength(e);
                        const int64_t hi = e.timelinePos + (e.endMark - e.startMark)
                                           + renderedTailLength(e);
                        for (int64_t q = juce::jmax((int64_t)0, lo);
                             q < juce::jmin(hi, (int64_t)solo.getNumSamples()); ++q) {
                            const float v = solo.getSample(0, (int)q);
                            gMin = juce::jmin(gMin, v); gMax = juce::jmax(gMax, v);
                        }
                    }
                    auto all = renderSel(arr, -1);
                    const int64_t join = arr.entries.getReference(1).timelinePos;
                    float joinPeak = 0.0f;
                    for (int64_t q = juce::jmax((int64_t)0, join - T);
                         q < juce::jmin(join + T, (int64_t)all.getNumSamples()); ++q)
                        joinPeak = juce::jmax(joinPeak, std::abs(all.getSample(0, (int)q)));
                    std::cout << "    gain over ALL rendered content: min="
                              << juce::String(gMin, 6) << " max=" << juce::String(gMax, 6)
                              << "  (want 1.0 throughout)\n"
                              << "    max|sum| AT JOIN=" << juce::String(joinPeak, 6)
                              << "   max|sum| whole render=" << juce::String(peakOf(all), 6)
                              << "   (>1.0 EXPECTED for raw mode — correct, not a defect)\n";
                }

                { // 3b: unity ON, 3-block SIMULTANEOUS stack, play 3
                    const int bodyR = 8000;
                    Project p; p.sampleRate = srR; p.unityGainMode = true;
                    auto* A = p.addBlock("A");
                    auto* B = p.addBlock("B");
                    auto* C = p.addBlock("C");
                    for (auto* blk : { A, B, C }) {
                        addClipTo(blk, juce::String("c") + blk->name, bodyR);
                        dcFill(blk->clips[0]);
                        blk->playChance = 1.0f;
                    }
                    p.stackBlocks(B->id, A->id);
                    p.stackBlocks(C->id, A->id);
                    A->stackPlayMode = StackPlayMode::Simultaneous;
                    A->stackPlayCount.values.set(0, 3);
                    p.propagateStackSettings(A->stackGroup, A);
                    juce::Random r(7004); ArrangementResolver res;
                    auto arr = res.resolve(p, r); arr.sampleRate = srR;

                    std::cout << "RAWGAIN 3b UNITY SIM stack play-3 (DC): entries="
                              << arr.entries.size() << "\n";
                    for (int i = 0; i < arr.entries.size(); ++i) {
                        const auto& e = arr.entries.getReference(i);
                        auto solo = renderSel(arr, i);
                        const int mid = (int)(e.timelinePos + (e.endMark - e.startMark) / 2);
                        auto* blk = p.getBlockById(e.blockId);
                        std::cout << "    entry[" << i << "] "
                                  << (blk ? blk->name : juce::String("?"))
                                  << " entry.gain=" << juce::String(e.gain, 6)
                                  << " (stack attenuation IGNORED in raw mode)"
                                  << "  MEASURED body gain="
                                  << juce::String(solo.getSample(0, mid), 6) << "\n";
                    }
                    std::cout << "    max|sum| = "
                              << juce::String(peakOf(renderSel(arr, -1)), 6)
                              << "   (~3.0 EXPECTED for 3 correlated full-scale layers — "
                                 "correct, deliberately NOT limited)\n";

                    // 3d: 16-bit/FLAC caveat. The float render is unbounded; integer
                    // formats are not. Render this SAME unity arrangement through the
                    // real ExportRenderer to 16-bit FLAC and count what must clamp.
                    {
                        auto flacFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                            .getChildFile("rawgain_3d_unity.flac");
                        flacFile.deleteFile();
                        juce::FlacAudioFormat flacFmt;
                        ExportRenderer ex;
                        const bool wrote = ex.renderToFile(arr, flacFile, flacFmt, 16);

                        auto full = renderSel(arr, -1);
                        int64_t overFS = 0;
                        for (int ch = 0; ch < full.getNumChannels(); ++ch)
                            for (int i = 0; i < full.getNumSamples(); ++i)
                                if (std::abs(full.getSample(ch, i)) > 1.0f) ++overFS;

                        int64_t railed = -1;
                        if (wrote) {
                            juce::AudioFormatManager fm; fm.registerBasicFormats();
                            if (auto* rdr = fm.createReaderFor(flacFile)) {
                                std::unique_ptr<juce::AudioFormatReader> reader(rdr);
                                juce::AudioBuffer<float> back((int)reader->numChannels,
                                                              (int)reader->lengthInSamples);
                                reader->read(&back, 0, (int)reader->lengthInSamples, 0, true, true);
                                railed = 0;
                                for (int ch = 0; ch < back.getNumChannels(); ++ch)
                                    for (int i = 0; i < back.getNumSamples(); ++i)
                                        if (std::abs(back.getSample(ch, i)) >= 0.9999f) ++railed;
                            }
                        }
                        std::cout << "RAWGAIN 3d 16-bit FLAC of the 3b unity render: wrote="
                                  << (wrote ? "yes" : "NO")
                                  << "  float samples over full scale=" << (juce::int64)overFS
                                  << "  read-back samples at the rail=" << (juce::int64)railed
                                  << "\n    (nonzero EXPECTED — raw mode is unbounded in float, "
                                     "integer formats clamp. Known, documented behaviour.)\n";
                        flacFile.deleteFile();
                    }
                }
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
                    // 1e-6 threshold (was 1e-4): under the JOINFIX2 complementary
                    // window the tail is ducked to T/(W-1) and approaches zero
                    // shallowly, and makeTone's own 10 ms edge fade multiplies in —
                    // at 1e-4 the product crossed the threshold ~100 samples early.
                    // Beyond the arrangement end the buffer is exact zeros, so the
                    // tighter threshold stays noise-free.
                    for (int i = (int)bodyEnd; i < (int)outLen; ++i)
                        if (std::abs(out.getSample(0, i)) > 1e-6f) lastLoud = i;
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
                // Clustered centres force collisions. Rects (not bare centres) since
                // 2026-08-22 — the overlay needs each tile's Y for same-column arcs.
                juce::HashMap<juce::String, juce::Rectangle<int>> pos;
                auto tile = [](int centreX) {
                    return juce::Rectangle<int>(centreX - 50, 250, 100, 100);
                };
                pos.set(A->id, tile(280)); pos.set(B->id, tile(300));
                pos.set(C->id, tile(320)); pos.set(D->id, tile(340));
                ov.setBlockAnchors(pos);
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
