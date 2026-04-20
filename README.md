# BlockShuffler

A desktop audio tool for randomizing song arrangements. Split songs into blocks, assign weighted clips, stack and link blocks, then export losslessly for playback.

## Getting Started

```bash
# Clone with JUCE submodule
git clone --recursive https://github.com/gobi10k/BlockShuffler.git
cd BlockShuffler

# If you already cloned without --recursive:
git submodule add https://github.com/juce-framework/JUCE.git libs/JUCE
git submodule update --init

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target BlockShuffler_Standalone
```

## Development

See **[CLAUDE.md](CLAUDE.md)** for the full architecture spec, implementation phases, and Claude Code handoff guide.

## License

TBD
