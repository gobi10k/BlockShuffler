#pragma once
#include <cmath>
#include <cstdint>

namespace BlockShuffler {

/**
 * Snap a sample position to the nearest grid line defined by tempo.
 * @param samplePos    Position in samples to snap
 * @param tempo        BPM value
 * @param sampleRate   Audio sample rate (e.g. 48000)
 * @param subdivision  Grid subdivision (1 = quarter note, 2 = eighth, 4 = sixteenth)
 * @return             Snapped sample position
 */
inline int64_t snapToGrid(int64_t samplePos, double tempo, double sampleRate, int subdivision = 1) {
    if (tempo <= 0.0 || sampleRate <= 0.0 || subdivision <= 0) return samplePos;
    double samplesPerBeat = (sampleRate * 60.0) / tempo;
    double gridUnit = samplesPerBeat / static_cast<double>(subdivision);
    return static_cast<int64_t>(std::round(static_cast<double>(samplePos) / gridUnit) * gridUnit);
}

/**
 * Get the grid unit size in samples for a given tempo.
 */
inline double gridUnitSamples(double tempo, double sampleRate, int subdivision = 1) {
    if (tempo <= 0.0 || sampleRate <= 0.0 || subdivision <= 0) return 0.0;
    return (sampleRate * 60.0) / (tempo * static_cast<double>(subdivision));
}

/**
 * Nudge a position by N grid units.
 */
inline int64_t nudgeByGridUnits(int64_t samplePos, int units, double tempo, double sampleRate, int subdivision = 1) {
    double gridSize = gridUnitSamples(tempo, sampleRate, subdivision);
    return samplePos + static_cast<int64_t>(std::round(units * gridSize));
}

} // namespace BlockShuffler
