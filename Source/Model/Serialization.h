#pragma once
#include <juce_core/juce_core.h>

namespace BlockShuffler {

class Project;

/**
 * JSON serialization for Project save/load and .bsf export manifests.
 * See CLAUDE.md for JSON schema details.
 */
namespace Serialization {
    // baseDir: when valid, audio file paths are written/read relative to this directory.
    // Pass {} (default) for undo/redo snapshots where absolute paths are required.
    juce::var projectToJSON(const Project& project, const juce::File& baseDir = {});
    bool projectFromJSON(const juce::var& json, Project& project, const juce::File& baseDir = {});

    juce::var exportManifestToJSON(const Project& project);
}

} // namespace BlockShuffler
