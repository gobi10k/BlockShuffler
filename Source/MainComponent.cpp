#include "MainComponent.h"

namespace BlockShuffler {

MainComponent::MainComponent(PlaybackEngine& eng)
    : engine(eng)
{
    setLookAndFeel(&customLookAndFeel);
    setWantsKeyboardFocus(true);

    project = std::make_unique<Project>();
    project->name = "Untitled Project";
    project->addChangeListener(this);

    inspectorPanel.setProject(project.get());

    blockStrip.init(*project, &linkOverlay);
    blockStrip.onBlockSelected = [this](Block* block) {
        applyBlockSelection(block);
    };
    waveformView.defaultTempo = project->defaultClipTempo;

    waveformView.onClipSelected = [this](Clip* clip) {
        inspectorPanel.setClip(clip, selectedBlock);
    };
    inspectorPanel.onClipProbabilityChanged = [this] { waveformView.repaint(); };
    waveformView.onCaptureSnapshot  = [this] { return project ? project->toJSON() : juce::var{}; };
    waveformView.onUndoableMutation = [this](const juce::var& pre) {
        if (project) project->applyExternalMutation(pre);
    };

    transportBar.onPlay   = [this] { onPlayPressed();   };
    transportBar.onStop   = [this] { onStopPressed();   };
    transportBar.onRewind = [this] { onRewindPressed(); };
    transportBar.onExport = [this] { exportProject(); };
    transportBar.onSave   = [this] { saveProject(); };
    transportBar.onOpen   = [this] { openProject(); };

    blockStrip.onClipDropped = [this](const juce::String& clipId, const juce::String& targetBlockId) {
        if (!project) return;
        // Find the clip and its source block
        Block* sourceBlock = nullptr;
        Clip*  movedClip   = nullptr;
        for (auto* b : project->blocks) {
            for (auto* c : b->clips) {
                if (c->id == clipId) { sourceBlock = b; movedClip = c; break; }
            }
            if (movedClip) break;
        }
        auto* targetBlock = project->getBlockById(targetBlockId);
        if (!movedClip || !sourceBlock || !targetBlock || sourceBlock == targetBlock) return;

        auto pre = project->toJSON();
        // Transfer ownership: take clip out of source, add to target.
        // Also update the clip's tempo to match the target block so it plays on the
        // correct grid, then fire change messages on both blocks so their waveform
        // views rebuild immediately (the clip disappears from the source row and
        // appears in the target row without waiting for a project-level rebuild).
        bool moved = false;
        for (int i = 0; i < sourceBlock->clips.size(); ++i) {
            if (sourceBlock->clips[i] == movedClip) {
                Clip* rawClip = sourceBlock->clips.removeAndReturn(i);
                rawClip->tempo = targetBlock->tempo > 0.0 ? targetBlock->tempo : rawClip->tempo;
                targetBlock->clips.add(rawClip);
                moved = true;
                break;
            }
        }
        if (moved) {
            sourceBlock->sendChangeMessage();  // source waveform view drops the clip row
            targetBlock->sendChangeMessage();  // target waveform view gains the clip row
        }
        project->applyExternalMutation(pre);
    };

    blockStrip.onPlayBlockRequested = [this](const juce::String& blockId) {
        playBlock(blockId);
    };

    waveformView.onPlayClipRequested = [this](const juce::String& clipId) {
        playClip(clipId);
    };

    blockStrip.onPlayFromHereRequested = [this](const juce::String& blockId) {
        currentArrangement = resolver.resolve(*project, rng);
        // Find the body start of the target block in the resolved arrangement
        int64_t seekPos = 0;
        for (const auto& entry : currentArrangement.entries) {
            if (entry.blockId == blockId) {
                seekPos = entry.timelinePos;  // timelinePos = body start
                break;
            }
        }
        engine.play(currentArrangement);
        engine.seekTo(seekPos);
        transportBar.setIsPlaying(true);
    };

    addAndMakeVisible(waveformView);
    addAndMakeVisible(blockStrip);
    addAndMakeVisible(linkOverlay);
    addAndMakeVisible(transportBar);

    inspectorViewport.setViewedComponent(&inspectorPanel);
    inspectorViewport.setScrollBarsShown(true, false, false, false);
    inspectorViewport.setColour(juce::ScrollBar::backgroundColourId,
                               juce::Colour(LookAndFeel_BlockShuffler::bgMedium));
    inspectorViewport.setColour(juce::ScrollBar::thumbColourId,
                               juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    addAndMakeVisible(inspectorViewport);

    linkOverlay.setAlwaysOnTop(true);
    linkOverlay.setInterceptsMouseClicks(false, false);

    auto* defaultBlock = project->addBlock("Block 1");
    // The initial block creation should not be in undo history — Cmd+Z on the very
    // first user action should undo that action, not remove the startup block.
    project->undoManager.clearUndoHistory();
    blockStrip.selectBlock(defaultBlock);  // fires onBlockSelected → applyBlockSelection

    // Transport display refresh is driven by the MainWindow timer at 30fps.
}

MainComponent::~MainComponent() {
    project->removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void MainComponent::applyBlockSelection(Block* block) {
    selectedBlock   = block;
    selectedBlockId = block ? block->id : juce::String{};
    waveformView.setBlock(block, project->sampleRate, block ? &project->formatManager : nullptr);
    inspectorPanel.setBlock(block);
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgDark));
}

void MainComponent::resized() {
    if (auto* topLevel = getTopLevelComponent()) {
        auto bounds = topLevel->getBounds();
        bool needsResize = false;
        if (bounds.getWidth()  < 800) { bounds.setWidth(800);  needsResize = true; }
        if (bounds.getHeight() < 600) { bounds.setHeight(600); needsResize = true; }
        if (needsResize)
            topLevel->setBounds(bounds);
    }
    auto area = getLocalBounds();
    transportBar  .setBounds(area.removeFromBottom(transportHeight));
    inspectorViewport.setBounds(area.removeFromRight(inspectorWidth));

    // Set inspector panel size to accommodate all content (including stack settings)
    // Width matches viewport, height is enough for full content with some margin
    int panelHeight = juce::jmax(getHeight() - transportHeight + 200, 800);
    inspectorPanel.setBounds(0, 0, inspectorWidth, panelHeight);

    auto blockArea = area.removeFromBottom(blockStripHeight);
    blockStrip .setBounds(blockArea);
    linkOverlay.setBounds(blockArea);
    waveformView.setBounds(area);
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files) {
    for (auto& f : files) {
        auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif" ||
            ext == ".flac" || ext == ".ogg"  || ext == ".mp3")
            return true;
    }
    return false;
}

void MainComponent::filesDropped(const juce::StringArray& files, int x, int y) {
    // Try to find the block tile the user dropped onto; fall back to selected block
    auto stripPt = blockStrip.getLocalPoint(this, juce::Point<int>(x, y));
    Block* dropTarget = blockStrip.getBlockAtLocalPoint(stripPt);
    if (dropTarget) {
        // Select the drop-target block so the user can see where files landed
        blockStrip.selectBlock(dropTarget);  // fires onBlockSelected → applyBlockSelection
    } else if (!selectedBlock) {
        if (project->blocks.isEmpty()) project->addBlock("Block 1");
        blockStrip.selectBlock(project->blocks.getFirst());  // fires onBlockSelected → applyBlockSelection
    }
    auto pre = project->toJSON();
    bool anyAdded = false;
    for (auto& filePath : files) {
        juce::File file(filePath);
        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif" ||
            ext == ".flac" || ext == ".ogg"  || ext == ".mp3") {
            auto clip = std::make_unique<Clip>();
            if (clip->loadFromFile(file, project->formatManager, project->sampleRate)) {
                // Block tempo takes priority; fall back to project default.
                clip->tempo = (selectedBlock->tempo > 0.0)
                              ? selectedBlock->tempo
                              : (project->defaultClipTempo > 0.0 ? project->defaultClipTempo : 120.0);
                selectedBlock->addClip(std::move(clip));
                anyAdded = true;
            }
        }
    }
    if (anyAdded)
        project->applyExternalMutation(pre);
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* /*source*/) {
    // Keep waveformView in sync with project default tempo (may have changed via undo/redo)
    waveformView.defaultTempo = project->defaultClipTempo;

    // Re-validate the selected block pointer — undo/redo may have deleted/recreated it.
    auto* found = project->getBlockById(selectedBlockId);
    if (found != selectedBlock) {
        // Block changed (deleted, recreated, or new project) — full refresh
        selectedBlock = found;
        waveformView.setBlock(found, project->sampleRate, found ? &project->formatManager : nullptr);
        inspectorPanel.setBlock(selectedBlock);
    } else {
        // Same block, possibly different values/link structure — lightweight refresh
        inspectorPanel.refreshValues();
    }
    repaint();
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
    // Arrow keys: forward to waveformView regardless of which child has focus
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey) {
        return waveformView.keyPressed(key);
    }

    if (key == juce::KeyPress(juce::KeyPress::spaceKey, juce::ModifierKeys::shiftModifier, 0)) {
        if (selectedBlock) playBlock(selectedBlock->id);
        return true;
    }
    if (key == juce::KeyPress(juce::KeyPress::spaceKey)) {
        onPlayPressed();
        return true;
    }
    if (key.isKeyCode('S') && key.getModifiers().isCommandDown()) {
        saveProject();
        return true;
    }
    if (key.isKeyCode('O') && key.getModifiers().isCommandDown()) {
        openProject();
        return true;
    }
    if (key == juce::KeyPress(juce::KeyPress::escapeKey)) {
        blockStrip.cancelPendingMode();
        return true;
    }
    if (key.isKeyCode('Z') && key.getModifiers().isCommandDown() &&
        !key.getModifiers().isShiftDown()) {
        project->undoManager.undo();
        return true;
    }
    if (key.isKeyCode('Z') && key.getModifiers().isCommandDown() &&
        key.getModifiers().isShiftDown()) {
        project->undoManager.redo();
        return true;
    }
    if (key.isKeyCode('Y') && key.getModifiers().isCommandDown()) {
        project->undoManager.redo();
        return true;
    }
    if (key.isKeyCode('N') && key.getModifiers().isCommandDown()) {
        juce::NativeMessageBox::showOkCancelBox(
            juce::MessageBoxIconType::QuestionIcon,
            "New Project",
            "Are you sure you want to create a new project? Any unsaved changes will be lost.",
            nullptr,
            juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<MainComponent>(this)](int result) {
                if (result == 1 && safe) { // OK
                    auto* self = safe.getComponent();
                    self->engine.stop();
                    // Clear UI state BEFORE destroying old project to avoid dangling pointers.
                    // waveformView holds a Block* (currentBlock) and ClipRowComponents hold Clip&
                    // refs — clearing them while the old project still exists prevents a crash in
                    // ClipWaveformView::setBlock's removeChangeListener call on freed memory.
                    self->selectedBlock   = nullptr;
                    self->selectedBlockId = {};
                    self->waveformView.setBlock(nullptr, 48000.0);
                    self->inspectorPanel.setClip(nullptr, nullptr);
                    self->inspectorPanel.setBlock(nullptr);
                    self->project->removeChangeListener(self);
                    self->project = std::make_unique<Project>();
                    self->project->name = "Untitled Project";
                    self->project->addChangeListener(self);
                    self->currentProjectFile = juce::File{};
                    self->inspectorPanel.setProject(self->project.get());
                    self->blockStrip.init(*(self->project), &(self->linkOverlay));
                    auto* b = self->project->addBlock("Block 1");
                    self->project->undoManager.clearUndoHistory();
                    self->blockStrip.selectBlock(b);  // fires onBlockSelected → applyBlockSelection
                }
            })
        );
        return true;
    }
    return false;
}

void MainComponent::playBlock(const juce::String& blockId) {
    if (!project) return;
    auto* block = project->getBlockById(blockId);
    if (!block || block->clips.isEmpty()) return;

    // Always stop before starting — prevents the engine ignoring a new play()
    // call while the audio thread still thinks it is playing.
    engine.stop();

    // Pick a clip by weighted selection directly. Using the full probabilistic
    // resolver here caused ~50% silent failures: the resolver might not include
    // this block (e.g. a link swap placed a different block in its slot, or
    // playChance < 1 skipped it), so single.isEmpty() triggered an early return.
    auto* clip = ArrangementResolver::pickClip(*block, rng);
    if (!clip || !clip->audioBuffer || clip->audioBuffer->getNumSamples() == 0)
        return;

    ResolvedEntry entry;
    entry.audioBuffer        = clip->audioBuffer;
    entry.startMark          = clip->startMark;
    entry.endMark            = clip->endMark;
    entry.originalStartMark  = clip->startMark;
    entry.clipId             = clip->id;
    entry.clipName           = clip->name;
    entry.blockId            = blockId;
    entry.timelinePos        = 0;
    entry.gain               = 1.0f;
    entry.isOverlay          = false;

    ResolvedArrangement single;
    single.sampleRate           = project->sampleRate;
    single.totalDurationSamples = clip->endMark - clip->startMark;
    single.entries.add(entry);

    currentArrangement = std::move(single);
    engine.play(currentArrangement);
    transportBar.setIsPlaying(true);
}

void MainComponent::playClip(const juce::String& clipId) {
    if (!project) return;
    Clip* found = nullptr;
    juce::String foundBlockId;
    for (auto* b : project->blocks) {
        for (auto* c : b->clips) {
            if (c->id == clipId) { found = c; foundBlockId = b->id; break; }
        }
        if (found) break;
    }
    if (!found || !found->audioBuffer) return;

    ResolvedEntry entry;
    entry.audioBuffer   = found->audioBuffer;
    entry.startMark     = found->startMark;
    entry.endMark       = found->endMark;
    entry.originalStartMark = found->startMark;
    entry.clipId        = found->id;
    entry.clipName      = found->name;
    entry.blockId       = foundBlockId;
    entry.timelinePos   = 0;
    entry.gain          = 1.0f;

    ResolvedArrangement single;
    single.sampleRate           = project->sampleRate;
    single.totalDurationSamples = found->endMark - found->startMark;
    single.entries.add(entry);

    currentArrangement = std::move(single);
    engine.play(currentArrangement);
    transportBar.setIsPlaying(true);
}

void MainComponent::onPlayPressed() {
    if (engine.isPlaying()) {
        engine.stop();
        transportBar.setIsPlaying(false);
        blockStrip.setPlayingBlock({});
        waveformView.setPlayingClip({}, 0, 0.0);
    } else {
        currentArrangement = resolver.resolve(*project, rng);
        engine.play(currentArrangement);  // engine takes a copy
        transportBar.setIsPlaying(true);
    }
}

void MainComponent::onStopPressed() {
    engine.stop();
    engine.rewind();
    transportBar.setIsPlaying(false);
    transportBar.setTimeDisplay(0.0, engine.getTotalSeconds());
    blockStrip.setPlayingBlock({});
    waveformView.setPlayingClip({}, 0, 0.0);
    if (lastPlayingBlock != nullptr) {
        lastPlayingBlock = nullptr;
        waveformView.setBlock(selectedBlock, project->sampleRate,
                              selectedBlock ? &project->formatManager : nullptr);
        inspectorPanel.setBlock(selectedBlock);
    }
}

void MainComponent::onRewindPressed() {
    engine.rewind();
    transportBar.setTimeDisplay(0.0, engine.getTotalSeconds());
}

void MainComponent::saveProject() {
    if (currentProjectFile.existsAsFile()) {
        project->saveToFile(currentProjectFile);
    } else {
        saveProjectAs();
    }
}

void MainComponent::saveProjectAs() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(project->name + ".bsp"),
        "*.bsp");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                         juce::FileBrowserComponent::canSelectFiles |
                         juce::FileBrowserComponent::warnAboutOverwriting,
        [this, chooser](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result != juce::File{}) {
                auto f = result.withFileExtension(".bsp");
                if (project->saveToFile(f))
                    currentProjectFile = f;
            }
        });
}

void MainComponent::openProject() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Open Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.bsp");

    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                         juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.existsAsFile())
                loadProject(result);
        });
}

void MainComponent::loadProject(const juce::File& file) {
    engine.stop();

    auto newProject = std::make_unique<Project>();
    if (!newProject->loadFromFile(file)) return;

    // Warn about missing audio files before swapping the project in
    if (!newProject->missingFilesOnLoad.isEmpty()) {
        juce::String msg = "The following audio files could not be found and will be silent:\n\n"
                         + newProject->missingFilesOnLoad.joinIntoString("\n");
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Missing Audio Files",
            msg);
    }

    project->removeChangeListener(this);
    project = std::move(newProject);
    project->undoManager.clearUndoHistory();  // don't allow undoing back into the previous project
    project->addChangeListener(this);
    currentProjectFile = file;
    inspectorPanel.setProject(project.get());

    selectedBlock   = nullptr;
    selectedBlockId = {};
    blockStrip.init(*project, &linkOverlay);
    waveformView.setBlock(nullptr, project->sampleRate);
    inspectorPanel.setClip(nullptr, nullptr);

    if (!project->blocks.isEmpty()) {
        blockStrip.selectBlock(project->blocks.getFirst());  // fires onBlockSelected → applyBlockSelection
    }
    waveformView.defaultTempo = project->defaultClipTempo;
    project->sendChangeMessage();  // ensures BlockStrip runs its async rebuildBlocks()+resized()
}

void MainComponent::updateTimeDisplay() {
    double current = engine.getPlayheadSeconds();
    double total   = engine.getTotalSeconds();
    transportBar.setTimeDisplay(current, total);

    if (!engine.isPlaying()) {
        transportBar.setIsPlaying(false);
        blockStrip.setPlayingBlock({});
        waveformView.setPlayingClip({}, 0, 0.0);
        if (lastPlayingBlock != nullptr) {
            lastPlayingBlock = nullptr;
            waveformView.setBlock(selectedBlock, project->sampleRate,
                                  selectedBlock ? &project->formatManager : nullptr);
            inspectorPanel.setBlock(selectedBlock);
        }
        return;
    }

    const double sr = currentArrangement.sampleRate;
    int64_t headSamples = (sr > 0.0) ? (int64_t)(current * sr) : 0;

    juce::String nowPlayingBlockId;
    juce::String nowPlayingClipId;
    int64_t clipSamplePos = 0;

    // Forward scan through primary (non-overlay) entries only.
    // Pick the first entry whose body has not yet ended — this naturally handles:
    //   • lead-in (headSamples < timelinePos): clamp clipSamplePos to startMark
    //   • body (headSamples in [timelinePos, bodyEnd)): compute exact position
    //   • tail / gap (headSamples >= bodyEnd): move on to next entry
    for (const auto& entry : currentArrangement.entries) {
        if (entry.isOverlay) continue;
        int64_t bodyEnd = entry.timelinePos + (entry.endMark - entry.startMark);
        if (headSamples < bodyEnd) {
            nowPlayingBlockId = entry.blockId;
            nowPlayingClipId  = entry.clipId;
            int64_t offsetIntoBody = headSamples - entry.timelinePos;
            clipSamplePos = entry.startMark + juce::jmax((int64_t)0, offsetIntoBody);
            break;
        }
    }

    // Fallback: playhead is past all entries (playing through the tail of the last block).
    if (nowPlayingBlockId.isEmpty()) {
        for (int i = currentArrangement.entries.size() - 1; i >= 0; --i) {
            const auto& e = currentArrangement.entries.getReference(i);
            if (!e.isOverlay) {
                nowPlayingBlockId = e.blockId;
                nowPlayingClipId  = e.clipId;
                clipSamplePos     = e.endMark;
                break;
            }
        }
    }

    // Follow the playing block: switch waveform + inspector when block changes
    if (nowPlayingBlockId.isNotEmpty()) {
        auto* playingBlock = project->getBlockById(nowPlayingBlockId);
        if (playingBlock != nullptr && playingBlock != lastPlayingBlock) {
            lastPlayingBlock = playingBlock;
            waveformView.setBlock(playingBlock, project->sampleRate, &project->formatManager);
            inspectorPanel.setBlock(playingBlock);
        }
    }

    blockStrip.setPlayingBlock(nowPlayingBlockId);
    waveformView.setPlayingClip(nowPlayingClipId, clipSamplePos, sr);
}

namespace {

// Self-deleting export job using launchThread() (works without JUCE_MODAL_LOOPS_PERMITTED).
class ExportJob final : public juce::ThreadWithProgressWindow {
public:
    ExportJob(ResolvedArrangement arr, juce::File f, juce::String ext,
              juce::var snap = {})
        : juce::ThreadWithProgressWindow("Exporting...", true, false),
          arrangement(std::move(arr)), file(std::move(f)), ext(std::move(ext)),
          projectSnapshot(std::move(snap)) {}

    void run() override {
        ExportRenderer renderer;
        auto progressFn = [this](float p) { setProgress((double)p); };

        if (ext == ".bsf") {
            ok = renderer.renderToBsf(arrangement, file, 24, progressFn, projectSnapshot);
        } else {
            juce::WavAudioFormat  wavFmt;
            juce::FlacAudioFormat flacFmt;
            juce::AudioFormat* fmt = (ext == ".flac")
                                   ? (juce::AudioFormat*)&flacFmt
                                   : (juce::AudioFormat*)&wavFmt;
            ok = renderer.renderToFile(arrangement, file, *fmt, 24, progressFn);
        }
    }

    void threadComplete(bool userPressedCancel) override {
        const bool succeeded = !userPressedCancel && ok;
        const juce::File f   = file;

        juce::MessageManager::callAsync([succeeded, f] {
            if (succeeded)
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "Export Complete",
                    "Saved to:\n" + f.getFullPathName());
            else
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Export Failed",
                    "Could not write to:\n" + f.getFullPathName());
        });

        delete this;  // self-destruct after completion
    }

    bool ok = false;

private:
    ResolvedArrangement arrangement;
    juce::File          file;
    juce::String        ext;
    juce::var           projectSnapshot;
};

} // anonymous namespace

void MainComponent::exportProject() {
    // Show a popup menu to choose the export format, then open a file chooser.
    juce::PopupMenu exportMenu;
    exportMenu.addItem(1, "WAV (flat mix)");
    exportMenu.addItem(2, "FLAC (flat mix, lossless)");
    exportMenu.addItem(3, "BSF Bundle (for mobile player)");

    exportMenu.showMenuAsync(juce::PopupMenu::Options{},
        [this](int choice) {
            if (choice == 0) return;  // dismissed

            juce::String filterPattern;
            juce::String defaultExt;
            if      (choice == 1) { filterPattern = "*.wav";  defaultExt = ".wav"; }
            else if (choice == 2) { filterPattern = "*.flac"; defaultExt = ".flac"; }
            else                  { filterPattern = "*.bsf";  defaultExt = ".bsf"; }

            auto chooser = std::make_shared<juce::FileChooser>(
                "Export Arrangement",
                juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                    .getChildFile(project->name + defaultExt),
                filterPattern);

            chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                                 juce::FileBrowserComponent::canSelectFiles |
                                 juce::FileBrowserComponent::warnAboutOverwriting,
                [this, chooser, defaultExt](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result == juce::File{}) return;

                    auto arr = std::make_shared<ResolvedArrangement>(
                        resolver.resolve(*project, rng));

                    if (arr->isEmpty()) {
                        juce::NativeMessageBox::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Nothing to Export",
                            "The arrangement is empty. Add blocks with clips to export.");
                        return;
                    }

                    auto file = result.withFileExtension(defaultExt);

                    // Capture project model snapshot on UI thread for model.json in BSF
                    juce::var snap = (defaultExt == ".bsf") ? project->toJSON() : juce::var{};

                    // ExportJob is heap-allocated and self-deletes in threadComplete()
                    (new ExportJob(std::move(*arr), file, defaultExt, std::move(snap)))->launchThread();
                });
        });
}

} // namespace BlockShuffler
