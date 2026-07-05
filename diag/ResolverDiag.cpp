// STEP1 DIAG (temporary — remove in MASTER_PROMPT Step 7).
// Headless reproduction of the stack play-count regression:
// builds a 3-block stack through the same model APIs the UI uses
// (Project::addBlock / stackBlocks / propagateStackSettings) and calls the
// same ArrangementResolver::resolve() the transport Play button calls.
#include <iostream>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Model/Project.h"
#include "Audio/ArrangementResolver.h"

using namespace BlockShuffler;

static std::shared_ptr<juce::AudioBuffer<float>> makeBuf(int len = 1000) {
    auto b = std::make_shared<juce::AudioBuffer<float>>(2, len);
    b->clear();
    return b;
}

static void addClipTo(Block* blk, const juce::String& nm) {
    auto c = std::make_unique<Clip>();
    c->name = nm;
    c->audioBuffer = makeBuf();
    c->startMark = 0;
    c->endMark = 1000;
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

    std::cout << "DONE\n";
    return 0;
}
