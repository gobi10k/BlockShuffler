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
//   preview <txt> <tag>   append what the strip's drop preview showed just before
//                         the last drag's release (action, target, insert index)
//   sweep <from> <to> <txt>  grab <from>, hover 1px steps across <to>'s column at
//                         its centre y (-40..+140px), log the live preview per px,
//                         release back where it started (no-op drop)
//   wheel <dx>            horizontal wheel over the block strip (through the peer)
//   tips <txt>            append every reachable tooltip, measured with the
//                         TooltipWindow's own LookAndFeel (wrap/clip/ellipsis)
//   quit
#if BLOCKSHUFFLER_QA_HOOK
#include <juce_gui_basics/juce_gui_basics.h>
#include "../UI/BlockStrip.h"
#include "../UI/BlockLinkOverlay.h"
#include "../UI/LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

struct QaHook : private juce::Timer {
    QaHook(juce::Component& w, std::function<void(const juce::File&)> opener)
        : window(w), open(std::move(opener)) {
        juce::File f(juce::SystemStats::getEnvironmentVariable("BS_QA_SCRIPT", {}));
        f.readLines(lines);
        // macOS hit-tests through the window server (ComponentPeer::contains asks
        // which window is TOPMOST at the point): a QA window launched behind another
        // app silently loses every synthetic press. Keep it frontmost for the run.
        if (auto* tlw = dynamic_cast<juce::TopLevelWindow*>(&window)) tlw->setAlwaysOnTop(true);
        juce::Process::makeForegroundProcess();
        window.toFront(true);
        startTimer(50);
    }

private:
    juce::Component& window;
    std::function<void(const juce::File&)> open;
    juce::StringArray lines;
    int pc = 0;
    // pending drag
    juce::Array<juce::Point<float>> dragPath; juce::ModifierKeys dragMods;
    juce::String lastPreview;                       // strip drop preview just before release
    juce::File sweepOut; float sweepColX = 0; int sweepPending = 0; juce::String sweepLog;

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
            // PEN source, not mouse (macOS never creates touch sources): same
            // ComponentPeer::handleMouseEvent entry and
            // same component mouseDown/Drag/Up, but a source of its own, so real
            // pointer activity on this machine cannot move it mid-drag (2026-11-23:
            // the shared mouse source was overwritten to the menu bar, drags vanished).
            peer->handleMouseEvent(juce::MouseInputSource::InputSourceType::pen, windowPt, mods,
                                   juce::MouseInputSource::defaultPressure,
                                   juce::MouseInputSource::defaultOrientation,
                                   juce::Time::currentTimeMillis());
    }

    void timerCallback() override {
        stopTimer();
        if (!dragPath.isEmpty()) {                 // one mouse step per tick, loop runs between
            if (dragPath.size() == 1) lastPreview = previewString();
            auto p = dragPath.removeAndReturn(0);
            mouse(p, dragPath.isEmpty() ? juce::ModifierKeys(dragMods.withoutMouseButtons()) : dragMods);
            if (approachSkip > 0) --approachSkip;
            else if (sweepPending > 0) {                 // sweep step: log what the live preview shows here
                --sweepPending;
                sweepLog << "  x=" << juce::String(p.x - sweepColX, 0) << " " << previewString() << "\n";
                if (sweepPending == 0) { sweepOut.appendText(sweepLog); sweepLog = {}; }
            }
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
        else if (cmd == "size")  { window.setSize(t[1].getIntValue(), t[2].getIntValue());
                                   // pin to the main display: on a multi-display Mac the window
                                   // was observed drifting onto the second screen between files
                                   if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
                                       window.setTopLeftPosition(d->userArea.getPosition() + juce::Point<int>(20, 20));
                                   next = 600; }
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
        else if (cmd == "plus")  { auto& b = strip()->addButton;                                // real press+release on the strip's "+" button
                                   const auto a = window.getLocalPoint(&b, b.getLocalBounds().getCentre()).toFloat();
                                   const juce::ModifierKeys down(juce::ModifierKeys::leftButtonModifier);
                                   mouse(a, {}); mouse(a, down); mouse(a, {}); next = 400; }
        else if (cmd == "tempo") { auto* pr = strip()->project; juce::String o;               // model tempo state + what each tile draws
                                   o << "[" << t[2] << "] default " << pr->defaultClipTempo << "  blocks " << pr->blocks.size()
                                     << "  canUndo " << (pr->undoManager.canUndo() ? "y" : "n") << "\n";
                                   for (auto* b : pr->blocks) o << "   " << b->name << "  tempo " << b->tempo
                                     << (b->tempoOverridden ? "  OVERRIDDEN" : "  inherits") << "  tile label \"" << (int)b->tempo << " bpm\""
                                     << (strip()->selectedBlockId == b->id ? "  SELECTED" : "") << "\n";
                                   juce::File(t[1]).appendText(o); }
        else if (cmd == "deftempo") { strip()->project->setDefaultTempo(t[1].getDoubleValue()); next = 300; }
        else if (cmd == "preview") juce::File(t[1]).appendText("[" + t[2] + "] preview: " + lastPreview + "\n");
        else if (cmd == "sweep") { startSweep(t); next = 16; }
        else if (cmd == "tips")  tips(juce::File(t[1]));
        else if (cmd == "wheel") { auto* s = strip();
                                   const auto c = window.getLocalPoint(s, s->getLocalBounds().getCentre()).toFloat();
                                   juce::MouseWheelDetails w { t[1].getFloatValue(), 0.0f, false, false, false };
                                   if (auto* peer = window.getPeer())
                                       peer->handleMouseWheel(juce::MouseInputSource::InputSourceType::mouse, c,
                                                              juce::Time::currentTimeMillis(), w);
                                   next = 400; }
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

    juce::String previewString() {
        auto* s = strip();
        using DA = BlockStrip::DropAction;
        const auto a = s->currentDropAction;
        juce::String out = a == DA::Stack ? "STACK" : a == DA::Reorder ? "INSERT"
                         : a == DA::RearrangeInStack ? "REARRANGE" : "NONE";
        if ((a == DA::Stack || a == DA::RearrangeInStack) && s->dropTargetComp && s->dropTargetComp->getBlock())
            out << " target=" << s->dropTargetComp->getBlock()->name;
        else if (a == DA::Reorder)
            out << " before=" << (s->dropTargetIndex >= 0 && s->dropTargetIndex < s->project->blocks.size()
                                  ? s->project->blocks[s->dropTargetIndex]->name : juce::String("END"));
        if (a == DA::None) {   // diagnose a drag that never registered
            const juce::MouseInputSource* src = nullptr;
            for (auto& m : juce::Desktop::getInstance().getMouseSources())
                if (m.isPen()) src = &m;
            if (src == nullptr) return out << " (no input source)";
            auto& ms = *src;
            out << " (activeDrag=" << (s->activeDragComp ? "y" : "n")
                << " mouseScreen=" << ms.getScreenPosition().toString()
                << " down=" << (ms.isDragging() ? "drag" : ms.getCurrentModifiers().isAnyMouseButtonDown() ? "y" : "n")
                << " underMouse=" << (ms.getComponentUnderMouse() ? ms.getComponentUnderMouse()->getName() + "/" + juce::String(typeid(*ms.getComponentUnderMouse()).name()) : juce::String("null"))
                << " winScreen=" << window.getScreenBounds().toString() << ")";
        }
        return out;
    }

    // sweep "Block 6" "Block 7" out.txt -- hover only; releases where it started.
    void startSweep(const juce::StringArray& t) {
        auto* from = compFor(blockNamed(t[1])); auto* to = compFor(blockNamed(t[2]));
        if (!from || !to) return;
        sweepOut = juce::File(t[3]);
        const auto a  = window.getLocalPoint(from, from->getLocalBounds().getCentre()).toFloat();
        const auto tb = window.getLocalArea(to, to->getLocalBounds()).toFloat();
        sweepColX = tb.getX();
        sweepLog = "[sweep " + t[1] + " over " + t[2] + "] x relative to the target column's left edge\n";
        dragMods = juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier);
        mouse(a, {}); mouse(a, dragMods);
        const float y = tb.getCentreY();
        for (int k = 0; k <= 10; ++k) dragPath.add(a + (juce::Point<float>(tb.getX() - 40, y) - a) * (k / 10.0f));
        sweepPending = 0;
        for (int dx = -40; dx <= 140; ++dx) { dragPath.add({ tb.getX() + (float)dx, y }); ++sweepPending; }
        // the sweep log is written by the step that consumes the last sweep point
        for (int k = 1; k <= 10; ++k) dragPath.add(juce::Point<float>(tb.getX() + 140, y) + (a - juce::Point<float>(tb.getX() + 140, y)) * (k / 10.0f));
        dragPath.add(a);
        // sweepPending counts down only on sweep points: skip the approach steps
        approachSkip = 11;
    }
    int approachSkip = 0;

    void tips(const juce::File& f) {
        auto* tw = find<juce::TooltipWindow>(window);
        auto& lnf = tw ? tw->getLookAndFeel() : window.getLookAndFeel();
        auto* bs = dynamic_cast<LookAndFeel_BlockShuffler*>(&lnf);
        juce::String out;
        const auto area = window.getScreenBounds();
        std::function<void(juce::Component&)> walk = [&](juce::Component& c) {
            if (c.isShowing())
                if (auto* tc = dynamic_cast<juce::TooltipClient*>(&c)) {
                    const auto tip = tc->getTooltip();
                    if (tip.isNotEmpty()) {
                        bool ok = bs != nullptr; juce::String why = bs ? "" : " LnF-not-BlockShuffler";
                        int lines = 0; juce::Rectangle<int> box;
                        if (bs) {
                            const auto tl = LookAndFeel_BlockShuffler::tooltipLayout(tip);
                            lines = tl.getNumLines();
                            // worst case: pointer at the far corner of the window
                            for (auto pt : { area.getTopLeft(), area.getBottomRight() }) {
                                box = lnf.getTooltipBounds(tip, pt, area);
                                if (tl.getWidth()  > box.getWidth()  - 2 * LookAndFeel_BlockShuffler::tooltipPadX + 0.5f
                                 || tl.getHeight() > box.getHeight() - 2 * LookAndFeel_BlockShuffler::tooltipPadY + 0.5f)
                                    { ok = false; why << " CLIPPED"; }
                                if (!area.contains(box)) { ok = false; why << " OFFSCREEN"; }
                            }
                            int chars = 0;
                            for (int i = 0; i < lines; ++i) chars += tl.getLine(i).runs.isEmpty() ? 0
                                         : tl.getLine(i).stringRange.getLength();
                            if (chars < tip.length() - lines) { ok = false; why << " TRUNCATED"; }   // wrap may eat 1 space/line
                        }
                        if (tip.contains(juce::String::fromUTF8("\xe2\x80\xa6")) || tip.contains("...")) { ok = false; why << " ELLIPSIS"; }
                        out << (ok ? "PASS " : "FAIL ") << lines << " line(s) " << box.getWidth() << "x" << box.getHeight()
                            << why << " | " << tip.replace("\n", " ").substring(0, 70) << "\n";
                    }
                }
            for (auto* ch : c.getChildren()) walk(*ch);
        };
        walk(window);
        f.appendText(out);
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
                << " keepOut(banner) " << r2s(ov->keepOutRect.toFloat())
                << " bannerVisible " << (s->modeLabel.isVisible() ? "y " : "n ") << r2s(s->modeLabel.getBounds().toFloat())
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
