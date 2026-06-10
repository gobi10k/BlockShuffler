#include "InspectorPanel.h"

namespace BlockShuffler {

// ── Helpers ───────────────────────────────────────────────────────────────────

static void setupLabel(juce::Component* parent, juce::Label& lbl,
                       const juce::String& text, float fontSize, bool dim = false)
{
    lbl.setText(text, juce::dontSendNotification);
    if (dim) {
        // Section header style: small-caps approximation via Inter Bold at 10px in tertiary color
        lbl.setFont(LookAndFeel_BlockShuffler::uiFontBold(10.0f));
        lbl.setColour(juce::Label::textColourId,
                      juce::Colour(LookAndFeel_BlockShuffler::textTertiary));
    } else {
        lbl.setFont(LookAndFeel_BlockShuffler::uiFont(fontSize));
        lbl.setColour(juce::Label::textColourId,
                      juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
    }
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

    effectiveProbLabel.setFont(LookAndFeel_BlockShuffler::monoFont(11.0f));
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
    setupLabel(this, blockTitle,      "BLOCK",          11.0f, true);
    setupLabel(this, blockTempoLabel, "Block Tempo (BPM)", 12.0f);

    blockTempoField.onValueChanged = [this](double t) {
        if (selectedBlock && !updatingFromModel && project) {
            if (t > 0.0) {
                auto pre = project->toJSON();
                // Store the block-level tempo so new clips added later inherit it,
                // and update all current clips so they all play to the same grid.
                selectedBlock->tempo = t;
                for (auto* c : selectedBlock->clips)
                    c->tempo = t;
                project->applyExternalMutation(pre);
            }
        }
    };
    blockTempoField.setTooltip("Set the tempo for all clips in this block (new clips will inherit this tempo)");
    addAndMakeVisible(blockTempoField);

    blockDoneToggle.addListener(this);
    addAndMakeVisible(blockDoneToggle);

    setupLabel(this, playChanceLabel, "Block Chance (%)", 12.0f);

    playChanceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    playChanceSlider.setRange(0.0, 100.0, 1.0);
    playChanceSlider.setValue(100.0, juce::dontSendNotification);
    playChanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    playChanceSlider.setColour(juce::Slider::textBoxTextColourId,
                               juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
    playChanceSlider.setColour(juce::Slider::textBoxBackgroundColourId,
                               juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    playChanceSlider.addListener(this);
    addAndMakeVisible(playChanceSlider);

    // ── Stack settings section ───────────────────────────────────────────────
    stackSectionTitle.setText("STACK SETTINGS", juce::dontSendNotification);
    stackSectionTitle.setFont(LookAndFeel_BlockShuffler::uiFontBold(10.0f));
    stackSectionTitle.setColour(juce::Label::textColourId,
                                juce::Colour(LookAndFeel_BlockShuffler::accentCol));
    addAndMakeVisible(stackSectionTitle);

    stackInfoLabel.setFont(LookAndFeel_BlockShuffler::uiFont(11.0f));
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
        project->propagateStackSettings(selectedBlock->stackGroup, selectedBlock);
        project->applyExternalMutation(pre);
    };
    addAndMakeVisible(playModeCombo);


    // ── Project section (no block selected) ──────────────────────────────────
    setupLabel(this, projectTitle,     "PROJECT",              11.0f, true);
    setupLabel(this, defaultTempoLabel,"Default Clip Tempo (BPM)", 12.0f);

    defaultTempoField.onValueChanged = [this](double t) {
        if (!updatingFromModel && project && t > 0.0) {
            auto pre = project->toJSON();
            project->defaultClipTempo = t;
            // applyExternalMutation records the undo snapshot AND fires sendChangeMessage
            // so MainComponent syncs waveformView.defaultTempo on the next tick.
            project->applyExternalMutation(pre);
        }
    };
    defaultTempoField.setTooltip("Default tempo applied to new clips when they are loaded");
    addAndMakeVisible(defaultTempoField);

    // ── Links section ────────────────────────────────────────────────────────
    setupLabel(this, linksTitle, "LINKS", 11.0f, true);

    // Tooltips
    probSlider     .setTooltip("Relative weight for clip selection within a block.");
    tempoField     .setTooltip("BPM of this clip - sets the tempo grid for the waveform editor");
    songEnderToggle.setTooltip("If this clip plays, the arrangement stops after it ends");
    clipDoneToggle .setTooltip("Mark this clip as done (visual flag only — does not affect playback or export)");
    retainLeadIn   .setTooltip("Play the lead-in at its original speed instead of stretching");
    retainTail     .setTooltip("Play the tail at its original speed instead of stretching");
    blockDoneToggle.setTooltip("Mark this block as done (visual flag only — does not affect playback or export)");
    playChanceSlider.setTooltip("Chance (%) this block is included in the arrangement.");
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
                memberCount != (int)stackBlockProbRows.size()) {
                rebuildStackBlockLabels();
                resized();
            } else {
                recalcStackEffectiveLabels();
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
        juce::String otherName = "Unknown";   // never show a raw UUID
        juce::Colour labelColor = juce::Colour(LookAndFeel_BlockShuffler::textPrimary);
        if (auto* other = project->getBlockById(otherId)) {
            otherName  = other->name;
            labelColor = other->color;
        }

        row->label.setText("<-> " + otherName, juce::dontSendNotification);
        row->label.setFont(LookAndFeel_BlockShuffler::uiFont(12.0f));
        row->label.setColour(juce::Label::textColourId, labelColor);
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
        row->countLbl.setFont(LookAndFeel_BlockShuffler::monoFont(12.0f));
        row->countLbl.setColour(juce::Label::textColourId,
                                juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        row->countLbl.setJustificationType(juce::Justification::centredLeft);

        row->decBtn.onClick = [this, row] {
            if (!selectedBlock || updatingFromModel || !project) return;
            int idx = stackCountRows.indexOf(row);
            if (idx < 0 || idx >= selectedBlock->stackPlayCount.values.size()) return;
            int cur = selectedBlock->stackPlayCount.values[idx];
            if (cur <= 1) return;
            auto pre = project->toJSON();
            selectedBlock->stackPlayCount.values.set(idx, cur - 1);
            project->propagateStackSettings(selectedBlock->stackGroup, selectedBlock);
            project->applyExternalMutation(pre);
        };

        row->incBtn.onClick = [this, row] {
            if (!selectedBlock || updatingFromModel || !project) return;
            int idx = stackCountRows.indexOf(row);
            if (idx < 0 || idx >= selectedBlock->stackPlayCount.values.size()) return;
            // Clamp to total blocks in this stack (isDone is cosmetic —
            // the resolver plays Done blocks, so they count toward the max)
            int maxPlayable = 0;
            for (auto* b : project->blocks)
                if (b->stackGroup == selectedBlock->stackGroup)
                    ++maxPlayable;
            maxPlayable = juce::jmax(1, maxPlayable);
            int cur = selectedBlock->stackPlayCount.values[idx];
            if (cur >= maxPlayable) return;
            auto pre = project->toJSON();
            selectedBlock->stackPlayCount.values.set(idx, cur + 1);
            project->propagateStackSettings(selectedBlock->stackGroup, selectedBlock);
            project->applyExternalMutation(pre);
        };

        row->removeBtn.onClick = [this, row] {
            if (!selectedBlock || !project) return;
            int idx = stackCountRows.indexOf(row);
            if (idx < 0) return;
            auto& spc2 = selectedBlock->stackPlayCount;
            if (spc2.values.size() <= 1) return;
            auto pre = project->toJSON();
            spc2.values.remove(idx);
            spc2.weights.remove(idx);
            project->propagateStackSettings(selectedBlock->stackGroup, selectedBlock);
            project->applyExternalMutation(pre);
            // applyExternalMutation fires sendChangeMessage → refreshValues → rebuild
        };

        addAndMakeVisible(row->decBtn);
        addAndMakeVisible(row->countLbl);
        addAndMakeVisible(row->incBtn);
        addAndMakeVisible(row->removeBtn);
    }
}

void InspectorPanel::rebuildStackBlockLabels() {
    for (auto* row : stackBlockProbRows) {
        removeChildComponent(&row->nameLabel);
        removeChildComponent(&row->probSlider);
        removeChildComponent(&row->effectiveLabel);
    }
    stackBlockProbRows.clear();
    lastBuiltStackGroup = selectedBlock ? selectedBlock->stackGroup : -2;

    if (!selectedBlock || selectedBlock->stackGroup < 0 || !project) return;

    for (auto* b : project->blocks) {
        if (b->stackGroup != selectedBlock->stackGroup) continue;
        bool isThis = (b->id == selectedBlock->id);

        auto* row = stackBlockProbRows.add(new StackBlockProbRow());

        // Name label
        juce::String text = juce::String(juce::CharPointer_UTF8("\xe2\x80\xa2 ")) + b->name;
        if (isThis) text += "  \xe2\x86\x90";
        row->nameLabel.setText(text, juce::dontSendNotification);
        row->nameLabel.setFont(LookAndFeel_BlockShuffler::uiFont(11.0f));
        row->nameLabel.setColour(juce::Label::textColourId,
            isThis ? juce::Colour(LookAndFeel_BlockShuffler::textPrimary)
                   : juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
        addAndMakeVisible(row->nameLabel);

        // Play-chance slider
        row->probSlider.setValue(b->playChance * 100.0, juce::dontSendNotification);
        row->probSlider.setColour(juce::Slider::textBoxTextColourId,
                                  juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        row->probSlider.setColour(juce::Slider::textBoxBackgroundColourId,
                                  juce::Colour(LookAndFeel_BlockShuffler::bgLight));
        row->probSlider.setColour(juce::Slider::thumbColourId,
                                  juce::Colour(LookAndFeel_BlockShuffler::accentCol));
        row->probSlider.onDragStart = [this] {
            if (!updatingFromModel && project)
                stackBlockDragPre = project->toJSON();
        };
        Block* capturedBlock = b;
        row->probSlider.onValueChange = [this, capturedBlock, row] {
            if (updatingFromModel || !project) return;
            capturedBlock->playChance = juce::jlimit(0.0f, 1.0f,
                (float)(row->probSlider.getValue() / 100.0));
            recalcStackEffectiveLabels();
        };
        row->probSlider.onDragEnd = [this] {
            if (updatingFromModel || !project || stackBlockDragPre.isVoid()) return;
            project->applyExternalMutation(stackBlockDragPre);
            stackBlockDragPre = juce::var{};
        };
        addAndMakeVisible(row->probSlider);

        // Effective probability label
        row->effectiveLabel.setFont(LookAndFeel_BlockShuffler::uiFont(10.0f));
        row->effectiveLabel.setColour(juce::Label::textColourId,
                                      juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
        row->effectiveLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(row->effectiveLabel);
    }

    recalcStackEffectiveLabels();
}

void InspectorPanel::recalcStackEffectiveLabels() {
    if (!selectedBlock || selectedBlock->stackGroup < 0 || !project) return;

    // Collect blocks in this stack
    juce::Array<Block*> stackBlocks;
    for (auto* b : project->blocks)
        if (b->stackGroup == selectedBlock->stackGroup)
            stackBlocks.add(b);

    // Determine minimum possible play count to detect "all play" case
    int minPlayCount = 1;
    if (stackBlocks.size() > 0 && stackBlocks[0]->stackPlayCount.isValid()) {
        minPlayCount = stackBlocks[0]->stackPlayCount.values[0];
        for (int v : stackBlocks[0]->stackPlayCount.values)
            minPlayCount = std::min(minPlayCount, v);
    }
    const bool allPlay = (minPlayCount >= stackBlocks.size());

    // Sum of playChance weights
    float total = 0.0f;
    for (auto* b : stackBlocks) total += b->playChance;

    for (int i = 0; i < stackBlocks.size() && i < stackBlockProbRows.size(); ++i) {
        float eff;
        if (allPlay) {
            eff = 1.0f;
        } else if (total <= 0.0f) {
            eff = 1.0f / (float)stackBlocks.size();
        } else {
            eff = stackBlocks[i]->playChance / total;
        }
        stackBlockProbRows[i]->effectiveLabel.setText(
            "eff: " + juce::String(eff * 100.0f, 1) + "%",
            juce::dontSendNotification);
    }
}

// ── Plays-over rows ───────────────────────────────────────────────────────────


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
        float total = 0.0f;
        for (auto* c : selectedBlock->clips)
            total += c->probability;
        float eff = (total > 0.0f)
                  ? (selectedClip->probability / total) * 100.0f
                  : 100.0f / (float)selectedBlock->clips.size();
        effectiveProbLabel.setText(juce::String(eff, 1) + "% effective",
                                   juce::dontSendNotification);
        effectiveProbLabel.setVisible(true);
    } else {
        effectiveProbLabel.setVisible(false);
    }

    // ── Block section
    blockDoneToggle  .setEnabled(hasBlock);
    blockTempoField  .setEnabled(hasBlock);
    blockTempoLabel  .setVisible(hasBlock);
    blockTempoField  .setVisible(hasBlock);
    // Standalone play-chance slider is only shown for non-stacked blocks
    const bool inStackForSlider = hasBlock && selectedBlock->stackGroup >= 0;
    playChanceLabel  .setVisible(hasBlock && !inStackForSlider);
    playChanceSlider .setVisible(hasBlock && !inStackForSlider);
    playChanceSlider .setEnabled(hasBlock && !inStackForSlider);
    if (hasBlock) {
        blockDoneToggle.setToggleState(selectedBlock->isDone, juce::dontSendNotification);
        if (!inStackForSlider)
            playChanceSlider.setValue(selectedBlock->playChance * 100.0, juce::dontSendNotification);
        // Show the block's stored tempo.  This is authoritative: it stays fixed even when
        // individual clip tempos are overridden, and is what new clips inherit.
        // Fall back to project default only if the block tempo is somehow 0.
        double bt = selectedBlock->tempo > 0.0
                    ? selectedBlock->tempo
                    : (project ? project->defaultClipTempo : 120.0);
        blockTempoField.setValue(bt, juce::dontSendNotification);
    }

    // ── Stack settings section
    const bool inStack = hasBlock && selectedBlock->stackGroup >= 0;
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

    // Stack block probability rows
    for (auto* row : stackBlockProbRows) {
        row->nameLabel    .setVisible(inStack);
        row->probSlider   .setVisible(inStack);
        row->effectiveLabel.setVisible(inStack);
    }
    // Sync slider values and effective labels when refreshing from model
    if (inStack) {
        juce::Array<Block*> stackBlocks;
        for (auto* b : project->blocks)
            if (b->stackGroup == selectedBlock->stackGroup)
                stackBlocks.add(b);
        for (int i = 0; i < stackBlocks.size() && i < stackBlockProbRows.size(); ++i)
            stackBlockProbRows[i]->probSlider.setValue(
                stackBlocks[i]->playChance * 100.0, juce::dontSendNotification);
        recalcStackEffectiveLabels();
    }

    // ── Project section (no block selected)
    const bool showProject = !hasBlock;
    projectTitle      .setVisible(showProject);
    defaultTempoLabel .setVisible(showProject);
    defaultTempoField .setVisible(showProject);
    if (showProject && project)
        defaultTempoField.setValue(project->defaultClipTempo, juce::dontSendNotification);

    // ── Links section
    for (auto* row : linkRows) {
        if (auto* link = findLinkForRow(row)) {
            row->slider.setValue(link->swapProbability * 100.0, juce::dontSendNotification);

            // Refresh label text so block renames (and undo/redo) are always current.
            const juce::String otherId = (row->blockA == (selectedBlock ? selectedBlock->id : juce::String{}))
                                         ? row->blockB : row->blockA;
            juce::String otherName = "Unknown";
            juce::Colour labelColor = juce::Colour(LookAndFeel_BlockShuffler::textPrimary);
            if (project) {
                if (auto* other = project->getBlockById(otherId)) {
                    otherName  = other->name;
                    labelColor = other->color;
                }
            }
            row->label.setText("<-> " + otherName, juce::dontSendNotification);
            row->label.setColour(juce::Label::textColourId, labelColor);
        }
    }

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
                total += c->probability;
            float eff = (total > 0.0f)
                      ? (selectedClip->probability / total) * 100.0f
                      : 100.0f / (float)selectedBlock->clips.size();
            effectiveProbLabel.setText(juce::String(eff, 1) + "% effective",
                                       juce::dontSendNotification);
        }
        if (onClipProbabilityChanged) onClipProbabilityChanged();
        return;
    }
    if (slider == &playChanceSlider && selectedBlock) {
        // FIX M7/M10: clamp to [0,1]; apply without recording undo (drag end does that)
        selectedBlock->playChance = juce::jlimit(0.0f, 1.0f, (float)(playChanceSlider.getValue() / 100.0));
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
    else if (slider == &playChanceSlider && selectedBlock && project)
        playChanceSliderDragPre = project->toJSON();
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
    if (slider == &playChanceSlider && selectedBlock && project && !playChanceSliderDragPre.isVoid()) {
        project->applyExternalMutation(playChanceSliderDragPre);
        playChanceSliderDragPre = juce::var{};
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

    // Section header divider lines — 1 px below each visible title
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::borderSubtle));
    auto drawDivider = [&](const juce::Label& title) {
        if (!title.isVisible()) return;
        float y = (float)(title.getBottom() + 1);
        g.drawHorizontalLine((int)y, 8.0f, (float)(getWidth() - 8));
    };
    drawDivider(clipTitle);
    drawDivider(blockTitle);
    drawDivider(stackSectionTitle);
    drawDivider(projectTitle);
    drawDivider(linksTitle);
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
    if (blockTempoLabel.isVisible()) {
        blockTempoLabel.setBounds(area.removeFromTop(rh));
        blockTempoField.setBounds(area.removeFromTop(rh));
        area.removeFromTop(gap);
    }
    blockDoneToggle.setBounds(area.removeFromTop(rh)); area.removeFromTop(2);
    if (playChanceLabel.isVisible()) {  // hidden when block is stacked
        playChanceLabel .setBounds(area.removeFromTop(rh));
        playChanceSlider.setBounds(area.removeFromTop(slh));
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
        for (auto* row : stackBlockProbRows) {
            auto rowArea = area.removeFromTop(rh);
            row->nameLabel    .setBounds(rowArea.removeFromLeft(80));
            row->effectiveLabel.setBounds(rowArea.removeFromRight(56));
            row->probSlider   .setBounds(rowArea.reduced(0, 2));
            area.removeFromTop(2);
        }
        area.removeFromTop(gap);
    }

    area.removeFromTop(sec - gap);

    // ── Project section (shown when no block selected)
    if (projectTitle.isVisible()) {
        projectTitle     .setBounds(area.removeFromTop(rh)); area.removeFromTop(gap);
        defaultTempoLabel.setBounds(area.removeFromTop(rh));
        defaultTempoField.setBounds(area.removeFromTop(rh));
        area.removeFromTop(gap);
    }

    // ── Links section
    linksTitle.setBounds(area.removeFromTop(rh)); area.removeFromTop(gap);
    for (auto* row : linkRows) {
        row->label .setBounds(area.removeFromTop(rh));
        row->slider.setBounds(area.removeFromTop(slh));
        area.removeFromTop(gap);
    }
}

} // namespace BlockShuffler
