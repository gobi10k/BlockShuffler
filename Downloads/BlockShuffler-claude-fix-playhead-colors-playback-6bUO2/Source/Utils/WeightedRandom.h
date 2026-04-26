#pragma once
#include <juce_core/juce_core.h>

namespace BlockShuffler {

/**
 * Weighted random selection utility.
 * Used throughout the app for clip probability, stack count, overlap chances.
 */
template<typename T>
struct WeightedValue {
    juce::Array<T> values;
    juce::Array<float> weights;

    bool isValid() const { return values.size() > 0 && values.size() == weights.size(); }

    T pick(juce::Random& rng) const {
        jassert(isValid());
        float total = 0.0f;
        for (auto w : weights) total += w;

        float roll = rng.nextFloat() * total;
        float cumulative = 0.0f;
        for (int i = 0; i < values.size(); ++i) {
            cumulative += weights[i];
            if (roll <= cumulative)
                return values[i];
        }
        return values.getLast();
    }

    /** Pick without replacement — returns `count` items in random weighted order. */
    juce::Array<T> sample(int count, juce::Random& rng) const {
        jassert(isValid());
        juce::Array<T> result;
        juce::Array<T> remaining = values;
        juce::Array<float> remainingWeights = weights;

        count = juce::jmin(count, remaining.size());
        for (int n = 0; n < count; ++n) {
            float total = 0.0f;
            for (auto w : remainingWeights) total += w;

            float roll = rng.nextFloat() * total;
            float cumulative = 0.0f;
            for (int i = 0; i < remaining.size(); ++i) {
                cumulative += remainingWeights[i];
                if (roll <= cumulative) {
                    result.add(remaining[i]);
                    remaining.remove(i);
                    remainingWeights.remove(i);
                    break;
                }
            }
        }
        return result;
    }
};

} // namespace BlockShuffler
