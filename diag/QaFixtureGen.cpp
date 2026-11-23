// QaFixtureGen -- manual-QA project generator (diag only, EXCLUDE_FROM_ALL).
//
//   cmake --build build-diag --target QaFixtureGen
//   ./build-diag/QaFixtureGen_artefacts/Debug/QaFixtureGen <outDir> <repoRoot> [onlyFilesContaining]
//
// Every project is built with the real model API (addBlock / stackBlocks /
// propagateStackSettings / addLink / Clip::loadFromFile, the same calls the UI
// makes), written with Project::saveToFile (the app's only save path) and read
// back with Project::loadFromFile (what MainComponent::loadProject() calls).
// Audio: macOS `say` -> 16-bit WAV, padded/trimmed here to exactly 2.0 s
// (one 4/4 bar at 120 BPM).
#include <iostream>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Model/Project.h"
#include "Audio/ArrangementResolver.h"

using namespace BlockShuffler;

static juce::File gOut;
static int gFailures = 0;
static juce::String gOnly;   // optional 3rd arg: only (re)write files whose name contains this

static void say(const juce::String& s) { std::cout << s << std::endl; }

/** audio/<stem>.wav: `text` spoken at `rate`, exactly 2.0 s long. Cached per run. */
static juce::File speech(const juce::String& text, int rate) {
    const auto stem = text.toLowerCase().replaceCharacter(' ', '_') + (rate == 48000 ? "_48k" : "");
    auto dir = gOut.getChildFile("audio");
    dir.createDirectory();
    auto out = dir.getChildFile(stem + ".wav");
    if (out.existsAsFile()) return out;

    auto tmp = dir.getChildFile(stem + ".say.wav");
    juce::ChildProcess cp;
    cp.start(juce::StringArray { "say", "--file-format=WAVE",
                                 "--data-format=LEI16@" + juce::String(rate),
                                 "-o", tmp.getFullPathName(), text });
    cp.waitForProcessToFinish(30000);

    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(tmp));
    if (!r) { say("AUDIO FAIL " + text); ++gFailures; return out; }
    const int len = rate * 2;                                   // 2.0 s exactly
    juce::AudioBuffer<float> buf(1, len);
    buf.clear();
    r->read(&buf, 0, (int)juce::jmin<juce::int64>(len, r->lengthInSamples), 0, true, false);
    r.reset();
    tmp.deleteFile();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os(out.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(os.get(), rate, 1, 16, {}, 0));
    if (w) { os.release(); w->writeFromAudioSampleBuffer(buf, 0, len); }
    return out;
}

/** One block named `name` with its spoken clip -- the same import call the UI uses. */
static Block* blk(Project& p, const juce::String& name, int rate = 44100, const juce::String& clipText = {}) {
    auto* b = p.addBlock(name);
    auto c = std::make_unique<Clip>();
    const auto f = speech(clipText.isEmpty() ? name : clipText, rate);
    if (c->loadFromFile(f, p.formatManager, p.sampleRate)) b->addClip(std::move(c));
    else { say("CLIP LOAD FAIL " + f.getFullPathName()); ++gFailures; }
    return b;
}

static void addTake(Project& p, Block* b, const juce::String& text, float prob) {
    auto c = std::make_unique<Clip>();
    c->loadFromFile(speech(text, 44100), p.formatManager, p.sampleRate);
    c->probability = prob;
    b->addClip(std::move(c));
}

/** Stack `members` (first = base) via the model's own stackBlocks + propagate. */
static void stack(Project& p, std::vector<Block*> members, StackPlayMode mode, int playCount, bool base = false) {
    for (size_t i = 1; i < members.size(); ++i) p.stackBlocks(members[i]->id, members[0]->id);
    auto* b0 = members[0];
    b0->stackPlayMode = mode;
    b0->stackPlayCount.values.clearQuick(); b0->stackPlayCount.weights.clearQuick();
    b0->stackPlayCount.values.add(playCount); b0->stackPlayCount.weights.add(1.0f);
    b0->alwaysPlayBase = base;
    p.propagateStackSettings(b0->stackGroup, b0);
}

static Block* byName(Project& p, const juce::String& n) {
    for (auto* b : p.blocks) if (b->name == n) return b;
    jassertfalse; return nullptr;
}
static void link(Project& p, int a, int b, float prob) {
    p.addLink(byName(p, "Block " + juce::String(a))->id, byName(p, "Block " + juce::String(b))->id, prob);
}

/** Canonical summary of a project: compared before save and after reload. */
static juce::StringArray summary(Project& p) {
    juce::StringArray s;
    s.add("blocks=" + juce::String(p.blocks.size()));
    std::map<int, juce::StringArray> groups;
    for (auto* b : p.blocks) if (b->stackGroup >= 0) groups[b->stackGroup].add(b->name);
    for (auto& [g, names] : groups) {
        auto* base = byName(p, names[0]);
        s.add("stack[" + names.joinIntoString(", ") + "] base=" + names[0]
              + " mode=" + (base->stackPlayMode == StackPlayMode::Sequential ? "SEQ" : "SIM")
              + " play=" + juce::String(base->stackPlayCount.values[0])
              + " baseToggle=" + (base->alwaysPlayBase ? "ON" : "off"));
    }
    for (auto* l : p.links) {
        auto* a = p.getBlockById(l->blockA); auto* b = p.getBlockById(l->blockB);
        s.add("link " + (a ? a->name : "?") + " <-> " + (b ? b->name : "?") + " "
              + juce::String(juce::roundToInt(l->swapProbability * 100.0f)) + "%");
    }
    juce::String clips = "clips/block:";
    for (auto* b : p.blocks) clips << " " << b->name.fromFirstOccurrenceOf("Block ", false, false) << "=" << b->clips.size();
    s.add(clips);
    return s;
}

/** Save through the app's save path, reload through the app's load path, compare. */
static void saveAndCheck(Project& p, const juce::String& fileName) {
    if (gOnly.isNotEmpty() && !fileName.contains(gOnly)) return;   // leave already-signed-off files untouched
    auto f = gOut.getChildFile(fileName);
    const auto before = summary(p);
    if (!p.saveToFile(f)) { say("SAVE FAIL " + fileName); ++gFailures; return; }

    Project q;
    const bool loaded = q.loadFromFile(f);
    const auto after = summary(q);

    // Paths as written: relative, forward slashes, under audio/.
    int badPaths = 0, paths = 0;
    auto json = juce::JSON::parse(f);
    for (auto& b : *json["blocks"].getArray())
        for (auto& c : *b["clips"].getArray()) {
            const auto ap = c["audioFile"].toString(); ++paths;
            if (!ap.startsWith("audio/") || ap.containsChar('\\') || juce::File::isAbsolutePath(ap)) ++badPaths;
        }

    const bool match = loaded && before == after;
    say("\n=== " + fileName + "  round-trip: " + (match ? "MATCH" : "MISMATCH")
        + "  missingAudio=" + juce::String(q.missingFilesOnLoad.size())
        + "  audioPaths=" + juce::String(paths) + " (non-relative/backslash: " + juce::String(badPaths) + ")");
    for (auto& line : after) say("  " + line);
    if (!match) { say("  BEFORE SAVE:"); for (auto& line : before) say("    " + line); }
    if (!match || q.missingFilesOnLoad.size() != 0 || badPaths != 0) ++gFailures;
}

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) { say("usage: QaFixtureGen <outDir> <repoRoot> [onlyFilesContaining]"); return 2; }
    if (argc > 3) gOnly = juce::String(argv[3]);
    gOut = juce::File(juce::String(argv[1]));
    gOut.createDirectory();
    const juce::File repo { juce::String(argv[2]) };
    const auto SEQ = StackPlayMode::Sequential, SIM = StackPlayMode::Simultaneous;

    { // QA1 -- Issue 1: link-mode source marker
        Project p; p.name = "QA1_link_mode";
        blk(p, "Block 1"); blk(p, "Block 2");
        auto* b3 = blk(p, "Block 3"); auto* b4 = blk(p, "Block 4"); auto* b6 = blk(p, "Block 6");
        blk(p, "Block 5"); blk(p, "Block 7");
        auto* b8 = blk(p, "Block 8"); auto* b9 = blk(p, "Block 9");
        stack(p, { b3, b4, b6 }, SEQ, 1);
        stack(p, { b8, b9 }, SEQ, 1);
        saveAndCheck(p, "QA1_link_mode.bsp");
    }
    { // QA2 -- Issue 2: the client screenshot (= diag/fixtures/link_stack_fixture)
        Project p; p.name = "QA2_links_between_stacks";
        for (int n : { 1, 2, 3, 4, 6, 12, 5, 7, 8, 9, 10, 11 }) blk(p, "Block " + juce::String(n));
        stack(p, { byName(p, "Block 3"), byName(p, "Block 4"), byName(p, "Block 6"), byName(p, "Block 12") }, SEQ, 1);
        stack(p, { byName(p, "Block 10"), byName(p, "Block 11") }, SEQ, 1);
        for (auto [a, b] : std::vector<std::pair<int,int>> { {3,4},{3,6},{6,4},{12,4},{12,3},{11,12},{12,10},
                                                             {3,10},{6,10},{11,6},{4,10},{11,4},{11,3} })
            link(p, a, b, 0.5f);
        saveAndCheck(p, "QA2_links_between_stacks.bsp");
    }
    { // QA2b -- Issue 2 reference style (= diag/fixtures/link_flat_fixture)
        Project p; p.name = "QA2b_flat_links";
        for (int n = 1; n <= 8; ++n) blk(p, "Block " + juce::String(n));
        for (auto [a, b] : std::vector<std::pair<int,int>> { {1,3},{2,5},{4,7},{6,8} }) link(p, a, b, 0.5f);
        saveAndCheck(p, "QA2b_flat_links.bsp");
    }
    { // QA2c -- Issue 2 stress: 9-member stack A, 3-member stack B
        Project p; p.name = "QA2c_tall_stack_links";
        for (int n = 1; n <= 16; ++n) blk(p, "Block " + juce::String(n));
        std::vector<Block*> a, b;
        for (int n = 2; n <= 10; ++n) a.push_back(byName(p, "Block " + juce::String(n)));
        for (int n = 13; n <= 15; ++n) b.push_back(byName(p, "Block " + juce::String(n)));
        stack(p, a, SEQ, 1); stack(p, b, SEQ, 1);
        // between stacks: top/middle/bottom of A (2/6/10) x top/middle/bottom of B (13/14/15)
        for (auto [x, y] : std::vector<std::pair<int,int>> { {2,13},{6,14},{10,15},{2,15},{10,13},{6,13} }) link(p, x, y, 0.5f);
        link(p, 4, 16, 0.5f);   // stack A member -> unstacked (right)
        link(p, 14, 1, 0.5f);   // stack B member -> unstacked (left)
        saveAndCheck(p, "QA2c_tall_stack_links.bsp");
    }
    { // QA3 -- Issue 3: every inspector section reachable
        Project p; p.name = "QA3_tooltips";
        auto* b1 = blk(p, "Block 1", 44100, "Block 1 take 1");
        b1->clips[0]->probability = 0.6f;
        addTake(p, b1, "Block 1 take 2", 0.3f);
        addTake(p, b1, "Block 1 take 3", 0.1f);
        auto* b2 = blk(p, "Block 2"); auto* b3 = blk(p, "Block 3");
        auto* b4 = blk(p, "Block 4"); auto* b5 = blk(p, "Block 5");
        blk(p, "Block 6");
        stack(p, { b2, b3 }, SIM, 2, true);   // simultaneous, base toggle ON
        stack(p, { b4, b5 }, SEQ, 1);
        link(p, 1, 6, 0.5f);
        saveAndCheck(p, "QA3_tooltips.bsp");
    }
    { // QA4 -- Issue 4: 8 unstacked, a 3-stack and a 9-stack, gaps both sides of each stack
        Project p; p.name = "QA4_drag_zones";
        for (int n = 1; n <= 20; ++n) blk(p, "Block " + juce::String(n));
        std::vector<Block*> s3, s9;
        for (int n = 2; n <= 4; ++n)  s3.push_back(byName(p, "Block " + juce::String(n)));
        for (int n = 8; n <= 16; ++n) s9.push_back(byName(p, "Block " + juce::String(n)));
        stack(p, s3, SEQ, 1); stack(p, s9, SEQ, 1);
        saveAndCheck(p, "QA4_drag_zones.bsp");
    }
    { // QA4b -- stress50 fixture, re-saved through the canonical save path
        Project p;
        if (!p.loadFromFile(repo.getChildFile("diag/fixtures/stress50_fixture.bsp"))) { say("LOAD FAIL stress50"); ++gFailures; }
        else saveAndCheck(p, "QA4b_stress50.bsp");
    }
    { // QA5 -- link swaps by ear (4.4 / base link / A3) + 48 kHz pitch check (10.5)
        Project p; p.name = "QA5_link_swaps_audio";
        blk(p, "Block 1"); blk(p, "Block 2", 48000); blk(p, "Block 3"); blk(p, "Block 4");
        auto* b5 = blk(p, "Block 5"); auto* b6 = blk(p, "Block 6"); auto* b7 = blk(p, "Block 7", 48000);
        blk(p, "Block 8"); blk(p, "Block 9");
        stack(p, { b5, b6, b7 }, SEQ, 3);
        link(p, 1, 3, 1.0f); link(p, 4, 5, 1.0f); link(p, 8, 6, 1.0f);
        saveAndCheck(p, "QA5_link_swaps_audio.bsp");

        // Expected spoken order: the REAL resolver on the RELOADED file, 5 seeds.
        Project q; q.loadFromFile(gOut.getChildFile("QA5_link_swaps_audio.bsp"));
        ArrangementResolver res;
        auto orderOf = [&](juce::Random& rng, bool withTimes) {
            juce::String order;
            for (auto& e : res.resolve(q, rng).entries) {
                auto* b = q.getBlockById(e.blockId);
                order << (b ? b->name.fromFirstOccurrenceOf("Block ", false, false) : "?");
                if (withTimes) order << "@" << juce::String(e.timelinePos / q.sampleRate, 1) << "s";
                order << "  ";
            }
            return order.trimEnd();
        };
        // Seeds spread out: juce::Random is an LCG, and adjacent small seeds
        // (1,2,3...) give near-identical first draws -- i.e. the same shuffle.
        say("\n=== QA5 resolver runs (reloaded file, ArrangementResolver::resolve)");
        for (int run = 1; run <= 5; ++run) {
            juce::Random rng((juce::int64)run * 0x9E3779B97F4A7C15LL);
            say("  run " + juce::String(run) + ": " + orderOf(rng, true));
        }
        std::map<juce::String, int> tally;
        juce::Random rng(0x5EED);
        for (int i = 0; i < 1000; ++i) ++tally[orderOf(rng, false)];
        say("  1000 runs, distinct orders:");
        for (auto& [o, n] : tally) say("    " + juce::String(n).paddedLeft(' ', 4) + "x  " + o);
    }

    { // QA6 -- where a block lands when stacked: 4 unstacked + a 4-stack with neighbours both sides
        Project p; p.name = "QA6_stack_insert_order";
        for (int n = 1; n <= 8; ++n) blk(p, "Block " + juce::String(n));
        stack(p, { byName(p, "Block 3"), byName(p, "Block 4"), byName(p, "Block 5"), byName(p, "Block 6") }, SEQ, 1);
        saveAndCheck(p, "QA6_stack_insert_order.bsp");
    }

    say(juce::String("\nGENERATOR RESULT: ") + (gFailures == 0 ? "ALL OK" : juce::String(gFailures) + " FAILURE(S)"));
    return gFailures == 0 ? 0 : 1;
}
