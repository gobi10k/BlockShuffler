#include "InspectorPanel.h"

namespace BlockShuffler {

InspectorPanel::InspectorPanel() {
    auto setupLabel = [this](juce::Label& lbl, const juce::String& text,
                             float fontSize, bool secondary = false) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(juce::FontOptions(fontSize).withStyle(secondary ? "Bold" : "Plain")));
        lbl.setColour(juce::Label::textColourId,
                      secondary ? juce::Colour(LookAndFeel_BlockShuffler::textSecondary)
                                : juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        addAndMakeVisible(lbl);
    };

    setupLabel(clipTitle,    "CLIP",                    11.0f, true);
    setupLabel(probLabel,    "Probability (%)",         12.0f);
    setupLabel(tempoLabel,   "Tempo (BPM)",             12.0f);
    setupLabel(blockTitle,   "BLOCK",                   11.0f, true);
    setupLabel(overlapLabel, "Overlap Probability (%)", 12.0f);
    setupLabel(stackPlayCountLabel, "Stack Play Count",  12.0f);
    setupLabel(stackPlayModeLabel,  "Stack Play Mode",  12.0f);
    setupLabel(linksTitle,          "LINKS",            11.0f, true);
    setupLabel(playsOverTitle,      "PLAYS OVER",       11.0f, true);

    playsOverHint.setText("Stack with a block to configure clip targeting.",
                          juce::dontSendNotification);
    playsOverHint.setFont(juce::Font(juce::FontOptions(11.0f)));
    playsOverHint.setColour(juce::Label::textColourId,
                            juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
    playsOverHint.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(playsOverHint);

    effectiveProbLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    effectiveProbLabel.setColour(juce::Label::textColourId,
                                 juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
    effectiveProbLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(effectiveProbLabel);

    // Probability slider
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

    // Tempo field
    tempoField.setInputRestrictions(7, "0123456789.");
    tempoField.setText("120.0", false);
    tempoField.setColour(juce::TextEditor::backgroundColourId,
                         juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    tempoField.setColour(juce::TextEditor::textColourId,
                         juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
    tempoField.setColour(juce::TextEditor::outlineColourId,
                         juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    tempoField.onReturnKey = [this] {
        if (selectedClip && !updatingFromModel && project) {
            double t = tempoField.getText().getDoubleValue();
            if (t > 0.0 && std::abs(t - selectedClip->tempo) > 0.001) {
                auto pre = project->toJSON();
                selectedClip->tempo = t;
                project->applyExternalMutation(pre);
            }
        }
    };
    tempoField.onFocusLost = [this] {
        if (selectedClip && !updatingFromModel && project) {
            double t = tempoField.getText().getDoubleValue();
            if (t > 0.0 && std::abs(t - selectedClip->tempo) > 0.001) {
                auto pre = project->toJSON();
                selectedClip->tempo = t;
                project->applyExternalMutation(pre);
            }
        }
    };
    addAndMakeVisible(tempoField);

    retainLeadIn.addListener(this);
    retainTail.addListener(this);
    songEnderToggle.addListener(this);
    clipDoneToggle .addListener(this);
    blockDoneToggle.addListener(this);
    addAndMakeVisible(songEnderToggle);
    addAndMakeVisible(clipDoneToggle);
    addAndMakeVisible(retainLeadIn);
    addAndMakeVisible(retainTail);
    addAndMakeVisible(blockDoneToggle);

    // Overlap slider
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

    // Stack count weighted editor
    addStackCountBtn.onClick = [this] {
        if (!selectedBlock || selectedBlock->stackGroup < 0 || !project) return;
        auto pre = project->toJSON();
        auto& spc = selectedBlock->stackPlayCount;
        int newCount = spc.values.isEmpty() ? 1 : spc.values.getLast() + 1;
        spc.values.add(newCount);
        spc.weights.add(10.0f);
        project->applyExternalMutation(pre);
        juce::Component::SafePointer<InspectorPanel> safeThis(this);
        juce::MessageManager::callAsync(
            [safeThis] {
                if (safeThis) { safeThis->rebuildStackCountRows(); safeThis->resized(); }
            });
    };
    addStackCountBtn.setTooltip("Add another count option with its own probability weight");
    addAndMakeVisible(addStackCountBtn);

    simultaneousToggle.addListener(this);

    // Tooltips
    probSlider        .setTooltip("Relative probability this clip plays when its block is reached (0 = never, 100 = always relative to others)");
    tempoField        .setTooltip("BPM of this clip - sets the tempo grid for the waveform editor");
    songEnderToggle   .setTooltip("If this clip plays, the arrangement stops after it ends");
    clipDoneToggle    .setTooltip("Exclude this clip from random selection");
    retainLeadIn      .setTooltip("Play the lead-in at its original speed instead of stretching to match the previous block's tempo");
    retainTail        .setTooltip("Play the tail at its original speed instead of stretching to match the next block's tempo");
    blockDoneToggle   .setTooltip("Exclude this entire block from the arrangement");
    overlapSlider     .setTooltip("Chance (%) this overlapping block plays on top of the block beneath it");
    simultaneousToggle.setTooltip("Play the chosen stack blocks simultaneously (layered) instead of one after another");

    addAndMakeVisible(stackPlayCountLabel);
    addAndMakeVisible(stackPlayModeLabel);
    addAndMakeVisible(simultaneousToggle);

    updateFromModel();
}

//==============================================================================
void InspectorPanel::setProject(Project* p) {
    project = p;
}

void InspectorPanel::setClip(Clip* clip, Block* block) {
    selectedClip  = clip;
    selectedBlock = block;
    updateFromModel();
}

void InspectorPanel::setBlock(Block* block) {
    selectedBlock = block;
    selectedClip  = nullptr;  // block changed (or undo rebuilt it) — clip pointer is now stale
    rebuildLinkRows();
    rebuildStackCountRows();
    rebuildPlaysOverRows();
    updateFromModel();
    resized();  // give stack-count rows their bounds after rebuild
}

void InspectorPanel::refreshValues() {
    // Rebuild link rows if count changed
    if (project && selectedBlock) {
        int count = 0;
        for (auto* l : project->links)
            if (l->blockA == selectedBlock->id || l->blockB == selectedBlock->id)
                ++count;
        if (count != lastBuiltLinkCount)
            rebuildLinkRows();
    }
    // Rebuild stack count rows if entry count changed
    if (selectedBlock && selectedBlock->stackGroup >= 0) {
        if (selectedBlock->stackPlayCount.values.size() != lastBuiltStackCountRows) {
            rebuildStackCountRows();
            resized();
        }
    }
    // Rebuild plays-over rows if sibling clip count changed
    if (selectedBlock && selectedBlock->isOverlapping && selectedBlock->stackGroup >= 0 && project) {
        int siblingClipCount = 0;
        for (auto* block : project->blocks)
            if (block->stackGroup == selectedBlock->stackGroup && !block->isOverlapping && block != selectedBlock)
                siblingClipCount += block->clips.size();
        if (siblingClipCount != lastBuiltPlaysOverClipCount) {
            rebuildPlaysOverRows();
            resized();
        }
    }
    updateFromModel();
}

//==============================================================================
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
        const bool involves = (link->blockA == selectedBlock->id ||
                               link->blockB == selectedBlock->id);
        if (!involves) continue;

        auto* row = linkRows.add(new LinkRow());
        row->blockA = link->blockA;
        row->blockB = link->blockB;
        ++lastBuiltLinkCount;

        const juce::String otherId = (link->blockA == selectedBlock->id)
                                     ? link->blockB : link->blockA;
        juce::String otherName = otherId;
        if (auto* other = project->getBlockById(otherId))
            otherName = other->name;

        row->label.setText("<-> " + otherName,
                           juce::dontSendNotification);
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

void InspectorPanel::rebuildStackCountRows() {
    for (auto* row : stackCountRows) {
        removeChildComponent(&row->decBtn);
        removeChildComponent(&row->countLbl);
        removeChildComponent(&row->incBtn);
        removeChildComponent(&row->weightSlider);
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

        row->countLbl.setText(juce::String(spc.values[i]), juce::dontSendNotification);
        row->countLbl.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
        row->countLbl.setColour(juce::Label::textColourId,
                                juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        row->countLbl.setJustificationType(juce::Justification::centred);

        row->weightSlider.setValue(spc.weights[i], juce::dontSendNotification);
        row->weightSlider.setColour(juce::Slider::textBoxTextColourId,
                                    juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        row->weightSlider.setColour(juce::Slider::textBoxBackgroundColourId,
                                    juce::Colour(LookAndFeel_BlockShuffler::bgLight));
        row->weightSlider.setTooltip("Relative weight - higher = more likely this count is chosen");

        row->decBtn.onClick = [this, row] {
            if (!selectedBlock || updatingFromModel || !project) return;
            int idx = stackCountRows.indexOf(row);
            if (idx < 0 || idx >= selectedBlock->stackPlayCount.values.size()) return;
            int cur = selectedBlock->stackPlayCount.values[idx];
            if (cur > 1) {
                auto pre = project->toJSON();
                selectedBlock->stackPlayCount.values.set(idx, cur - 1);
                row->countLbl.setText(juce::String(cur - 1), juce::dontSendNotification);
                project->applyExternalMutation(pre);
            }
        };

        row->incBtn.onClick = [this, row] {
            if (!selectedBlock || updatingFromModel || !project) return;
            int idx = stackCountRows.indexOf(row);
            if (idx < 0 || idx >= selectedBlock->stackPlayCount.values.size()) return;
            int cur = selectedBlock->stackPlayCount.values[idx];
            auto pre = project->toJSON();
            selectedBlock->stackPlayCount.values.set(idx, cur + 1);
            row->countLbl.setText(juce::String(cur + 1), juce::dontSendNotification);
            project->applyExternalMutation(pre);
        };

        row->weightSlider.onDragStart = [this, row] {
            if (!selectedBlock || updatingFromModel || !project) return;
            int idx = stackCountRows.indexOf(row);
            if (idx >= 0) row->dragPre = project->toJSON();
        };
        row->weightSlider.onValueChange = [this, row] {
            if (!selectedBlock || updatingFromModel) return;
            int idx = stackCountRows.indexOf(row);
            if (idx < 0 || idx >= selectedBlock->stackPlayCount.weights.size()) return;
            selectedBlock->stackPlayCount.weights.set(idx, (float)row->weightSlider.getValue());
        };
        row->weightSlider.onDragEnd = [this, row] {
            if (!selectedBlock || updatingFromModel || !project) return;
            int idx = stackCountRows.indexOf(row);
            if (idx >= 0 && !row->dragPre.isVoid())
                project->applyExternalMutation(row->dragPre);
        };

        row->removeBtn.onClick = [this, row] {
            if (!selectedBlock || !project) return;
            int idx = stackCountRows.indexOf(row);
            if (idx < 0) return;
            auto& spc2 = selectedBlock->stackPlayCount;
            if (spc2.values.size() <= 1) return;  // must keep at least one entry
            auto pre = project->toJSON();
            spc2.values.remove(idx);
            spc2.weights.remove(idx);
            project->applyExternalMutation(pre);
            // Defer rebuild: rebuildStackCountRows() destroys the StackCountRow that owns
            // this button. Doing it synchronously would destroy the button while JUCE's
            // click handler is still running (use-after-free → juce_String assertion crash).
            juce::Component::SafePointer<InspectorPanel> safeThis(this);
            juce::MessageManager::callAsync(
                [safeThis] {
                    if (safeThis) { safeThis->rebuildStackCountRows(); safeThis->resized(); }
                });
        };

        addAndMakeVisible(row->decBtn);
        addAndMakeVisible(row->countLbl);
        addAndMakeVisible(row->incBtn);
        addAndMakeVisible(row->weightSlider);
        addAndMakeVisible(row->removeBtn);
    }
}

void InspectorPanel::rebuildPlaysOverRows() {
    for (auto* row : playsOverRows)
        removeChildComponent(&row->toggle);
    playsOverRows.clear();
    lastBuiltPlaysOverClipCount = 0;

    if (!selectedBlock || !project || !selectedBlock->isOverlapping || selectedBlock->stackGroup < 0)
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

void InspectorPanel::updateFromModel() {
    updatingFromModel = true;

    const bool hasClip  = (selectedClip  != nullptr);
    const bool hasBlock = (selectedBlock != nullptr);

    probSlider     .setEnabled(hasClip);
    tempoField     .setEnabled(hasClip);
    songEnderToggle.setEnabled(hasClip);
    clipDoneToggle .setEnabled(hasClip);
    retainLeadIn   .setEnabled(hasClip);
    retainTail     .setEnabled(hasClip);

    if (hasClip) {
        probSlider.setValue(selectedClip->probability * 100.0,
                            juce::dontSendNotification);
        tempoField.setText(juce::String(selectedClip->tempo, 1), false);
        songEnderToggle.setToggleState(selectedClip->isSongEnder,       juce::dontSendNotification);
        clipDoneToggle .setToggleState(selectedClip->isDone,            juce::dontSendNotification);
        retainLeadIn   .setToggleState(selectedClip->retainLeadInTempo, juce::dontSendNotification);
        retainTail     .setToggleState(selectedClip->retainTailTempo,   juce::dontSendNotification);
    }

    // Effective probability label
    if (hasClip && hasBlock) {
        juce::String effText;
        if (selectedClip->isDone) {
            effText = "0% effective (excluded)";
        } else {
            float totalWeight = 0.0f;
            for (auto* c : selectedBlock->clips)
                if (!c->isDone) totalWeight += c->probability;
            float eff = (totalWeight > 0.0f)
                      ? (selectedClip->probability / totalWeight) * 100.0f
                      : 0.0f;
            effText = juce::String(eff, 1) + "% effective";
        }
        effectiveProbLabel.setText(effText, juce::dontSendNotification);
        effectiveProbLabel.setVisible(true);
    } else {
        effectiveProbLabel.setVisible(false);
    }

    blockDoneToggle.setEnabled(hasBlock);
    if (hasBlock)
        blockDoneToggle.setToggleState(selectedBlock->isDone, juce::dontSendNotification);

    const bool canOverlap = hasBlock && selectedBlock->isOverlapping;
    overlapSlider.setEnabled(canOverlap);
    if (hasBlock)
        overlapSlider.setValue(selectedBlock->overlapProbability * 100.0,
                               juce::dontSendNotification);

    // "Plays Over" section — only when block is overlapping
    const bool showPlaysOver = hasBlock && selectedBlock->isOverlapping;
    playsOverTitle.setVisible(showPlaysOver);
    const bool inStack = hasBlock && selectedBlock->stackGroup >= 0;
    const bool noSiblingClips = playsOverRows.isEmpty();
    playsOverHint.setVisible(showPlaysOver && (!inStack || noSiblingClips));

    for (auto* row : playsOverRows)
        row->toggle.setVisible(showPlaysOver && inStack);

    if (showPlaysOver && inStack && !playsOverRows.isEmpty()) {
        for (auto* row : playsOverRows) {
            bool checked = selectedBlock->allowedParentClipIds.isEmpty() ||
                           selectedBlock->allowedParentClipIds.contains(row->clipId);
            row->toggle.setToggleState(checked, juce::dontSendNotification);
        }
    }

    // Stack controls — only relevant when block is in a stack
    stackPlayCountLabel.setVisible(inStack);
    stackPlayModeLabel .setVisible(inStack);
    simultaneousToggle .setVisible(inStack);
    addStackCountBtn   .setVisible(inStack);

    if (inStack) {
        auto& spc = selectedBlock->stackPlayCount;
        // Rebuild rows if structure changed
        if (spc.values.size() != lastBuiltStackCountRows)
            rebuildStackCountRows();
        // Refresh values
        for (int i = 0; i < stackCountRows.size(); ++i) {
            auto* row = stackCountRows[i];
            if (i < spc.values.size())
                row->countLbl.setText(juce::String(spc.values[i]), juce::dontSendNotification);
            if (i < spc.weights.size())
                row->weightSlider.setValue(spc.weights[i], juce::dontSendNotification);
            row->decBtn.setVisible(true);
            row->countLbl.setVisible(true);
            row->incBtn.setVisible(true);
            row->weightSlider.setVisible(true);
            row->removeBtn.setVisible(true);
        }
        simultaneousToggle.setToggleState(
            selectedBlock->stackPlayMode == StackPlayMode::Simultaneous,
            juce::dontSendNotification);
    } else {
        // Hide all rows
        for (auto* row : stackCountRows) {
            row->decBtn.setVisible(false);
            row->countLbl.setVisible(false);
            row->incBtn.setVisible(false);
            row->weightSlider.setVisible(false);
            row->removeBtn.setVisible(false);
        }
    }

    // Refresh link row slider values (e.g. after undo restores a different value)
    for (auto* row : linkRows) {
        if (auto* link = findLinkForRow(row))
            row->slider.setValue(link->swapProbability * 100.0, juce::dontSendNotification);
    }

    updatingFromModel = false;
    repaint();
}

//==============================================================================
void InspectorPanel::sliderValueChanged(juce::Slider* slider) {
    if (updatingFromModel) return;

    if (slider == &probSlider && selectedClip) {
        selectedClip->probability = (float)(probSlider.getValue() / 100.0);
        // Keep effective probability label in sync during drag
        if (selectedBlock) {
            float totalWeight = 0.0f;
            for (auto* c : selectedBlock->clips)
                if (!c->isDone) totalWeight += c->probability;
            float eff = (totalWeight > 0.0f)
                      ? (selectedClip->probability / totalWeight) * 100.0f
                      : 0.0f;
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

    // Link sliders: update value directly (no undo, no sendChangeMessage during drag).
    // The undo snapshot is recorded in sliderDragEnded.
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

    // For link sliders: the value was already updated live in sliderValueChanged.
    // Use the pre-snapshot captured at drag start to record the undo action.
    for (auto* row : linkRows) {
        if (slider == &row->slider) {
            if (project && !row->dragPre.isVoid()) {
                project->applyExternalMutation(row->dragPre);
                row->dragPre = juce::var{};
            }
            return;
        }
    }
}

void InspectorPanel::buttonClicked(juce::Button* btn) {
    if (updatingFromModel) return;

    // Block-level buttons don't require a selected clip
    if (btn == &simultaneousToggle && selectedBlock && project) {
        auto pre = project->toJSON();
        selectedBlock->stackPlayMode = simultaneousToggle.getToggleState()
                                       ? StackPlayMode::Simultaneous
                                       : StackPlayMode::Sequential;
        project->applyExternalMutation(pre);
        return;
    }
    if (btn == &blockDoneToggle && selectedBlock && project) {
        auto pre = project->toJSON();
        selectedBlock->isDone = blockDoneToggle.getToggleState();
        project->applyExternalMutation(pre);
        return;
    }

    // Plays-over toggle rows
    for (auto* row : playsOverRows) {
        if (btn == &row->toggle && selectedBlock && project) {
            bool isChecked = row->toggle.getToggleState();
            auto pre = project->toJSON();

            if (isChecked) {
                // Adding this clip to the allowed set
                if (!selectedBlock->allowedParentClipIds.contains(row->clipId))
                    selectedBlock->allowedParentClipIds.add(row->clipId);
                // If all sibling clips are now in the list, clear it (empty = allow all)
                bool allCovered = true;
                for (auto* r : playsOverRows)
                    if (!selectedBlock->allowedParentClipIds.contains(r->clipId))
                        { allCovered = false; break; }
                if (allCovered)
                    selectedBlock->allowedParentClipIds.clear();
            } else {
                // Removing this clip from the allowed set
                if (selectedBlock->allowedParentClipIds.isEmpty()) {
                    // Was allowing all — now exclude only this clip
                    for (auto* r : playsOverRows)
                        if (r->clipId != row->clipId &&
                            !selectedBlock->allowedParentClipIds.contains(r->clipId))
                            selectedBlock->allowedParentClipIds.add(r->clipId);
                } else {
                    selectedBlock->allowedParentClipIds.removeString(row->clipId);
                }
            }

            project->applyExternalMutation(pre);

            // Immediately refresh checkboxes (e.g. auto-clear case)
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
    else if (btn == &retainTail)      selectedClip->retainTailTempo   = retainTail    .getToggleState();
    else if (btn == &songEnderToggle) selectedClip->isSongEnder       = songEnderToggle.getToggleState();
    else if (btn == &clipDoneToggle)  selectedClip->isDone             = clipDoneToggle .getToggleState();
    else return;  // unknown button — don't record a spurious undo action
    project->applyExternalMutation(pre);
}

void InspectorPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgMedium));
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    g.drawLine(0.0f, 0.0f, 0.0f, (float)getHeight(), 1.0f);

}

void InspectorPanel::resized() {
    auto area = getLocalBounds().reduced(8, 8);
    const int rh  = 22;
    const int slh = 28;
    const int gap = 4;
    const int sec = 10;

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

    songEnderToggle.setBounds(area.removeFromTop(rh));
    area.removeFromTop(2);
    clipDoneToggle .setBounds(area.removeFromTop(rh));
    area.removeFromTop(2);
    retainLeadIn   .setBounds(area.removeFromTop(rh));
    area.removeFromTop(2);
    retainTail     .setBounds(area.removeFromTop(rh));
    area.removeFromTop(sec);

    blockTitle    .setBounds(area.removeFromTop(rh));
    area.removeFromTop(gap);
    blockDoneToggle.setBounds(area.removeFromTop(rh));
    area.removeFromTop(2);
    overlapLabel .setBounds(area.removeFromTop(rh));
    overlapSlider.setBounds(area.removeFromTop(slh));
    area.removeFromTop(gap);

    // "Plays Over" section
    if (playsOverTitle.isVisible()) {
        playsOverTitle.setBounds(area.removeFromTop(rh));
        area.removeFromTop(2);
        if (playsOverHint.isVisible()) {
            // Two-line hint — give it a bit of extra height
            playsOverHint.setBounds(area.removeFromTop(rh * 2));
            area.removeFromTop(gap);
        } else {
            for (auto* row : playsOverRows) {
                if (row->toggle.isVisible()) {
                    row->toggle.setBounds(area.removeFromTop(rh));
                    area.removeFromTop(2);
                }
            }
            area.removeFromTop(gap - 2);
        }
    }

    // Stack controls
    const bool inStack = (selectedBlock != nullptr && selectedBlock->stackGroup >= 0);
    if (inStack) {
        stackPlayCountLabel.setBounds(area.removeFromTop(rh));
        area.removeFromTop(2);

        for (auto* row : stackCountRows) {
            // Single row: [dec 22][count 26][inc 22] [weight slider] [× 22]
            auto rowArea = area.removeFromTop(rh);
            row->decBtn   .setBounds(rowArea.removeFromLeft(22));
            row->countLbl .setBounds(rowArea.removeFromLeft(26));
            row->incBtn   .setBounds(rowArea.removeFromLeft(22));
            row->removeBtn.setBounds(rowArea.removeFromRight(22));
            row->weightSlider.setBounds(rowArea.reduced(2, 0));
            area.removeFromTop(2);
        }

        addStackCountBtn.setBounds(area.removeFromTop(rh));
        area.removeFromTop(gap);

        stackPlayModeLabel .setBounds(area.removeFromTop(rh));
        simultaneousToggle .setBounds(area.removeFromTop(rh));
        area.removeFromTop(gap);
    }
    area.removeFromTop(sec - gap);

    linksTitle.setBounds(area.removeFromTop(rh));
    area.removeFromTop(gap);
    for (auto* row : linkRows) {
        row->label .setBounds(area.removeFromTop(rh));
        row->slider.setBounds(area.removeFromTop(slh));
        area.removeFromTop(gap);
    }
}

} // namespace BlockShuffler
