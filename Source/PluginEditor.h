#pragma once
#include "PluginProcessor.h"

class CatLook : public juce::LookAndFeel_V4
{
public:
    CatLook();
};

class GatofonoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GatofonoEditor (GatofonoProcessor&);
    ~GatofonoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob  { juce::Slider s; juce::Label l; std::unique_ptr<SA> a; };
    struct Combo { juce::ComboBox c; juce::Label l; std::unique_ptr<CA> a; };
    struct Toggle { juce::ToggleButton b; std::unique_ptr<BA> a; };

    void timerCallback() override;
    Knob& addKnob (const char* id, const char* text);
    Combo& addCombo (const char* id, const char* text);
    Toggle& addToggle (const char* id, const char* text);

    GatofonoProcessor& proc;
    CatLook look;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Combo>> combos;
    std::vector<std::unique_ptr<Toggle>> toggles;
    juce::TextButton loadBtn { juce::String::fromUTF8 ("Cargar maullido…") };
    juce::Label sampleLabel;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GatofonoEditor)
};
