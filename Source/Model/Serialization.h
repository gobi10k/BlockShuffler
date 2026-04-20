#pragma once
#include <juce_core/juce_core.h>

namespace BlockShuffler {

class Project;

/**
 * JSON serialization for Project save/load and .bsf export manifests.
 * See CLAUDE.md for JSON schema details.
 */
namespace Serialization {
    juce::var projectToJSON(const Project& project);
    bool projectFromJSON(const juce::var& json, Project& project);

    juce::var exportManifestToJSON(const Project& project);
}

} // namespace BlockShuffler
