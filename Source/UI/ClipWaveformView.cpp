#include "ClipWaveformView.h"
#include "../Utils/GridSnap.h"
#include <cmath>

namespace BlockShuffler {

//==============================================================================
// ClipRowComponent
//==============================================================================

ClipRowComponent::ClipRowComponent(Clip& c,
                                   double psr,
                                   std::function<void()> onSel,
                                   std::function<void()> onRepaint,
                                   std::function<void()> onRemove)
    : clip(&c),
      projectSampleRate(psr),
      onSelectedCallback(std::move(onSel)),
      onRepaintCallback(std::move(onRepaint)),
      onRemoveCallback(std::move(onRemove))
{
    nameLabel.setText(clip ? clip->name : "", juce::dontSendNotification);
    nameLabel.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    nameLabel.setColour(juce::Label::textColourId,
                        juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
    nameLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    nameLabel.setEditable(false, true, false); // double-click editable
    nameLabel.onTextChange = [this] {
        if (clip) clip->name = nameLabel.getText();
        if (onRepaintCallback) onRepaintCallback();
        // Record rename as undoable (pre was captured when the editor was opened)
        if (onUndoableMutation && nameLabelEditPre.isObject()) {
            onUndoableMutation(nameLabelEditPre);
            nameLabelEditPre = juce::var{};
        }
    };
    // Don't intercept mouse events — let them fall through to ClipRowComponent
    // so right-click anywhere (including on the name label) opens the context menu.
    nameLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(nameLabel);
    setInterceptsMouseClicks(true, true);
}

void ClipRowComponent::setSelected(bool sel) {
    if (selected == sel) return;
    selected = sel;
    repaint();
}

juce::Rectangle<int> ClipRowComponent::waveArea() const {
    return getLocalBounds().withTrimmedTop(headerH).reduced(2, 2);
}

int ClipRowComponent::sampleToX(int64_t sample) const {
    auto wa     = waveArea();
    if (!clip || !clip->audioBuffer) return wa.getX();
    int64_t tot = (int64_t)clip->audioBuffer->getNumSamples();
    if (tot <= 0 || wa.getWidth() <= 0) return wa.getX();
    return wa.getX() + (int)((double)sample / (double)tot * wa.getWidth());
}

int64_t ClipRowComponent::xToSample(int x) const {
    auto wa     = waveArea();
    if (!clip || !clip->audioBuffer) return 0;
    int64_t tot = (int64_t)clip->audioBuffer->getNumSamples();
    if (wa.getWidth() <= 0 || tot <= 0) return 0;
    double t = (double)(x - wa.getX()) / (double)wa.getWidth();
    return (int64_t)(juce::jlimit(0.0, 1.0, t) * (double)tot);
}

void ClipRowComponent::renderWaveform(juce::Graphics& g,
                                       juce::Rectangle<int> area) const {
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    g.fillRect(area);

    if (!clip || !clip->audioBuffer) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("No audio loaded", area, juce::Justification::centred);
        return;
    }

    const auto& buf = *(clip->audioBuffer);
    const int numSamples  = buf.getNumSamples();
    const int numChannels = buf.getNumChannels();
    if (numSamples == 0 || numChannels == 0) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("No audio loaded", area, juce::Justification::centred);
        return;
    }

    const int w  = area.getWidth();
    const int cy = area.getCentreY();
    const int halfH = area.getHeight() / 2;

    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::waveformFill));
    for (int px = 0; px < w; ++px) {
        int s0 = (int)((int64_t)px * numSamples / w);
        int s1 = juce::jmin((int)((int64_t)(px + 1) * numSamples / w), numSamples - 1);
        if (s1 < s0) s1 = s0;
        float mn = 0.0f, mx = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            const float* data = buf.getReadPointer(ch);
            for (int s = s0; s <= s1; ++s) {
                mn = juce::jmin(mn, data[s]);
                mx = juce::jmax(mx, data[s]);
            }
        }
        int y0 = cy - (int)(mx * (float)halfH);
        int y1 = cy - (int)(mn * (float)halfH);
        if (y0 > y1) std::swap(y0, y1);
        if (y0 == y1) ++y1;
        g.drawLine((float)(area.getX() + px), (float)y0,
                   (float)(area.getX() + px), (float)y1);
    }
}

void ClipRowComponent::paint(juce::Graphics& g) {
    if (!clip) return;
    auto bg = selected
        ? juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.18f)
        : juce::Colour(LookAndFeel_BlockShuffler::bgMedium);
    g.fillAll(bg);

    // Header
    auto headerRect = getLocalBounds().removeFromTop(headerH);
    g.setColour(clip->color.withAlpha(0.85f));
    g.fillRect(headerRect);

    // Pick black or white text depending on header luminance so it's always readable
    float lum = clip->color.getFloatRed()   * 0.299f
              + clip->color.getFloatGreen() * 0.587f
              + clip->color.getFloatBlue()  * 0.114f;
    auto headerTextCol = (lum > 0.55f) ? juce::Colours::black : juce::Colours::white;
    nameLabel.setColour(juce::Label::textColourId, headerTextCol);

    // Effective (normalized) probability on right of header
    juce::String probText;
    if (clip->isDone) {
        probText = "excl.";
    } else if (ownerBlock) {
        float totalWeight = 0.0f;
        for (auto* c : ownerBlock->clips)
            if (!c->isDone) totalWeight += c->probability;
        float eff = (totalWeight > 0.0f) ? (clip->probability / totalWeight) * 100.0f : 0.0f;
        probText = "eff: " + juce::String(eff, 1) + "%";
    } else {
        probText = juce::String((int)(clip->probability * 100.0f)) + "%";
    }
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.setColour(headerTextCol);
    g.drawText(probText, headerRect.withTrimmedRight(4), juce::Justification::centredRight);

    // Waveform
    auto wa = waveArea();
    renderWaveform(g, wa);

    // Grid lines — offset by gridOffsetSamples so the nudge is visible
    if (clip->tempo > 0.0 && projectSampleRate > 0.0) {
        double spb   = (projectSampleRate * 60.0) / clip->tempo;
        int64_t total = (clip->audioBuffer) ? (int64_t)clip->audioBuffer->getNumSamples() : 0;
        // Wrap the offset into [0, spb) so lines start at the right phase
        double offset = std::fmod((double)clip->gridOffsetSamples, spb);
        if (offset < 0.0) offset += spb;
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::gridLineColor));
        for (double s = offset; s < (double)total; s += spb) {
            int gx = sampleToX((int64_t)s);
            if (gx >= wa.getX() && gx < wa.getRight())
                g.drawLine((float)gx, (float)wa.getY(),
                           (float)gx, (float)wa.getBottom(), 1.0f);
        }
    }

    // Lead-in dim
    {
        int sx = sampleToX(clip->startMark);
        auto r = juce::Rectangle<int>(wa.getX(), wa.getY(),
                                      juce::jmax(0, sx - wa.getX()), wa.getHeight());
        if (r.getWidth() > 0) { g.setColour(juce::Colours::black.withAlpha(0.38f)); g.fillRect(r); }
    }
    // Tail dim
    {
        int ex = sampleToX(clip->endMark);
        auto r = juce::Rectangle<int>(ex, wa.getY(), juce::jmax(0, wa.getRight() - ex), wa.getHeight());
        if (r.getWidth() > 0) { g.setColour(juce::Colours::black.withAlpha(0.38f)); g.fillRect(r); }
    }

    // Start marker
    {
        int sx = sampleToX(clip->startMark);
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::startMarkerCol));
        g.drawLine((float)sx, (float)wa.getY(), (float)sx, (float)wa.getBottom(), 2.0f);
        juce::Path tri;
        tri.addTriangle((float)sx-5, (float)wa.getY(), (float)sx+5, (float)wa.getY(),
                        (float)sx, (float)(wa.getY()+10));
        g.fillPath(tri);
    }
    // End marker
    {
        int ex = sampleToX(clip->endMark);
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::endMarkerCol));
        g.drawLine((float)ex, (float)wa.getY(), (float)ex, (float)wa.getBottom(), 2.0f);
        juce::Path tri;
        tri.addTriangle((float)ex-5, (float)wa.getBottom(), (float)ex+5, (float)wa.getBottom(),
                        (float)ex, (float)(wa.getBottom()-10));
        g.fillPath(tri);
    }

    g.setColour(selected ? juce::Colour(LookAndFeel_BlockShuffler::accentCol)
                         : juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    g.drawRect(getLocalBounds());
}

void ClipRowComponent::resized() {
    auto hdr = getLocalBounds().removeFromTop(headerH);
    // Leave right 44px for the probability label (drawn in paint)
    nameLabel.setBounds(hdr.withTrimmedLeft(5).withTrimmedRight(44));
}

void ClipRowComponent::mouseDown(const juce::MouseEvent& e) {
    // isPopupMenu() covers both physical right-click AND Control+click on macOS
    if (e.mods.isPopupMenu()) {
        showContextMenu();
        return;
    }
    if (onSelectedCallback) onSelectedCallback();

    // Check marker hit in wave area
    auto wa = waveArea();
    if (!wa.contains(e.x, e.y) || !clip) { activeDrag = DragTarget::None; return; }
    int sx = sampleToX(clip->startMark);
    int ex = sampleToX(clip->endMark);
    if (std::abs(e.x - sx) <= markerHit)      activeDrag = DragTarget::StartMarker;
    else if (std::abs(e.x - ex) <= markerHit) activeDrag = DragTarget::EndMarker;
    else                                        activeDrag = DragTarget::None;

    // Capture pre-state so mouseUp can record the marker drag as undoable
    if (activeDrag != DragTarget::None && onCaptureSnapshot)
        markerDragPre = onCaptureSnapshot();
}

void ClipRowComponent::mouseDoubleClick(const juce::MouseEvent& e) {
    if (e.y < headerH) {
        // Capture pre-state before the name editor opens so onTextChange can record undo
        if (onCaptureSnapshot) nameLabelEditPre = onCaptureSnapshot();
        nameLabel.showEditor();
    }
}

void ClipRowComponent::mouseDrag(const juce::MouseEvent& e) {
    if (activeDrag == DragTarget::None) return;

    // Snap to tempo grid unless Shift is held (Shift = free drag)
    auto applySnap = [&](int64_t raw) -> int64_t {
        if (e.mods.isShiftDown() || !clip) return raw;
        if (clip->tempo > 0.0 && projectSampleRate > 0.0)
            return snapToGrid(raw, clip->tempo, projectSampleRate);
        return raw;
    };

    if (activeDrag == DragTarget::StartMarker && clip) {
        int64_t np = applySnap(xToSample(e.x));
        clip->startMark = juce::jlimit((int64_t)0, clip->endMark - 1, np);
        repaint();
        if (onRepaintCallback) onRepaintCallback();
    } else if (activeDrag == DragTarget::EndMarker && clip) {
        int64_t tot = (clip->audioBuffer) ? (int64_t)clip->audioBuffer->getNumSamples() : 0;
        int64_t np  = applySnap(xToSample(e.x));
        clip->endMark = juce::jlimit(clip->startMark + 1, tot, np);
        repaint();
        if (onRepaintCallback) onRepaintCallback();
    }
}

void ClipRowComponent::mouseUp(const juce::MouseEvent&) {
    if (activeDrag != DragTarget::None && onUndoableMutation && markerDragPre.isObject()) {
        onUndoableMutation(markerDragPre);
        markerDragPre = juce::var{};
    }
    activeDrag = DragTarget::None;
}

void ClipRowComponent::showContextMenu() {
    auto palette = LookAndFeel_BlockShuffler::getBlockPalette();
    juce::StringArray colourNames { "Red","Orange","Yellow","Green",
                                    "Cyan","Blue","Purple","Pink" };
    juce::PopupMenu colourMenu;
    for (int i = 0; i < palette.size() && i < colourNames.size(); ++i)
        colourMenu.addItem(30 + i, colourNames[i], true, clip && clip->color == palette[i]);

    juce::PopupMenu menu;
    menu.addItem(1, "Rename");
    menu.addSubMenu("Set Color", colourMenu);
    menu.addSeparator();
    menu.addItem(2, "Song Ender",  true, clip && clip->isSongEnder);
    menu.addItem(3, "Mark as Done", true, clip && clip->isDone);
    menu.addSeparator();
    menu.addItem(4, "Remove Clip");

    // Capture pre-state now (before the async callback) so mutations can be undone
    juce::var pre = onCaptureSnapshot ? onCaptureSnapshot() : juce::var{};

    // Use a SafePointer so the async callback doesn't access a destroyed component
    juce::Component::SafePointer<ClipRowComponent> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options{},
                       [safeThis, palette, pre](int result) {
        if (!safeThis) return;
        auto* self = safeThis.getComponent();
        if (result == 1) {
            // Rename: capture pre for the editor-commit callback
            if (self->onCaptureSnapshot) self->nameLabelEditPre = self->onCaptureSnapshot();
            self->nameLabel.showEditor();
        } else if (result >= 30 && result < 30 + palette.size() && self->clip) {
            self->clip->color = palette[result - 30];
            self->repaint();
            if (self->onRepaintCallback) self->onRepaintCallback();
            if (self->onUndoableMutation && pre.isObject()) self->onUndoableMutation(pre);
        } else if (result == 2 && self->clip) {
            self->clip->isSongEnder = !self->clip->isSongEnder;
            self->repaint();
            if (self->onRepaintCallback) self->onRepaintCallback();
            if (self->onUndoableMutation && pre.isObject()) self->onUndoableMutation(pre);
        } else if (result == 3 && self->clip) {
            self->clip->isDone = !self->clip->isDone;
            self->repaint();
            if (self->onRepaintCallback) self->onRepaintCallback();
            if (self->onUndoableMutation && pre.isObject()) self->onUndoableMutation(pre);
        } else if (result == 4) {
            if (self->onRemoveCallback) self->onRemoveCallback();
        }
    });
}

//==============================================================================
// ClipWaveformView
//==============================================================================

ClipWaveformView::ClipWaveformView() {
    setWantsKeyboardFocus(true);

    viewport.setViewedComponent(&contentArea, false);
    viewport.setScrollBarsShown(true, true);
    viewport.setScrollBarThickness(8);
    // Prevent the viewport from stealing keyboard focus away from ClipWaveformView.
    // Without this, clicking a clip row calls grabKeyboardFocus() here but the viewport
    // immediately reclaims focus and consumes arrow keys for scrolling instead of nudging.
    viewport.setWantsKeyboardFocus(false);
    // Route Cmd+scroll from the viewport subclass to our zoom logic.
    viewport.onZoomScroll = [this](float deltaY) {
        float delta = deltaY > 0 ? 1.25f : 0.8f;
        zoomFactor  = juce::jlimit(1.0f, 32.0f, zoomFactor * delta);
        juce::Component::SafePointer<ClipWaveformView> safeThis(this);
        juce::MessageManager::callAsync(
            [safeThis] {
                if (safeThis) safeThis->resized();
            });
    };
    addAndMakeVisible(viewport);

    addClipBtn.onClick = [this] { browseForClip(); };
    addAndMakeVisible(addClipBtn);
}

ClipWaveformView::~ClipWaveformView() {
    if (currentBlock) currentBlock->removeChangeListener(this);
}

void ClipWaveformView::setBlock(Block* block, double sampleRate, juce::AudioFormatManager* fmtMgr) {
    if (currentBlock) currentBlock->removeChangeListener(this);
    currentBlock  = block;
    projectSampleRate = sampleRate;
    selectedClip  = nullptr;
    if (fmtMgr) formatManager = fmtMgr;
    if (currentBlock) currentBlock->addChangeListener(this);
    rebuildRows();
    resized();
    repaint();
}

void ClipWaveformView::changeListenerCallback(juce::ChangeBroadcaster*) {
    juce::MessageManager::callAsync(
        [safe = juce::Component::SafePointer<ClipWaveformView>(this)] {
            if (safe) { safe->rebuildRows(); safe->resized(); safe->repaint(); }
        });
}

void ClipWaveformView::rebuildRows() {
    contentArea.removeAllChildren();
    clipRows.clear();
    if (!currentBlock) return;

    for (auto* clipPtr : currentBlock->clips) {
        auto* row = clipRows.add(new ClipRowComponent(
            *clipPtr,
            projectSampleRate,
            [this, clipPtr] { selectClip(clipPtr); },
            [this]       { repaint(); },
            [this, clipPtr] { removeClip(clipPtr); }
        ));
        row->onCaptureSnapshot  = onCaptureSnapshot;
        row->onUndoableMutation = onUndoableMutation;
        row->ownerBlock = currentBlock.get();
        row->setSelected(clipPtr == selectedClip);
        contentArea.addAndMakeVisible(row);
    }
}

void ClipWaveformView::selectClip(Clip* clipPtr) {
    selectedClip = clipPtr;
    for (auto* row : clipRows)
        row->setSelected(row->getClip() == selectedClip);
    if (onClipSelected) onClipSelected(clipPtr);
    // Defer focus grab to the next message loop iteration. Grabbing inside a
    // mouseDown handler gets overridden by JUCE's post-click focus reassignment.
    juce::MessageManager::callAsync(
        [safe = juce::Component::SafePointer<ClipWaveformView>(this)] {
            if (safe) safe->grabKeyboardFocus();
        });
}

void ClipWaveformView::removeClip(Clip* clipPtr) {
    if (!currentBlock) return;
    for (int i = 0; i < currentBlock->clips.size(); ++i) {
        if (currentBlock->clips[i] == clipPtr) {
            if (selectedClip == clipPtr) {
                selectedClip = nullptr;
                if (onClipSelected) onClipSelected(nullptr);
            }
            juce::var pre = onCaptureSnapshot ? onCaptureSnapshot() : juce::var{};
            currentBlock->removeClip(i);  // fires changeMessage → rebuild
            if (onUndoableMutation && pre.isObject()) onUndoableMutation(pre);
            return;
        }
    }
}

void ClipWaveformView::browseForClip() {
    if (!currentBlock || !formatManager) return;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Add Audio Clip",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.mp3");

    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                         juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc) {
            auto results = fc.getResults();
            if (results.isEmpty() || !currentBlock) return;
            juce::var pre = onCaptureSnapshot ? onCaptureSnapshot() : juce::var{};
            bool anyAdded = false;
            for (auto& f : results) {
                if (f.existsAsFile()) {
                    auto clipPtr = std::make_unique<Clip>();
                    if (clipPtr->loadFromFile(f, *formatManager, projectSampleRate)) {
                        currentBlock->addClip(std::move(clipPtr));
                        anyAdded = true;
                    }
                }
            }
            if (anyAdded && onUndoableMutation && pre.isObject())
                onUndoableMutation(pre);
        });
}

bool ClipWaveformView::keyPressed(const juce::KeyPress& key) {
    if (!selectedClip) return false;

    if (key.getKeyCode() == juce::KeyPress::deleteKey ||
        key.getKeyCode() == juce::KeyPress::backspaceKey) {
        removeClip(selectedClip);
        return true;
    }

    // Arrow key nudge: ±1 grid unit based on clip tempo and sample rate
    const bool isLeft  = (key.getKeyCode() == juce::KeyPress::leftKey);
    const bool isRight = (key.getKeyCode() == juce::KeyPress::rightKey);
    if (isLeft || isRight) {
        const double sr    = projectSampleRate > 0.0 ? projectSampleRate : 48000.0;
        const double tempo = selectedClip->tempo > 0.0            ? selectedClip->tempo             : 120.0;
        // Use subdivision=4 (quarter-beat steps) so each press produces a visible
        // grid shift. Nudging by a full beat (subdivision=1) would fmod back to the
        // same pixel positions with no visual change.
        selectedClip->gridOffsetSamples = nudgeByGridUnits(
            selectedClip->gridOffsetSamples, isRight ? 1 : -1, tempo, sr, 4);
        DBG("Nudged grid offset to: " + juce::String(selectedClip->gridOffsetSamples));
        // Repaint all clip rows to reflect the offset change
        for (auto* row : clipRows)
            row->repaint();
        repaint();
        return true;
    }

    return false;
}

void ClipWaveformView::setPlayheadFraction(float fraction) {
    if (playheadFraction == fraction) return;
    playheadFraction = fraction;
    repaint();
}

void ClipWaveformView::paintOverChildren(juce::Graphics& g) {
    if (playheadFraction < 0.0f || playheadFraction > 1.0f) return;
    auto vp = viewport.getBounds();
    int x = vp.getX() + (int)(playheadFraction * (float)vp.getWidth());
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::playheadCol).withAlpha(0.85f));
    g.drawLine((float)x, (float)vp.getY(), (float)x, (float)vp.getBottom(), 2.0f);
    // Small triangle at top
    juce::Path tri;
    tri.addTriangle((float)x - 5, (float)vp.getY(),
                    (float)x + 5, (float)vp.getY(),
                    (float)x,     (float)(vp.getY() + 10));
    g.fillPath(tri);
}

void ClipWaveformView::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgDark));
    if (!currentBlock || currentBlock->clips.isEmpty()) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.drawText(currentBlock ? "Drop audio files here or click  \"+Add Clip\""
                                : "Select a block to view clips",
                   getLocalBounds().withTrimmedBottom(btnH + 4),
                   juce::Justification::centred);
    }
}

void ClipWaveformView::resized() {
    auto area = getLocalBounds();
    addClipBtn.setBounds(area.removeFromBottom(btnH).reduced(4, 2));
    viewport.setBounds(area);

    int visW   = juce::jmax(viewport.getMaximumVisibleWidth(), 1);
    int rowW   = (int)(visW * zoomFactor);
    int totalH = juce::jmax((int)clipRows.size() * (rowH + rowGap),
                            viewport.getMaximumVisibleHeight());
    contentArea.setBounds(0, 0, rowW, totalH);

    int y = 0;
    for (auto* row : clipRows) {
        row->setBounds(0, y, rowW, rowH);
        y += rowH + rowGap;
    }
}

void ClipWaveformView::mouseWheelMove(const juce::MouseEvent& e,
                                       const juce::MouseWheelDetails& w) {
    // Events that land directly on ClipWaveformView (outside the viewport area):
    // delegate zoom to shared logic, or scroll the viewport manually.
    if (e.mods.isCommandDown()) {
        float delta = w.deltaY > 0 ? 1.25f : 0.8f;
        zoomFactor  = juce::jlimit(1.0f, 32.0f, zoomFactor * delta);
        juce::MessageManager::callAsync(
            [safe = juce::Component::SafePointer<ClipWaveformView>(this)] {
                if (safe) safe->resized();
            });
    } else {
        auto pos = viewport.getViewPosition();
        int newY = juce::jlimit(
            0,
            juce::jmax(0, viewport.getViewedComponent()->getHeight() - viewport.getMaximumVisibleHeight()),
            pos.y - juce::roundToInt(w.deltaY * 100.0f));
        viewport.setViewPosition(pos.x, newY);
    }
}

} // namespace BlockShuffler
