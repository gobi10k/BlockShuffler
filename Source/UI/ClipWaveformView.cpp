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
    nameLabel.setFont(LookAndFeel_BlockShuffler::uiFontBold(12.0f));
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
    setOpaque(true);  // paint() covers every pixel; skip alpha-compositing path
}

void ClipRowComponent::setSelected(bool sel) {
    if (selected == sel) return;
    selected = sel;
    repaint();
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
        g.setFont(LookAndFeel_BlockShuffler::uiFont(11.0f));
        g.drawText("No audio loaded", area, juce::Justification::centred);
        return;
    }

    const auto& buf = *(clip->audioBuffer);
    const int numSamples  = buf.getNumSamples();
    const int numChannels = buf.getNumChannels();
    if (numSamples == 0 || numChannels == 0) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
        g.setFont(LookAndFeel_BlockShuffler::uiFont(11.0f));
        g.drawText("No audio loaded", area, juce::Justification::centred);
        return;
    }

    const int w      = area.getWidth();
    const int cy     = area.getCentreY();
    const int halfH  = area.getHeight() / 2;
    const auto waveCol = juce::Colour(LookAndFeel_BlockShuffler::waveformFill);

    // Compute min/max for each pixel column in one pass
    juce::HeapBlock<int> peakY(w), troughY(w);
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
        peakY[px]   = y0;
        troughY[px] = y1;
    }

    // Fill pass — outline path traced peak→trough, filled at low alpha
    juce::Path fillPath;
    fillPath.startNewSubPath((float)area.getX(), (float)cy);
    for (int px = 0; px < w; ++px)
        fillPath.lineTo((float)(area.getX() + px), (float)peakY[px]);
    for (int px = w - 1; px >= 0; --px)
        fillPath.lineTo((float)(area.getX() + px), (float)troughY[px]);
    fillPath.closeSubPath();
    g.setColour(waveCol.withAlpha(0.18f));
    g.fillPath(fillPath);

    // Line pass — 1px peak-to-trough at full opacity
    g.setColour(waveCol);
    for (int px = 0; px < w; ++px)
        g.drawLine((float)(area.getX() + px), (float)peakY[px],
                   (float)(area.getX() + px), (float)troughY[px]);
}

void ClipRowComponent::paint(juce::Graphics& g) {
    if (!clip) return;
    // Background is always the neutral panel colour — selection is shown via border only
    // so the clip's header colour is never tinted by the accent.
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgMedium));

    // Header — clip's own colour, fully opaque
    auto headerRect = getLocalBounds().removeFromTop(headerH);
    g.setColour(clip->color);
    g.fillRect(headerRect);

    // Pick black or white text depending on header luminance so it's always readable
    float lum = clip->color.getFloatRed()   * 0.299f
              + clip->color.getFloatGreen() * 0.587f
              + clip->color.getFloatBlue()  * 0.114f;
    auto headerTextCol = (lum > 0.55f) ? juce::Colours::black : juce::Colours::white;
    nameLabel.setColour(juce::Label::textColourId, headerTextCol);

    // Raw probability weight displayed in the header pill.
    // Effective (normalized) probability is shown in the inspector's label when the clip is selected.
    juce::String probText = juce::String((int)(clip->probability * 100.0f)) + "%";
    {
        auto pillFont  = LookAndFeel_BlockShuffler::monoFont(10.5f);
        float pillW    = LookAndFeel_BlockShuffler::measureTextWidth(pillFont, probText) + 10.0f;
        float pillH    = 14.0f;
        float pillX    = (float)headerRect.getRight() - pillW - 4.0f;
        float pillY    = (float)headerRect.getCentreY() - pillH * 0.5f;
        auto  pillRect = juce::Rectangle<float>(pillX, pillY, pillW, pillH);
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.fillRoundedRectangle(pillRect, 3.5f);
        g.setFont(pillFont);
        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.drawText(probText, pillRect.toNearestInt(), juce::Justification::centred);
    }

    // Waveform
    auto wa = waveArea();
    renderWaveform(g, wa);

    // Grid lines — adaptive density: coarsen grid until lines are >= 8px apart
    if (clip->tempo > 0.0 && projectSampleRate > 0.0) {
        int64_t total = (clip->audioBuffer) ? (int64_t)clip->audioBuffer->getNumSamples() : 0;
        double spb = (projectSampleRate * 60.0) / clip->tempo;
        if (total > 0 && wa.getWidth() > 0 && spb > 0.0) {
            double pixelsPerSample = (double)wa.getWidth() / (double)total;
            double drawSpb = spb;
            double pixelsPerLine = drawSpb * pixelsPerSample;
            while (pixelsPerLine < 8.0 && drawSpb < (double)total) {
                drawSpb *= 2.0;
                pixelsPerLine *= 2.0;
            }
            if (pixelsPerLine >= 8.0) {
                float alpha = juce::jmap((float)pixelsPerLine, 8.0f, 40.0f, 0.15f, 0.40f);
                g.setColour(juce::Colour(LookAndFeel_BlockShuffler::borderStrong).withAlpha(alpha));
                double offset = std::fmod((double)clip->gridOffsetSamples, drawSpb);
                if (offset < 0.0) offset += drawSpb;
                for (double s = offset; s < (double)total; s += drawSpb) {
                    int gx = sampleToX((int64_t)s);
                    if (gx >= wa.getX() && gx < wa.getRight())
                        g.drawLine((float)gx, (float)wa.getY(),
                                   (float)gx, (float)wa.getBottom(), 1.5f);
                }
            }
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

    // Done overlay — semi-transparent dark + strikethrough + "DONE" label
    if (clip->isDone) {
        auto wa2 = waveArea();
        g.setColour(juce::Colour(0x88000000));
        g.fillRect(wa2);
        g.setColour(juce::Colour(0xAAFF4444));
        g.drawLine((float)wa2.getX(), (float)wa2.getCentreY(),
                   (float)wa2.getRight(), (float)wa2.getCentreY(), 2.0f);
        g.setColour(juce::Colour(0xCCFFFFFF));
        g.setFont(LookAndFeel_BlockShuffler::uiFontBold(13.0f));
        g.drawText("DONE", wa2, juce::Justification::centred);
    }

    // Border: 2px accent for selected, 1px borderSubtle separator otherwise
    if (selected) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol));
        g.drawRect(getLocalBounds(), 2);
    } else {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::borderSubtle));
        g.drawRect(getLocalBounds(), 1);
    }
}

void ClipRowComponent::resized() {
    auto hdr = getLocalBounds().removeFromTop(headerH);
    // Leave right 44px for the probability label (drawn in paint)
    nameLabel.setBounds(hdr.withTrimmedLeft(5).withTrimmedRight(72));
}

void ClipRowComponent::mouseDown(const juce::MouseEvent& e) {
    // isPopupMenu() covers both physical right-click AND Control+click on macOS
    if (e.mods.isPopupMenu()) {
        showContextMenu();
        return;
    }
    if (onSelectedCallback) onSelectedCallback();

    if (!clip) { activeDrag = DragTarget::None; return; }

    auto wa = waveArea();
    if (!wa.contains(e.x, e.y)) {
        // Header area: arm for a potential clip drag
        activeDrag = DragTarget::Clip;
        clipDragStartX = e.x;
        clipDragStartY = e.y;
        return;
    }

    // Wave area: check for marker hit
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
    if (activeDrag == DragTarget::Clip) {
        int dx = e.x - clipDragStartX;
        int dy = e.y - clipDragStartY;
        if (std::abs(dx) + std::abs(dy) > 5 && clip) {
            if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor(this))
                dc->startDragging("clip:" + clip->id, this);
            activeDrag = DragTarget::None;
        }
        return;
    }

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
    menu.addItem(5, "Play Clip");
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
        } else if (result == 5 && self->clip) {
            if (self->onPlayClipRequested) self->onPlayClipRequested(self->clip->id);
        }
    });
}

//==============================================================================
// ClipWaveformView
//==============================================================================

ClipWaveformView::ClipWaveformView() {
    setOpaque(true);  // ensures paint() runs first so transparent children show bgDark
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
        zoomFactor  = juce::jlimit(1.0f, computeMaxZoom(), zoomFactor * delta);
        juce::Component::SafePointer<ClipWaveformView> safeThis(this);
        juce::MessageManager::callAsync(
            [safeThis] {
                if (safeThis) safeThis->resized();
            });
    };
    addAndMakeVisible(viewport);

    addClipBtn.onClick = [this] { browseForClip(); };
    addAndMakeVisible(addClipBtn);

    auto applyZoom = [this] {
        ClipWaveformView* self = this;
        juce::MessageManager::callAsync(
            [safe = juce::Component::SafePointer<ClipWaveformView>(self)] {
                if (safe) safe->resized();
            });
    };
    zoomInBtn .onClick = [this, applyZoom] { zoomFactor = juce::jlimit(1.0f, computeMaxZoom(), zoomFactor * 1.5f); applyZoom(); };
    zoomOutBtn.onClick = [this, applyZoom] { zoomFactor = juce::jlimit(1.0f, computeMaxZoom(), zoomFactor / 1.5f); applyZoom(); };
    zoomFitBtn.onClick = [this, applyZoom] { zoomFactor = 1.0f; applyZoom(); };
    zoomInBtn .setTooltip("Zoom in  [Cmd+scroll]");
    zoomOutBtn.setTooltip("Zoom out  [Cmd+scroll]");
    zoomFitBtn.setTooltip("Reset zoom to fit");
    // Tag buttons so LookAndFeel can draw them as a joined segmented control
    zoomOutBtn.getProperties().set("segmentPos", "left");
    zoomFitBtn.getProperties().set("segmentPos", "middle");
    zoomInBtn .getProperties().set("segmentPos", "right");
    addAndMakeVisible(zoomInBtn);
    addAndMakeVisible(zoomOutBtn);
    addAndMakeVisible(zoomFitBtn);
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
    juce::Component::SafePointer<ClipWaveformView> safe(this);
    juce::MessageManager::callAsync(
        [safe] {
            if (safe) { safe->rebuildRows(); safe->resized(); safe->repaint(); }
        });
}

void ClipWaveformView::rebuildRows() {
    contentArea.removeAllChildren();
    clipRows.clear();
    if (!currentBlock) return;

    if (selectedClip != nullptr) {
        bool stillHere = false;
        for (auto* c : currentBlock->clips)
            if (c == selectedClip) { stillHere = true; break; }
        if (!stillHere) {
            selectedClip = nullptr;
            if (onClipSelected) onClipSelected(nullptr);
        }
    }

    // Pre-compute total weight for effective-probability tooltip
    float totalWeight = 0.0f;
    for (auto* c : currentBlock->clips)
        totalWeight += c->probability;

    for (auto* clipPtr : currentBlock->clips) {
        auto* row = clipRows.add(new ClipRowComponent(
            *clipPtr,
            projectSampleRate,
            [this, clipPtr] { selectClip(clipPtr); },
            [this]       { repaint(); },
            [this, clipPtr] { removeClip(clipPtr); }
        ));
        row->onCaptureSnapshot   = onCaptureSnapshot;
        row->onUndoableMutation  = onUndoableMutation;
        row->onPlayClipRequested = onPlayClipRequested;
        row->ownerBlock = currentBlock.get();
        row->setSelected(clipPtr == selectedClip);

        float rawPct = clipPtr->probability * 100.0f;
        float effPct = (totalWeight > 0.0f)
                     ? (clipPtr->probability / totalWeight) * 100.0f
                     : 100.0f / (float)currentBlock->clips.size();
        row->setTooltip(clipPtr->name
            + " | Weight: " + juce::String((int)rawPct) + "%"
            + " | Effective: " + juce::String(effPct, 1) + "%");

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
                        double blockT = currentBlock ? currentBlock->tempo : 0.0;
                        clipPtr->tempo = (blockT > 0.0) ? blockT
                                       : (defaultTempo > 0.0 ? defaultTempo : 120.0);
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
        // Repaint all clip rows to reflect the offset change
        for (auto* row : clipRows)
            row->repaint();
        repaint();
        return true;
    }

    return false;
}

void ClipWaveformView::setPlayingClip(const juce::String& clipId, int64_t samplePos, double sampleRate) {
    bool changed = false;
    if (playingClipId != clipId) {
        playingClipId = clipId;
        changed = true;

        // Scroll the playing clip's row into view WITHOUT changing the user's selection.
        // The playhead (red line in paintOverChildren) is the only visual playing indicator.
        // Do NOT change selectedClip or any row's selected state here — that would override
        // the clip the user explicitly clicked, switching the inspector to the playing clip.
        if (!clipId.isEmpty()) {
            for (int i = 0; i < clipRows.size(); ++i) {
                auto* row = clipRows[i];
                auto* clip = row->getClip();
                if (clip != nullptr && !clip->id.isEmpty() && clip->id == clipId) {
                    juce::MessageManager::callAsync([this, rowIndex = i]() {
                        if (rowIndex >= 0 && rowIndex < clipRows.size()) {
                            auto rowBounds = clipRows[rowIndex]->getBounds();
                            int visH = viewport.getMaximumVisibleHeight();
                            int viewH = contentArea.getHeight();
                            int targetY = juce::jlimit(0, juce::jmax(0, viewH - visH),
                                                      rowBounds.getY() - visH / 2 + rowH / 2);
                            viewport.setViewPosition(0, targetY);
                        }
                    });
                    break;
                }
            }
        }
    }
    if (playingSamplePos != samplePos) {
        playingSamplePos = samplePos;
        changed = true;
    }
    if (playingSampleRate != sampleRate) {
        playingSampleRate = sampleRate;
        changed = true;
    }
    if (changed) repaint();
}

void ClipWaveformView::paintOverChildren(juce::Graphics& g) {
    if (playingClipId.isEmpty()) return;

    for (int i = 0; i < clipRows.size(); ++i) {
        auto* row = clipRows[i];
        auto* clip = row->getClip();
        if (clip == nullptr || clip->id.isEmpty() || clip->id != playingClipId) continue;

        if (clip->audioBuffer == nullptr || clip->audioBuffer->getNumSamples() == 0) return;
        int64_t totalSamples = clip->audioBuffer->getNumSamples();
        if (totalSamples <= 0) return;

        double fraction = (double)playingSamplePos / (double)totalSamples;
        fraction = juce::jlimit(0.0, 1.0, fraction);

        // Row bounds are in contentArea-local space; convert to ClipWaveformView space.
        // Viewport sits at (vpX, vpY) in our space; contentArea is offset by the scroll.
        const int scrollX = viewport.getViewPositionX();
        const int scrollY = viewport.getViewPositionY();
        const int vpX     = viewport.getX();
        const int vpY     = viewport.getY();

        auto rowBounds = row->getBounds();   // contentArea coords
        auto wa        = row->waveArea();    // row-local coords (row.getX()==0 so same as contentArea)

        // Playhead X in ClipWaveformView space
        int waveX = vpX - scrollX + wa.getX() + (int)(fraction * wa.getWidth());

        // Viewport clip bounds in ClipWaveformView space
        const int vpLeft   = vpX;
        const int vpRight  = viewport.getRight();
        const int vpTop    = vpY;
        const int vpBottom = viewport.getBottom();

        if (waveX < vpLeft || waveX > vpRight) break;

        // Row Y range in ClipWaveformView space
        int rowTop    = vpY - scrollY + rowBounds.getY();
        int rowBottom = vpY - scrollY + rowBounds.getBottom();

        int drawTop    = juce::jmax(vpTop,    rowTop);
        int drawBottom = juce::jmin(vpBottom, rowBottom);

        if (drawBottom <= drawTop) break;

        // Glow pass — wide soft halo
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::playheadCol).withAlpha(0.18f));
        g.drawLine((float)waveX, (float)drawTop, (float)waveX, (float)drawBottom, 5.0f);
        // Sharp pass — 1px crisp line at full opacity
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::playheadCol));
        g.drawLine((float)waveX, (float)drawTop, (float)waveX, (float)drawBottom, 1.0f);

        juce::Path tri;
        tri.addTriangle((float)waveX - 5, (float)drawTop,
                        (float)waveX + 5, (float)drawTop,
                        (float)waveX,     (float)(drawTop + 10));
        g.fillPath(tri);

        break;
    }
}

void ClipWaveformView::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgDark));
    if (!currentBlock || currentBlock->clips.isEmpty()) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
        g.setFont(LookAndFeel_BlockShuffler::uiFont(14.0f));
        g.drawText(currentBlock ? "Drop audio files here or click  \"+Add Clip\""
                                : "Select a block to view clips",
                   getLocalBounds().withTrimmedBottom(btnH + 4),
                   juce::Justification::centred);
    }
}

void ClipWaveformView::resized() {
    auto area = getLocalBounds();
    addClipBtn.setBounds(area.removeFromBottom(btnH).reduced(4, 2));

    auto zoomBar = area.removeFromBottom(zoomBarH).reduced(4, 2);
    zoomOutBtn.setBounds(zoomBar.removeFromLeft(24));
    zoomFitBtn.setBounds(zoomBar.removeFromLeft(36));
    zoomInBtn .setBounds(zoomBar.removeFromLeft(24));

    viewport.setBounds(area);

    int visW   = juce::jmax(viewport.getMaximumVisibleWidth(), 1);
    int rowW   = (int)(visW * zoomFactor);
    int totalH = juce::jmax((int)clipRows.size() * (rowH + rowGap),
                            juce::jmax(viewport.getMaximumVisibleHeight(), 1));
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
        zoomFactor  = juce::jlimit(1.0f, computeMaxZoom(), zoomFactor * delta);
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

float ClipWaveformView::computeMaxZoom() const {
    // Find the longest clip in the current block.
    // Max zoom is chosen so the waveform shows ~0.5 seconds at full zoom.
    double maxDurationSeconds = 0.0;
    const double sr = projectSampleRate > 0.0 ? projectSampleRate : 48000.0;

    if (currentBlock) {
        for (auto* clip : currentBlock->clips) {
            if (clip->audioBuffer) {
                double dur = (double)clip->audioBuffer->getNumSamples() / sr;
                if (dur > maxDurationSeconds) maxDurationSeconds = dur;
            }
        }
    }

    if (maxDurationSeconds < 0.001) return 32.0f; // fallback for empty/tiny clips

    double maxZoom = maxDurationSeconds / 0.5;     // shows ~0.5 s at full zoom
    maxZoom = juce::jmax(maxZoom, 1.0);
    maxZoom = juce::jmin(maxZoom, 256.0);
    return (float)maxZoom;
}

} // namespace BlockShuffler
