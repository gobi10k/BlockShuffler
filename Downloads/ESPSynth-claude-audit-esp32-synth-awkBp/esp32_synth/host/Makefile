# host/Makefile — desktop build for the ESPSynth DSP core.
# Build from repo root:  make -C host
# Build from this dir:   make
#
# Produces: host/espsynth_host
# Usage:    ./espsynth_host   →  writes out.wav in the current directory

CXX      ?= c++
CXXFLAGS  = -std=c++17 -O2 -DHOST_BUILD -Wall \
            -I. -I..
LDFLAGS   = -lm

SRCDIR = ..

# DSP files compiled for the host build.
# Excluded: MIDIHandler, DisplayManager, HardwareEncoder, AnalogControls,
#           SDManager, PresetManager, SynthesisTests (all hardware-specific).
SRCS = \
    main.cpp \
    Analysis.cpp \
    $(SRCDIR)/Arpeggiator.cpp \
    $(SRCDIR)/CombFilter.cpp \
    $(SRCDIR)/Compressor.cpp \
    $(SRCDIR)/Effects.cpp \
    $(SRCDIR)/Envelope.cpp \
    $(SRCDIR)/Euclidean.cpp \
    $(SRCDIR)/Filter.cpp \
    $(SRCDIR)/Granular.cpp \
    $(SRCDIR)/LFO.cpp \
    $(SRCDIR)/ModMatrix.cpp \
    $(SRCDIR)/MoogFilter.cpp \
    $(SRCDIR)/Oscillator.cpp \
    $(SRCDIR)/Resonator.cpp \
    $(SRCDIR)/Reverb.cpp \
    $(SRCDIR)/Synthesis.cpp \
    $(SRCDIR)/SynthEngine.cpp \
    $(SRCDIR)/Voice.cpp \
    $(SRCDIR)/Wavetables.cpp

TARGET = espsynth_host

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET) out.wav

.PHONY: clean
