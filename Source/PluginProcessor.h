#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>

inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

struct SampleData
{
    juce::AudioBuffer<float> buf;
    double sr = 44100.0;
    juce::String name;
};

struct Params
{
    int mode = 0, vowel = 0, cats = 1, octave = 0, scale = 0, root = 0, srcRoot = 60, arpRate = 1, arpPat = 0;
    float formant = 1, dur = 0.6f, glide = 0.6f, vib = 0.4f, vibRate = 6, rough = 0.3f, tone = 8000, detune = 12,
          human = 0.2f, width = 0.3f, arpGate = 0.6f, revMix = 0.2f, revSize = 0.5f, gain = 0;
    bool oneShot = false, arp = false;
};

struct Voice
{
    bool active = false, held = false, sampleMode = false;
    int key = 0, nOsc = 1;
    float note = 60.f, vel = 1.f, x = 0.f, t = 0.f, lfo = 0.f, pitchHuman = 0.f, durScale = 1.f, pan = 0.5f;
    float envS = 1.f, lp = 0.f;
    uint32_t age = 0;
    int coefCount = 0;
    double ph[3] = { 0, 0, 0 };
    double pos = 0.0;
    float ic1[3] = { 0, 0, 0 }, ic2[3] = { 0, 0, 0 };
    float a1[3] = { 0, 0, 0 }, a2[3] = { 0, 0, 0 }, a3[3] = { 0, 0, 0 }, kk[3] = { 1, 1, 1 };
};

class GatofonoProcessor : public juce::AudioProcessor
{
public:
    GatofonoProcessor();
    ~GatofonoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Gatofono"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bool loadSample (const juce::File& f);
    juce::String getSampleName();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    Params readParams() const;
    void handleMidi (const juce::MidiMessage&, const Params&);
    void noteOn (int key, float vel, const Params&);
    void noteOff (int key);
    void arpAdd (int key, float vel);
    void arpRemove (int key);
    void arpClear();
    void arpTick (const Params&);
    void arpStep (const Params&);
    void renderVoice (Voice&, const Params&, const SampleData*, float& l, float& r);
    uint32_t nextRand();
    float rand01() { return (float) (nextRand() >> 8) * (1.f / 16777216.f); }

    std::array<Voice, 8> voices;
    uint32_t ageCounter = 0, rng = 22222u;
    double sr = 44100.0;
    float bend = 0.f, modWheel = 0.f, toneA = 0.5f;

    std::array<int, 16> arpKeys {};
    std::array<float, 16> arpVels {};
    int arpCount = 0, arpIdx = -1, arpDir = 1, arpLeft = 0, arpGateLeft = 0, arpCurKey = -1, arpStepLen = 11025;
    bool lastArp = false;

    juce::Reverb reverb;
    juce::SpinLock sampleLock;
    std::shared_ptr<const SampleData> sample;
    juce::String samplePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GatofonoProcessor)
};
