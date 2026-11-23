#pragma once
// ── QA HOOK (BLOCKSHUFFLER_QA_HOOK, OFF by default; build-qa only) ────────────
// Drives the REAL app from a script named by $BS_QA_SCRIPT, so visual claims are
// measured on what the user sees: the project is opened through the same
// MainWindow::openFile() -> MainComponent::loadProject() the Open button uses,
// the message loop runs between steps, mouse input goes in through the window's
// ComponentPeer (the same entry point AppKit events use), and snapshots are of
// the whole window. This file only READS and SNAPSHOTS. It contains no layout.
//
// Script (one command per line, '#' = comment):
//   open <bsp>            size <w> <h>           wait <ms>
//   snap <png>            dump <txt>             select <block name>|none
//   link <block name>     (enters link mode from that block, like the menu item)
//   esc                   undo                  click <block name>
//   drag <from> <to> <fx> <fy> [shift]   grab <from>'s tile centre, release at
//                         <to>'s tile x+fx*w, y+fy*h (fy>1 = below the tile)
//   order <txt>           append blocks[] / on-screen stack order
//   quit
#if BLOCKSHUFFLER_QA_HOOK
#include <juce_gui_basics/juce_gui_basics.h>
#include "../UI/BlockStrip.h"
#include "../UI/BlockLinkOverlay.h"

namespace BlockShuffler {

struct QaHook : private juce::Timer {
    QaHook(juce::Component& w, std::function<void(const juce::File&)> opener)
        : window(w), open(std::move(opener)) {
        juce::File f(juce::SystemStats::getEnvironmentVariable("BS_QA_SCRIPT", {}));
        f.readLines(lines);
        startTimer(50);
    }

private:
    juce::Component& window;
    std::function<void(const juce::File&)> open;
    juce::StringArray lines;
    int pc = 0;
    // pending drag
    juce::Array<juce::Point<float>> dragPath; juce::ModifierKeys dragMods;

    template <typename T> static T* find(juce::Component& c) {
        if (auto* t = dynamic_cast<T*>(&c)) return t;
        for (auto* ch : c.getChildren()) if (auto* t = find<T>(*ch)) return t;
        return nullptr;
    }
    BlockStrip* strip() { return find<BlockStrip>(window); }
    Block* blockNamed(const juce::String& n) {
        auto* s = strip(); if (!s || !s->project) return nullptr;
        for (auto* b : s->project->blocks) if (b->name == n) return b;
        return nullptr;
    }
    BlockComponent* compFor(Block* b) {
        for (auto* bc : strip()->blockComponents) if (bc->getBlock() == b) return bc;
        return nullptr;
    }
    static juce::String r2s(juce::Rectangle<float> r) {
        return "(" + juce::String(r.getX(), 1) + "," + juce::String(r.getY(), 1) + " "
             + juce::String(r.getWidth(), 1) + "x" + juce::String(r.getHeight(), 1) + ")";
    }
    static juce::String p2s(float x, float y) { return "(" + juce::String(x, 1) + "," + juce::String(y, 1) + ")"; }

    void mouse(juce::Point<float> windowPt, juce::ModifierKeys mods) {
        // JUCE reads keyboard modifiers (Shift) from this global, not from the event.
        juce::ModifierKeys::currentModifiers = mods;
        if (auto* peer = window.getPeer())
            peer->handleMouseEvent(juce::MouseInputSource::InputSourceType::mouse, windowPt, mods,
                                   juce::MouseInputSource::defaultPressure,
                                   juce::MouseInputSource::defaultOrientation,
                                   juce::Time::currentTimeMillis());
    }

    void timerCallback() override {
        stopTimer();
        if (!dragPath.isEmpty()) {                 // one mouse step per tick, loop runs between
            auto p = dragPath.removeAndReturn(0);
            mouse(p, dragPath.isEmpty() ? juce::ModifierKeys(dragMods.withoutMouseButtons()) : dragMods);
            startTimer(dragPath.isEmpty() ? 300 : 16);
            return;
        }
        if (pc >= lines.size()) { juce::JUCEApplicationBase::quit(); return; }
        auto t = juce::StringArray::fromTokens(lines[pc++].trim(), " ", "\"");
        t.removeEmptyStrings();
        for (auto& s : t) s = s.unquoted();
        int next = 50;
        const auto cmd = t.isEmpty() || t[0].startsWith("#") ? juce::String() : t[0];
        if (cmd == "open")  { open(juce::File(t[1])); next = 600; }
        else if (cmd == "size")  { window.setSize(t[1].getIntValue(), t[2].getIntValue()); next = 600; }
        else if (cmd == "wait")  next = t[1].getIntValue();
        else if (cmd == "snap")  { auto img = window.createComponentSnapshot(window.getLocalBounds(), true, 1.0f);
                                   juce::File f(t[1]); f.deleteFile(); juce::FileOutputStream os(f);
                                   juce::PNGImageFormat().writeImageToStream(img, os); }
        else if (cmd == "dump")  dump(juce::File(t[1]));
        else if (cmd == "order") order(juce::File(t[1]), t.size() > 2 ? t[2] : juce::String());
        else if (cmd == "select") { strip()->selectBlock(t[1] == "none" ? nullptr : blockNamed(t.joinIntoString(" ", 1)));
                                    next = 300; }
        else if (cmd == "link")  { strip()->enterLinkMode(blockNamed(t.joinIntoString(" ", 1))->id); next = 300; }
        else if (cmd == "esc")   { strip()->cancelPendingMode(); next = 300; }
        else if (cmd == "undo")  { strip()->project->undoManager.undo(); next = 400; }
        else if (cmd == "drag")  { startDrag(t); next = 16; }
        else if (cmd == "click") { auto* bc = compFor(blockNamed(t.joinIntoString(" ", 1)));   // press+release on tile centre
                                   const auto a = window.getLocalPoint(bc, bc->getLocalBounds().getCentre()).toFloat();
                                   const juce::ModifierKeys down(juce::ModifierKeys::leftButtonModifier);
                                   mouse(a, {}); mouse(a, down); mouse(a, {}); next = 400; }
        else if (cmd == "quit")  { juce::ModifierKeys::currentModifiers = {}; juce::JUCEApplicationBase::quit(); return; }
        startTimer(next);
    }

    // drag "Block 5" "Block 3" 0.5 0.5 [shift]
    void startDrag(const juce::StringArray& t) {
        auto* from = compFor(blockNamed(t[1])); auto* to = compFor(blockNamed(t[2]));
        if (!from || !to) { DBG("QA drag: unknown block"); return; }
        const auto a = window.getLocalPoint(from, from->getLocalBounds().getCentre()).toFloat();
        const auto tb = window.getLocalArea(to, to->getLocalBounds()).toFloat();
        const juce::Point<float> b(tb.getX() + t[3].getFloatValue() * tb.getWidth(),
                                   tb.getY() + t[4].getFloatValue() * tb.getHeight());
        dragMods = juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier
                                      | (t[5] == "shift" ? juce::ModifierKeys::shiftModifier : 0));
        mouse(a, juce::ModifierKeys(dragMods.withoutMouseButtons()));        // hover
        mouse(a, dragMods);                                                  // press
        for (int k = 1; k <= 20; ++k) dragPath.add(a + (b - a) * ((float)k / 20.0f));
        dragPath.add(b);                                                     // release (last)
    }

    void order(const juce::File& f, const juce::String& tag) {
        auto* s = strip(); juce::String out = "[" + tag + "] blocks[]:";
        for (auto* b : s->project->blocks) out << " " << b->name.fromFirstOccurrenceOf("Block ", false, false)
                                               << (b->stackGroup >= 0 ? "/s" + juce::String(b->stackGroup) : "");
        std::map<int, std::vector<std::pair<int, juce::String>>> stacks;
        for (auto* b : s->project->blocks)
            if (b->stackGroup >= 0) if (auto* bc = compFor(b))
                stacks[b->stackGroup].push_back({ bc->getY(), b->name.fromFirstOccurrenceOf("Block ", false, false) });
        for (auto& [g, v] : stacks) {
            std::sort(v.begin(), v.end());
            out << "  | screen s" << g << " top->bottom:";
            for (auto& [y, n] : v) out << " " << n;
        }
        f.appendText(out + "\n");
    }

    void dump(const juce::File& f) {
        auto* s = strip();
        juce::String out;
        auto& vp = s->viewport;
        out << "window " << window.getWidth() << "x" << window.getHeight()
            << "  strip " << r2s(s->getBounds().toFloat())
            << "  viewport " << r2s(vp.getBounds().toFloat())
            << "  viewH(visible) " << vp.getMaximumVisibleHeight()
            << "  contentH " << s->contentArea.getHeight()
            << "  contentW " << s->contentArea.getWidth()
            << "  vScrollBar " << (vp.getVerticalScrollBar().isVisible() ? "VISIBLE" : "hidden")
            << "  hScrollBar " << (vp.getHorizontalScrollBar().isVisible() ? "VISIBLE" : "hidden")
            << "  viewPos " << vp.getViewPositionX() << "," << vp.getViewPositionY() << "\n";
        out << "tiles (strip-local):\n";
        for (auto* bc : s->blockComponents) if (auto* b = bc->getBlock())
            out << "  " << b->name << (b->stackGroup >= 0 ? " [s" + juce::String(b->stackGroup) + "]" : "")
                << " " << r2s(s->getLocalArea(bc->getParentComponent(), bc->getBounds()).toFloat()) << "\n";
#if BS_OVERLAY_HAS_QA
        if (auto* ov = find<BlockLinkOverlay>(window)) {
            auto* p = ov->project;
            out << "overlay " << r2s(ov->getBounds().toFloat()) << " viewportRect " << r2s(ov->viewportRect.toFloat())
                << " links=" << ov->qaPlaced.size() << " paints=" << ov->qaPaintSeq << " drawOrder";
            for (int k : ov->qaDrawOrder) out << " L" << k;
            out << "\n";
            for (size_t i = 0; i < ov->qaPlaced.size() && (int)i < p->links.size(); ++i) {
                const auto& q = ov->qaPlaced[i]; const auto& in = ov->qaIns[i];
                auto* l = p->links[(int)i];
                auto nm = [&](const juce::String& id) { auto* b = p->getBlockById(id); return b ? b->name : juce::String("?"); };
                out << "  L" << (int)i << " " << nm(l->blockA) << " <-> " << nm(l->blockB)
                    << (q.sameColumn ? "  SAME-COL" : "  CROSS") << (q.visible ? "" : " HIDDEN")
                    << "\n     A tile " << r2s(in.a.tile) << (in.a.stacked ? " stacked" : "")
                    << " anchor " << p2s(q.anchorX1, q.anchorY1)
                    << "\n     B tile " << r2s(in.b.tile) << (in.b.stacked ? " stacked" : "")
                    << " anchor " << p2s(q.anchorX2, q.anchorY2)
                    << "\n     ctrl " << p2s(q.ctrlX1, q.arcControlY) << " " << p2s(q.ctrlX2, q.arcControlY)
                    << " apex " << p2s(q.apexX, q.apexY) << " mid " << p2s(q.midX, q.midY)
                    << "\n     label " << r2s(q.labelBox) << (q.labelVisible ? "" : " NOT-SHOWN")
                    << (q.displaced ? " displaced" : "")
                    << "\n     prob " << juce::String(l->swapProbability, 2)
                    << " arcAlpha " << juce::String(ov->qaArcAlpha[i], 3) << " labelAlpha " << juce::String(ov->qaLabelAlpha[i], 3)
                    << " stroke 2.0\n";
            }
        }
#endif
        f.replaceWithText(out);
    }
};

} // namespace BlockShuffler
#endif
