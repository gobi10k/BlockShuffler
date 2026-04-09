#include "InspectorPanel.h"

namespace BlockShuffler {

// ── Helpers ───────────────────────────────────────────────────────────────────

static void setupLabel(juce::Component* parent, juce::Label& lbl,
                       const juce::String& text, float fontSize, bool dim = false)
{
    lbl.setText(text, juce::dontSendNotification);
    lbl.setFont(juce::Font(juce::FontOptions(fontSize)));
    lbl.setColour(juce::Label::textColourId,
                  dim ? juce::Colour(LookAndFeel_BlockShuffler::textSecondary)
                      : juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
    parent->addAndMakeVisible(lbl);
}

// ── Constructor ───────────────────────────────────────────────────────────────

InspectorPanel::InspectorPanel()
{
    // ── Clip section ─────────────────────────────────────────────────────────
    setupLabel(this, clipTitle,    "CLIP",           11.0f, true);
    setupLabel(this, probLabel,    "Probability (%)", 12.0f);
    setupLabel(this, tempoLabel,   "Tempo (BPM)",     12.0f);

    probSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    probSlider.setRange(0.0, 100.0, 1.0);
    probSlider.setValue(100.0, juce::dontSendNotification);
    probSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    probSlider.setColour(juce::Slider::textBoxTextColourId,
                         juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
    probSlider.setColour(juce::Slider::textBoxBackgroundColourId,
                         juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    probSlider.addListener(this);
    addAndMakeVisible(probSlider);

    effectiveProbLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    effectiveProbLabel.setColour(juce::Label::textColourId,
                                 juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
    effectiveProbLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(effectiveProbLabel);

    tempoField.onValueChanged = [this](double t) {
        if (selectedClip && !updatingFromModel && project) {
            if (t > 0.0 && std::abs(t - selectedClip->tempo) > 0.001) {
                auto pre = project->toJSON();
                selectedClip->tempo = t;
                project->applyExternalMutation(pre);
            }
        }
    };
    addAndMakeVisible(tempoField);

    retainLeadIn.addListener(this);   addAndMakeVisible(retainLeadIn);
    retainTail  .addListener(this);   addAndMakeVisible(retainTail);
    songEnderToggle.addListener(this); addAndMakeVisible(songEnderToggle);
    clipDoneToggle .addListener(this); addAndMakeVisible(clipDoneToggle);

    // ── Block section ────────────────────────────────────────────────────────
    setupLabel(this, blockTitle,   "BLOCK",                   11.0f, true);
    setupLabel(this, overlapLabel, "Overlap Probability (%)", 12.0f);

    blockDoneToggle.addListener(this);
    addAndMakeVisible(blockDoneToggle);

    overlapSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    overlapSlider.setRange(0.0, 100.0, 1.0);
    overlapSlider.setValue(50.0, juce::dontSendNotification);
    overlapSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    overlapSlider.setColour(juce::Slider::textBoxTextColourId,
                            juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
    overlapSlider.setColour(juce::Slider::textBoxBackgroundColourId,
                            juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    overlapSlider.addListener(this);
    addAndMakeVisible(overlapSlider);

    // ── "Plays Over" section ─────────────────────────────────────────────────
    setupLabel(this, playsOverTitle, "PLAYS OVER", 11.0f, true);

    playsOverHint.setText("Stack with a block to configure clip targeting.",
                          juce::dontSendNotification);
    playsOverHint.setFont(juce::Font(juce::FontOptions(11.0f)));
    playsOverHint.setColour(juce::Label::textColourId,
                            juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
    playsOverHint.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(playsOverHint);

    // ── Stack settings section ───────────────────────────────────────────────
    stackSectionTitle.setText("STACK SETTINGS", juce::dontSendNotification);
    stackSectionTitle.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    stackSectionTitle.setColour(juce::Label::textColourId,
                                juce::Colour(LookAndFeel_BlockShuffler::accentCol));
    addAndMakeVisible(stackSectionTitle);

    stackInfoLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    stackInfoLabel.setColour(juce::Label::textColourId,
                             juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
    addAndMakeVisible(stackInfoLabel);

    setupLabel(this, playModeLabel,       "Play Mode:",          12.0f);
    setupLabel(this, stackPlayCountLabel, "How Many to Play:",   12.0f);
    setupLabel(this, stackBlocksTitle,    "Blocks in stack:",    12.0f);

    playModeCombo.addItem("Sequential",   1);
    playModeCombo.addItem("Simultaneous", 2);
    playModeCombo.setColour(juce::ComboBox::backgroundColourId,
                            juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    playModeCombo.setColour(juce::ComboBox::textColourId,
                            juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
    playModeCombo.setColour(juce::ComboBox::arrowColourId,
                            juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
    playModeCombo.onChange = [this] {
        if (updatingFromModel || !selectedBlock || selectedBlock->stackGroup < 0 || !project)
            return;
        auto pre = project->toJSON();
        selectedBlock->stackPlayMode = (playModeCombo.getSelectedId() == 2)
                                       ? StackPlayMode::Simultaneous
                                       : StackPlayMode::Sequential;
        project->propagateStackSettings(selectedBlock->stackGroup);
        project->applyExternalMutation(pre);
    };
    addAndMakeVisible(playModeCombo);


    // ── Links section ────────────────────────────────────────────────────────
    setupLabel(this, linksTitle, "LINKS", 11.0f, true);

    // Tooltips
    probSlider     .setTooltip("Relative probability this clip plays when its block is reached");
    tempoField     .setTooltip("BPM of this clip — sets the tempo grid for the waveform editor");
    songEnderToggle.setTooltip("If this clip plays, the arrangement stops after it ends");
    clipDoneToggle .setTooltip("Exclude this clip from random selection");
    retainLeadIn   .setTooltip("Play the lead-in at its original speed instead of stretching");
    retainTail     .setTooltip("Play the tail at its original speed instead of stretching");
    blockDoneToggle.setTooltip("Exclude this entire block from the arrangement");
    overlapSlider  .setTooltip("Chance (%) this overlapping block plays on top of the block beneath it");
    playModeCombo  .setTooltip("Sequential: play chosen blocks one after another. Simultaneous: layer them.");

    updateFromModel();
}

// ── Public API ────────────────────────────────────────────────────────────────

void InspectorPanel::setProject(Project* p) { project = p; }

void InspectorPanel::setClip(Clip* clip, Block* block) {
    selectedClip  = clip;
    selectedBlock = block;
    updateFromModel();
}

void InspectorPanel::setBlock(Block* block) {
    selectedBlock = block;
    selectedClip  = nullptr;
    rebuildLinkRows();
    rebuildStackCountRows();
    rebuildStackBlockLabels();
    rebuildPlaysOverRows();
    updateFromModel();
    resized();
}

void InspectorPanel::refreshValues() {
    if (project && selectedBlock) {
        // Rebuild link rows if count changed
        int count = 0;
        for (auto* l : project->links)
            if (l->blockA == selectedBlock->id || l->blockB == selectedBlock->id) ++count;
        if (count != lastBuiltLinkCount)
            rebuildLinkRows();

        // Rebuild stack count rows if entry count changed
        if (selectedBlock->stackGroup >= 0) {
            if (selectedBlock->stackPlayCount.values.size() != lastBuiltStackCountRows) {
                rebuildStackCountRows();
                resized();
            }
            // Rebuild block list if the stack group or membership changed
            int curGroup = selectedBlock->stackGroup;
            int memberCount = 0;
            for (auto* b : project->blocks)
                if (b->stackGroup == curGroup) ++memberCount;
            if (curGroup != lastBuiltStackGroup ||
                memberCount != (int)stackBlockLabels.size()) {
                rebuildStackBlockLabels();
                resized();
            }
        }

        // Rebuild plays-over rows if sibling clip count changed
        if (selectedBlock->isOverlapping && selectedBlock->stackGroup >= 0) {
            int sibCount = 0;
            for (auto* b : project->blocks)
                if (b->stackGroup == selectedBlock->stackGroup &&
                    !b->isOverlapping && b != selectedBlock)
                    sibCount += b->clips.size();
            if (sibCount != lastBuiltPlaysOverClipCount) {
                rebuildPlaysOverRows();
                resized();
            }
        }
    }
    updateFromModel();
}

// ── Link rows ─────────────────────────────────────────────────────────────────

BlockLink* InspectorPanel::findLinkForRow(const LinkRow* row) const {
    if (!project) return nullptr;
    for (auto* l : project->links)
        if ((l->blockA == row->blockA && l->blockB == row->blockB) ||
            (l->blockA == row->blockB && l->blockB == row->blockA))
            return l;
    return nullptr;
}

void InspectorPanel::rebuildLinkRows() {
    for (auto* row : linkRows) {
        removeChildComponent(&row->label);
        removeChildComponent(&row->slider);
    }
    linkRows.clear();
    lastBuiltLinkCount = 0;

    if (!project || !selectedBlock) { resized(); return; }

    for (auto* link : project->links) {
        if (link->blockA != selectedBlock->id && link->blockB != selectedBlock->id) continue;

        auto* row = linkRows.add(new LinkRow());
        row->blockA = link->blockA;
        row->blockB = link->blockB;
        ++lastBuiltLinkCount;

        const juce::String otherId = (link->blockA == selectedBlock->id)
                                     ? link->blockB : link->blockA;
        juce::String otherName = otherId;
        if (auto* other = project->getBlockById(otherId))
            otherName = other->name;

        row->label.setText("<-> " + otherName, juce::dontSendNotification);
        row->label.setFont(juce::Font(juce::FontOptions(12.0f)));
        row->label.setColour(juce::Label::textColourId,
                             juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        row->slider.setValue(link->swapProbability * 100.0, juce::dontSendNotification);
        row->slider.setColour(juce::Slider::textBoxTextColourId,
                              juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        row->slider.setColour(juce::Slider::textBoxBackgroundColourId,
                              juce::Colour(LookAndFeel_BlockShuffler::bgLight));
        row->slider.addListener(this);
        addAndMakeVisible(row->label);
        addAndMakeVisible(row->slider);
    }
    resized();
}

// ── Stack count rows ──────────────────────────────────────────────────────────

void InspectorPanel::rebuildStackCountRows() {
    for (auto* row : stackCountRows) {
        removeChildComponent(&row->decBtn);
        removeChildComponent(&row->countLbl);
        removeChildComponent(&row->incBtn);
        removeChildComponent(&row->removeBtn);
    }
    stackCountRows.clear();
    lastBuiltStackCountRows = 0;

    if (!selectedBlock || selectedBlock->stackGroup < 0) return;
    auto& spc = selectedBlock->stackPlayCount;
    if (!spc.isValid()) return;

    for (int i = 0; i < spc.values.size(); ++i) {
        auto* row = stackCountRows.add(new StackCountRow());
        ++lastBuiltStackCountRows;

        row->countLbl.setText("Play " + juce::String(spc.values[i]),
                              juce::dontSendNotification);
        row->countLbl.setFont(juce::Font(juce::FontOptions(12.0f)));
        row->countLbl.setColour(juce::Label::textColourId,
                                juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        row->countLbl.setJustificationType(juce::Justification::centredLeft);

        row->decBtn.onClick = [this, row] {
            if (!selectedBlock || updatingFromModel || !project) return;
            int idx = stackCountRows.indexOf(row);
            if (idx < 0 || idx >= selectedBlock->stackPlayCount.values.size()) return;
            int cur = selectedBlock->stackPlayCount.values[idx];
            if (cur <= 1) return;
            selectedBlock->stackPlayCount.values.set(idx, cur - 1);
            row->countLbl.setText("Play " + juce::String(cur - 1), juce::dontSendNotification);
            project->propagateStackSettings(selectedBlock->stackGroup, selectedBlock);
        };

        row->incBtn.onClick = [this, row] {
            if (!selectedBlock || updatingFromModel || !project) return;
            int idx = stackCountRows.indexOf(row);
            if (idx < 0 || idx >= selectedBlock->stackPlayCount.values.size()) return;
            // Clamp to number of non-overlapping, non-done blocks in this stack
            int maxPlayable = 0;
            for (auto* b : project->blocks)
                if (b->stackGroup == selectedBlock->stackGroup &&
                    !b->isOverlapping && !b->isDone)
                    ++maxPlayable;
            maxPlayable = juce::jmax(1, maxPlayable);
            int cur = selectedBlock->stackPlayCount.values[idx];
            if (cur >= maxPlayable) return;
            selectedBlock->stackPlayCount.values.set(idx, cur + 1);
            row->countLbl.setText("Play " + juce::String(cur + 1), juce::dontSendNotification);
            project->propagateStackSettings(selectedBlock->stackGroup, selectedBlock);
        };

        row->removeBtn.onClick = [this, row] {
            if (!selectedBlock || !project) return;
            int idx = stackCountRows.indexOf(row);
            if (idx < 0) return;
            auto& spc2 = selectedBlock->stackPlayCount;
            if (spc2.values.size() <= 1) return;
            spc2.values.remove(idx);
            spc2.weights.remove(idx);
            project->propagateStackSettings(selectedBlock->stackGroup, selectedBlock);
            rebuildStackCountRows();
            resized();
        };

        addAndMakeVisible(row->decBtn);
        addAndMakeVisible(row->countLbl);
        addAndMakeVisible(row->incBtn);
        addAndMakeVisible(row->removeBtn);
    }
}

void InspectorPanel::rebuildStackBlockLabels() {
    for (auto* lbl : stackBlockLabels)
        removeChildComponent(lbl);
    stackBlockLabels.clear();

    for (auto* s : stackBlockProbSliders)
        removeChildComponent(s);
    stackBlockProbSliders.clear();

    if (!selectedBlock || selectedBlock->stackGroup < 0 || !project) return;

    for (auto* b : project->blocks) {
        if (b->stackGroup != selectedBlock->stackGroup) continue;
        bool isThis = (b->id == selectedBlock->id);
        auto* lbl = stackBlockLabels.add(new juce::Label());
        juce::String text = juce::String(juce::CharPointer_UTF8("\xe2\x80\xa2 ")) + b->name;
        if (isThis) text += "  \xe2\x86\x90 this";
        lbl->setText(text, juce::dontSendNotification);
        lbl->setFont(juce::Font(juce::FontOptions(11.0f)));
        lbl->setColour(juce::Label::textColourId,
                       isThis ? juce::Colour(LookAndFeel_BlockShuffler::textPrimary)
                               : juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
        addAndMakeVisible(lbl);

        auto* slider = stackBlockProbSliders.add(new juce::Slider());
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setRange(0.0, 100.0, 1.0);
        slider->setValue(b->probability * 100.0, juce::dontSendNotification);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 16);
        slider->setColour(juce::Slider::textBoxTextColourId,
                         juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        slider->setColour(juce::Slider::textBoxBackgroundColourId,
                         juce::Colour(LookAndFeel_BlockShuffler::bgLight));
        slider->setColour(juce::Slider::thumbColourId,
                         juce::Colour(LookAndFeel_BlockShuffler::accentCol));
        slider->onDragStart = [b, this] {
            if (!updatingFromModel && project)
                probSliderDragPre = project->toJSON();
        };
        slider->onValueChange = [b, slider, this] {
            if (updatingFromModel || !project) return;
            b->probability = (float)slider->getValue() / 100.0f;
        };
        slider->onDragEnd = [b, this] {
            if (updatingFromModel || !project || probSliderDragPre.isVoid()) return;
            project->applyExternalMutation(probSliderDragPre);
            probSliderDragPre = juce::var{};
        };
        addAndMakeVisible(slider);
    }
}

// ── Plays-over rows ───────────────────────────────────────────────────────────

void InspectorPanel::rebuildPlaysOverRows() {
    for (auto* row : playsOverRows)
        removeChildComponent(&row->toggle);
    playsOverRows.clear();
    lastBuiltPlaysOverClipCount = 0;

    if (!selectedBlock || !project || !selectedBlock->isOverlapping ||
        selectedBlock->stackGroup < 0)
        return;

    for (auto* block : project->blocks) {
        if (block->stackGroup != selectedBlock->stackGroup) continue;
        if (block->isOverlapping || block == selectedBlock) continue;
        for (auto* clip : block->clips) {
            auto* row = playsOverRows.add(new PlaysOverRow());
            row->clipId = clip->id;
            row->toggle.setButtonText(clip->name + " (" + block->name + ")");
            row->toggle.setColour(juce::ToggleButton::textColourId,
                                  juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
            row->toggle.addListener(this);
            addAndMakeVisible(row->toggle);
            ++lastBuiltPlaysOverClipCount;
        }
    }
}

// ── updateFromModel ───────────────────────────────────────────────────────────

void InspectorPanel::updateFromModel() {
    updatingFromModel = true;

    const bool hasClip  = (selectedClip  != nullptr);
    const bool hasBlock = (selectedBlock != nullptr);

    // ── Clip section
    probSlider     .setEnabled(hasClip);
    tempoField     .setEnabled(hasClip);
    songEnderToggle.setEnabled(hasClip);
    clipDoneToggle .setEnabled(hasClip);
    retainLeadIn   .setEnabled(hasClip);
    retainTail     .setEnabled(hasClip);

    if (hasClip) {
        probSlider.setValue(selectedClip->probability * 100.0, juce::dontSendNotification);
        tempoField.setValue(selectedClip->tempo, juce::dontSendNotification);
        songEnderToggle.setToggleState(selectedClip->isSongEnder,       juce::dontSendNotification);
        clipDoneToggle .setToggleState(selectedClip->isDone,            juce::dontSendNotification);
        retainLeadIn   .setToggleState(selectedClip->retainLeadInTempo, juce::dontSendNotification);
        retainTail     .setToggleState(selectedClip->retainTailTempo,   juce::dontSendNotification);
    }

    if (hasClip && hasBlock) {
        juce::String effText;
        if (selectedClip->isDone) {
            effText = "0% effective (excluded)";
        } else {
            float total = 0.0f;
            for (auto* c : selectedBlock->clips)
                if (!c->isDone) total += c->probability;
            float eff = (total > 0.0f)
                      ? (selectedClip->probability / total) * 100.0f : 0.0f;
            effText = juce::String(eff, 1) + "% effective";
        }
        effectiveProbLabel.setText(effText, juce::dontSendNotification);
        effectiveProbLabel.setVisible(true);
    } else {
        effectiveProbLabel.setVisible(false);
    }

    // ── Block section
    blockDoneToggle.setEnabled(hasBlock);
    if (hasBlock)
        blockDoneToggle.setToggleState(selectedBlock->isDone, juce::dontSendNotification);

    const bool canOverlap = hasBlock && selectedBlock->isOverlapping;
    overlapSlider.setEnabled(canOverlap);
    if (hasBlock)
        overlapSlider.setValue(selectedBlock->overlapProbability * 100.0,
                               juce::dontSendNotification);

    // ── "Plays Over" section
    const bool showPlaysOver = hasBlock && selectedBlock->isOverlapping;
    playsOverTitle.setVisible(showPlaysOver);
    const bool inStack = hasBlock && selectedBlock->stackGroup >= 0;
    playsOverHint.setVisible(showPlaysOver && (!inStack || playsOverRows.isEmpty()));
    for (auto* row : playsOverRows)
        row->toggle.setVisible(showPlaysOver && inStack);
    if (showPlaysOver && inStack) {
        for (auto* row : playsOverRows) {
            bool checked = selectedBlock->allowedParentClipIds.isEmpty() ||
                           selectedBlock->allowedParentClipIds.contains(row->clipId);
            row->toggle.setToggleState(checked, juce::dontSendNotification);
        }
    }

    // ── Stack settings section
    stackSectionTitle  .setVisible(inStack);
    stackInfoLabel     .setVisible(inStack);
    playModeLabel      .setVisible(inStack);
    playModeCombo      .setVisible(inStack);
    stackPlayCountLabel.setVisible(inStack);
    addStackCountBtn   .setVisible(inStack);
    stackBlocksTitle   .setVisible(inStack);

    if (inStack) {
        // Update info label
        int memberCount = 0;
        if (project)
            for (auto* b : project->blocks)
                if (b->stackGroup == selectedBlock->stackGroup) ++memberCount;
        stackInfoLabel.setText("Group " + juce::String(selectedBlock->stackGroup + 1)
                               + "  \xc2\xb7  " + juce::String(memberCount) + " blocks",
                               juce::dontSendNotification);

        // Play mode combo
        playModeCombo.setSelectedId(
            selectedBlock->stackPlayMode == StackPlayMode::Simultaneous ? 2 : 1,
            juce::dontSendNotification);

        // Count rows
        auto& spc = selectedBlock->stackPlayCount;
        if (spc.values.size() != lastBuiltStackCountRows) {
            rebuildStackCountRows();
            resized();
        }
        for (int i = 0; i < stackCountRows.size(); ++i) {
            auto* row = stackCountRows[i];
            if (i < spc.values.size())
                row->countLbl.setText("Play " + juce::String(spc.values[i]),
                                      juce::dontSendNotification);
            row->decBtn.setVisible(true);
            row->countLbl.setVisible(true);
            row->incBtn.setVisible(true);
            row->removeBtn.setVisible(stackCountRows.size() > 1);
        }
    } else {
        for (auto* row : stackCountRows) {
            row->decBtn.setVisible(false);
            row->countLbl.setVisible(false);
            row->incBtn.setVisible(false);
            row->removeBtn.setVisible(false);
        }
    }

    // Block list labels
    for (auto* lbl : stackBlockLabels)
        lbl->setVisible(inStack);

    // ── Links section
    for (auto* row : linkRows)
        if (auto* link = findLinkForRow(row))
            row->slider.setValue(link->swapProbability * 100.0, juce::dontSendNotification);

    updatingFromModel = false;
    repaint();
}

// ── Slider listeners ──────────────────────────────────────────────────────────

void InspectorPanel::sliderValueChanged(juce::Slider* slider) {
    if (updatingFromModel) return;

    if (slider == &probSlider && selectedClip) {
        selectedClip->probability = (float)(probSlider.getValue() / 100.0);
        if (selectedBlock) {
            float total = 0.0f;
            for (auto* c : selectedBlock->clips)
                if (!c->isDone) total += c->probability;
            float eff = (total > 0.0f)
                      ? (selectedClip->probability / total) * 100.0f : 0.0f;
            effectiveProbLabel.setText(juce::String(eff, 1) + "% effective",
                                       juce::dontSendNotification);
        }
        if (onClipProbabilityChanged) onClipProbabilityChanged();
        return;
    }
    if (slider == &overlapSlider && selectedBlock) {
        selectedBlock->overlapProbability = (float)(overlapSlider.getValue() / 100.0);
        return;
    }
    for (auto* row : linkRows) {
        if (slider == &row->slider) {
            if (auto* link = findLinkForRow(row))
                link->swapProbability = juce::jlimit(0.0f, 1.0f,
                    (float)(row->slider.getValue() / 100.0));
            return;
        }
    }
}

void InspectorPanel::sliderDragStarted(juce::Slider* slider) {
    if (updatingFromModel) return;
    if (slider == &probSlider && selectedClip && project)
        probSliderDragPre = project->toJSON();
    else if (slider == &overlapSlider && selectedBlock && project)
        overlapSliderDragPre = project->toJSON();
    else {
        for (auto* row : linkRows) {
            if (slider == &row->slider && project) {
                row->dragPre = project->toJSON();
                break;
            }
        }
    }
}

void InspectorPanel::sliderDragEnded(juce::Slider* slider) {
    if (slider == &probSlider && selectedClip && project && !probSliderDragPre.isVoid()) {
        project->applyExternalMutation(probSliderDragPre);
        probSliderDragPre = juce::var{};
        return;
    }
    if (slider == &overlapSlider && selectedBlock && project && !overlapSliderDragPre.isVoid()) {
        project->applyExternalMutation(overlapSliderDragPre);
        overlapSliderDragPre = juce::var{};
        return;
    }
    for (auto* row : linkRows) {
        if (slider == &row->slider && project && !row->dragPre.isVoid()) {
            project->applyExternalMutation(row->dragPre);
            row->dragPre = juce::var{};
            return;
        }
    }
}

// ── Button listener ───────────────────────────────────────────────────────────

void InspectorPanel::buttonClicked(juce::Button* btn) {
    if (updatingFromModel) return;

    if (btn == &blockDoneToggle && selectedBlock && project) {
        auto pre = project->toJSON();
        selectedBlock->isDone = blockDoneToggle.getToggleState();
        project->applyExternalMutation(pre);
        return;
    }

    for (auto* row : playsOverRows) {
        if (btn == &row->toggle && selectedBlock && project) {
            bool isChecked = row->toggle.getToggleState();
            auto pre = project->toJSON();
            if (isChecked) {
                if (!selectedBlock->allowedParentClipIds.contains(row->clipId))
                    selectedBlock->allowedParentClipIds.add(row->clipId);
                bool allCovered = true;
                for (auto* r : playsOverRows)
                    if (!selectedBlock->allowedParentClipIds.contains(r->clipId))
                        { allCovered = false; break; }
                if (allCovered)
                    selectedBlock->allowedParentClipIds.clear();
            } else {
                if (selectedBlock->allowedParentClipIds.isEmpty()) {
                    for (auto* r : playsOverRows)
                        if (r->clipId != row->clipId &&
                            !selectedBlock->allowedParentClipIds.contains(r->clipId))
                            selectedBlock->allowedParentClipIds.add(r->clipId);
                } else {
                    selectedBlock->allowedParentClipIds.removeString(row->clipId);
                }
            }
            project->applyExternalMutation(pre);
            updatingFromModel = true;
            for (auto* r : playsOverRows) {
                bool checked = selectedBlock->allowedParentClipIds.isEmpty() ||
                               selectedBlock->allowedParentClipIds.contains(r->clipId);
                r->toggle.setToggleState(checked, juce::dontSendNotification);
            }
            updatingFromModel = false;
            return;
        }
    }

    if (!selectedClip || !project) return;
    auto pre = project->toJSON();
    if      (btn == &retainLeadIn)     selectedClip->retainLeadInTempo = retainLeadIn   .getToggleState();
    else if (btn == &retainTail)       selectedClip->retainTailTempo   = retainTail    .getToggleState();
    else if (btn == &songEnderToggle)  selectedClip->isSongEnder       = songEnderToggle.getToggleState();
    else if (btn == &clipDoneToggle)   selectedClip->isDone             = clipDoneToggle .getToggleState();
    else return;
    project->applyExternalMutation(pre);
}

// ── paint / resized ───────────────────────────────────────────────────────────

void InspectorPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgMedium));
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    g.drawLine(0.0f, 0.0f, 0.0f, (float)getHeight(), 1.0f);

    // Tinted background for the stack settings section
    if (stackSectionY >= 0 && stackSectionH > 0) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.06f));
        g.fillRect(0, stackSectionY, getWidth(), stackSectionH);
        // Top and bottom separator lines
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.3f));
        g.drawHorizontalLine(stackSectionY, 0.0f, (float)getWidth());
        g.drawHorizontalLine(stackSectionY + stackSectionH, 0.0f, (float)getWidth());
    }
}

void InspectorPanel::resized() {
    auto area = getLocalBounds().reduced(8, 8);
    const int rh  = 22;
    const int slh = 28;
    const int gap = 4;
    const int sec = 10;

    // ── Clip section
    clipTitle.setBounds(area.removeFromTop(rh));
    area.removeFromTop(gap);
    probLabel .setBounds(area.removeFromTop(rh));
    probSlider.setBounds(area.removeFromTop(slh));
    if (effectiveProbLabel.isVisible())
        effectiveProbLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(gap);
    tempoLabel.setBounds(area.removeFromTop(rh));
    tempoField.setBounds(area.removeFromTop(rh));
    area.removeFromTop(gap);
    songEnderToggle.setBounds(area.removeFromTop(rh)); area.removeFromTop(2);
    clipDoneToggle .setBounds(area.removeFromTop(rh)); area.removeFromTop(2);
    retainLeadIn   .setBounds(area.removeFromTop(rh)); area.removeFromTop(2);
    retainTail     .setBounds(area.removeFromTop(rh));
    area.removeFromTop(sec);

    // ── Block section
    blockTitle     .setBounds(area.removeFromTop(rh)); area.removeFromTop(gap);
    blockDoneToggle.setBounds(area.removeFromTop(rh)); area.removeFromTop(2);
    overlapLabel   .setBounds(area.removeFromTop(rh));
    overlapSlider  .setBounds(area.removeFromTop(slh));
    area.removeFromTop(gap);

    // ── Plays-over section
    if (playsOverTitle.isVisible()) {
        playsOverTitle.setBounds(area.removeFromTop(rh)); area.removeFromTop(2);
        if (playsOverHint.isVisible()) {
            playsOverHint.setBounds(area.removeFromTop(rh * 2));
        } else {
            for (auto* row : playsOverRows)
                if (row->toggle.isVisible()) {
                    row->toggle.setBounds(area.removeFromTop(rh)); area.removeFromTop(2);
                }
        }
        area.removeFromTop(gap);
    }

    // ── Stack settings section
    const bool inStack = (selectedBlock && selectedBlock->stackGroup >= 0);
    stackSectionY = -1;
    stackSectionH = 0;

    if (inStack) {
        stackSectionY = getHeight() - area.getHeight() - 8;  // top of section in component coords

        stackSectionTitle.setBounds(area.removeFromTop(rh)); area.removeFromTop(2);
        stackInfoLabel   .setBounds(area.removeFromTop(rh - 4)); area.removeFromTop(gap);

        playModeLabel.setBounds(area.removeFromTop(rh));
        playModeCombo.setBounds(area.removeFromTop(slh)); area.removeFromTop(gap);

        stackPlayCountLabel.setBounds(area.removeFromTop(rh)); area.removeFromTop(2);

        for (auto* row : stackCountRows) {
            auto rowArea = area.removeFromTop(rh);
            row->decBtn   .setBounds(rowArea.removeFromLeft(24));
            row->countLbl .setBounds(rowArea.removeFromLeft(44));
            row->incBtn   .setBounds(rowArea.removeFromLeft(24));
            row->removeBtn.setBounds(rowArea.removeFromRight(24));
            area.removeFromTop(2);
        }

        stackBlocksTitle.setBounds(area.removeFromTop(rh)); area.removeFromTop(2);
        for (int i = 0; i < stackBlockLabels.size(); ++i) {
            auto rowArea = area.removeFromTop(rh);
            stackBlockLabels[i]->setBounds(rowArea.removeFromLeft(90));
            stackBlockProbSliders[i]->setBounds(rowArea.reduced(0, 2));
            area.removeFromTop(2);
        }
        area.removeFromTop(gap);
    }

    area.removeFromTop(sec - gap);

    // ── Links section
    linksTitle.setBounds(area.removeFromTop(rh)); area.removeFromTop(gap);
    for (auto* row : linkRows) {
        row->label .setBounds(area.removeFromTop(rh));
        row->slider.setBounds(area.removeFromTop(slh));
        area.removeFromTop(gap);
    }
}

} // namespace BlockShuffler
