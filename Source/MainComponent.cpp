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

    addAndMakeVisible(waveformView);
    addAndMakeVisible(blockStrip);
    addAndMakeVisible(linkOverlay);
    addAndMakeVisible(inspectorPanel);
    addAndMakeVisible(transportBar);

    linkOverlay.setAlwaysOnTop(true);
    linkOverlay.setInterceptsMouseClicks(false, false);

    addKeyListener(this);

    auto* defaultBlock = project->addBlock("Block 1");
    // The initial block creation should not be in undo history — Cmd+Z on the very
    // first user action should undo that action, not remove the startup block.
    project->undoManager.clearUndoHistory();
    blockStrip.selectBlock(defaultBlock);  // fires onBlockSelected → applyBlockSelection
}

MainComponent::~MainComponent() {
    removeKeyListener(this);
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
    inspectorPanel.setBounds(area.removeFromRight(inspectorWidth));
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
                selectedBlock->addClip(std::move(clip));
                anyAdded = true;
            }
        }
    }
    if (anyAdded)
        project->applyExternalMutation(pre);
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* /*source*/) {
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

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* originator) {
    // Arrow keys: forward to waveformView regardless of which child has focus,
    // unless a text editor has focus (it needs them for cursor movement).
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey) {
        if (dynamic_cast<juce::TextEditor*>(originator) == nullptr)
            return waveformView.keyPressed(key);
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
            juce::ModalCallbackFunction::create([this](int result) {
                if (result == 1) { // OK
                    engine.stop();
                    // Clear UI state BEFORE destroying old project to avoid dangling pointers.
                    // waveformView holds a Block* (currentBlock) and ClipRowComponents hold Clip&
                    // refs — clearing them while the old project still exists prevents a crash in
                    // ClipWaveformView::setBlock's removeChangeListener call on freed memory.
                    selectedBlock   = nullptr;
                    selectedBlockId = {};
                    waveformView.setBlock(nullptr, 48000.0);
                    inspectorPanel.setClip(nullptr, nullptr);
                    inspectorPanel.setBlock(nullptr);
                    project->removeChangeListener(this);
                    project = std::make_unique<Project>();
                    project->name = "Untitled Project";
                    project->addChangeListener(this);
                    currentProjectFile = juce::File{};
                    inspectorPanel.setProject(project.get());
                    blockStrip.init(*project, &linkOverlay);
                    auto* b = project->addBlock("Block 1");
                    project->undoManager.clearUndoHistory();
                    blockStrip.selectBlock(b);  // fires onBlockSelected → applyBlockSelection
                }
            })
        );
        return true;
    }
    return false;
}

void MainComponent::onPlayPressed() {
    if (engine.isPlaying()) {
        engine.stop();
        transportBar.setIsPlaying(false);
        blockStrip.setPlayingBlock({});
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
}

void MainComponent::updateTimeDisplay() {
    double current = engine.getPlayheadSeconds();
    double total   = engine.getTotalSeconds();
    transportBar.setTimeDisplay(current, total);

    const bool playing = engine.isPlaying();
    if (!playing) {
        transportBar.setIsPlaying(false);
        blockStrip.setPlayingBlock({});
    } else {
        // Find which entry is at the current playhead position
        int64_t headSamples = (int64_t)(current * currentArrangement.sampleRate);
        juce::String nowPlayingId;
        for (const auto& entry : currentArrangement.entries) {
            int64_t bodyLen = entry.clip->endMark - entry.clip->startMark;
            if (headSamples >= entry.timelinePos &&
                headSamples <  entry.timelinePos + bodyLen) {
                nowPlayingId = entry.blockId;
                break;
            }
        }
        blockStrip.setPlayingBlock(nowPlayingId);
    }

    float fraction = (total > 0.0) ? (float)(current / total) : 0.0f;
    waveformView.setPlayheadFraction(playing ? fraction : -1.0f);
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
                            "All blocks are marked as Done. Unmark at least one block to export.");
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
