#include "PluginEditor.h"

namespace
{
const juce::Colour kBg (0xff1b1838), kPanel (0xff272350), kLine (0xff3d3980), kInk (0xfff1efff),
                   kPink (0xffff5c8a), kYellow (0xffffc93c);
}

CatLook::CatLook()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, kBg);
    setColour (Slider::rotarySliderFillColourId, kPink);
    setColour (Slider::rotarySliderOutlineColourId, kLine);
    setColour (Slider::thumbColourId, kYellow);
    setColour (Slider::textBoxTextColourId, kInk);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Label::textColourId, kInk);
    setColour (ComboBox::backgroundColourId, kPanel);
    setColour (ComboBox::textColourId, kInk);
    setColour (ComboBox::outlineColourId, kLine);
    setColour (ComboBox::arrowColourId, kYellow);
    setColour (PopupMenu::backgroundColourId, kPanel);
    setColour (PopupMenu::textColourId, kInk);
    setColour (PopupMenu::highlightedBackgroundColourId, kPink);
    setColour (TextButton::buttonColourId, kPink);
    setColour (TextButton::textColourOffId, Colours::white);
    setColour (ToggleButton::textColourId, kInk);
    setColour (ToggleButton::tickColourId, kYellow);
    setColour (ToggleButton::tickDisabledColourId, kLine);
    setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffe9e6ff));
    setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff1e1b4b));
    setColour (MidiKeyboardComponent::keySeparatorLineColourId, kLine);
    setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, kYellow.withAlpha (0.35f));
    setColour (MidiKeyboardComponent::keyDownOverlayColourId, kPink.withAlpha (0.8f));
    setColour (MidiKeyboardComponent::textLabelColourId, kBg);
}

GatofonoEditor::GatofonoEditor (GatofonoProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&look);

    addCombo ("mode", "Fuente");
    addCombo ("vowel", "Maullido");
    addToggle ("oneshot", "Gesto completo");
    addCombo ("scale", "Escala");
    addCombo ("root", "Tónica");
    addToggle ("arp", "Arpegiador");
    addCombo ("arprate", "Ritmo");
    addCombo ("arppat", "Patrón");

    const char* ids[]   = { "dur", "glide", "vib", "vibrate", "rough", "tone", "formant", "human", "cats",
                            "detune", "width", "octave", "srcroot", "arpgate", "revmix", "revsize", "gain" };
    const char* names[] = { "Duración", "Glissando", "Vibrato", "Vel. vibrato", "Aspereza", "Tono", "Formante", "Humanizar", "Gatos",
                            "Desafine", "Ancho", "Octava", "Nota audio", "Gate arp.", "Reverb", "Sala", "Volumen" };
    for (int i = 0; i < 17; ++i) addKnob (ids[i], names[i]);

    addAndMakeVisible (loadBtn);
    loadBtn.onClick = [safe = juce::Component::SafePointer<GatofonoEditor> (this)]
    {
        if (safe == nullptr) return;
        safe->chooser = std::make_unique<juce::FileChooser> (u8 ("Elige un audio de maullido"), juce::File(),
                                                              "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.ogg");
        safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                    [safe] (const juce::FileChooser& fc)
                                    {
                                        if (safe == nullptr) return;
                                        const auto f = fc.getResult();
                                        if (f.existsAsFile() && safe->proc.loadSample (f))
                                            if (auto* prm = safe->proc.apvts.getParameter ("mode"))
                                                prm->setValueNotifyingHost (1.f);
                                    });
    };
    sampleLabel.setJustificationType (juce::Justification::centredLeft);
    sampleLabel.setFont (juce::Font (13.f));
    addAndMakeVisible (sampleLabel);

    keyboard.setKeyWidth (30.f);
    keyboard.setLowestVisibleKey (48);
    addAndMakeVisible (keyboard);

    setSize (920, 500);
    startTimerHz (3);
    timerCallback();
}

GatofonoEditor::~GatofonoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

GatofonoEditor::Knob& GatofonoEditor::addKnob (const char* id, const char* text)
{
    auto k = std::make_unique<Knob>();
    k->s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k->l.setText (u8 (text), juce::dontSendNotification);
    k->l.setJustificationType (juce::Justification::centred);
    k->l.setFont (juce::Font (13.f));
    addAndMakeVisible (k->s);
    addAndMakeVisible (k->l);
    k->a = std::make_unique<SA> (proc.apvts, id, k->s);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

GatofonoEditor::Combo& GatofonoEditor::addCombo (const char* id, const char* text)
{
    auto c = std::make_unique<Combo>();
    if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->c.addItemList (cp->choices, 1);
    c->l.setText (u8 (text), juce::dontSendNotification);
    c->l.setFont (juce::Font (12.f));
    addAndMakeVisible (c->c);
    addAndMakeVisible (c->l);
    c->a = std::make_unique<CA> (proc.apvts, id, c->c);
    combos.push_back (std::move (c));
    return *combos.back();
}

GatofonoEditor::Toggle& GatofonoEditor::addToggle (const char* id, const char* text)
{
    auto t = std::make_unique<Toggle>();
    t->b.setButtonText (u8 (text));
    addAndMakeVisible (t->b);
    t->a = std::make_unique<BA> (proc.apvts, id, t->b);
    toggles.push_back (std::move (t));
    return *toggles.back();
}

void GatofonoEditor::timerCallback()
{
    const auto n = proc.getSampleName();
    sampleLabel.setText (n.isEmpty() ? u8 ("(sin audio cargado)") : n, juce::dontSendNotification);
}

void GatofonoEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kInk);
    g.setFont (juce::Font (34.f, juce::Font::bold));
    g.drawText (u8 ("Gatófono"), 18, 12, 240, 44, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffa9a5dd));
    g.setFont (juce::Font (13.f));
    g.drawText (u8 ("Melodías con maullidos"), 20, 54, 240, 20, juce::Justification::centredLeft);
}

void GatofonoEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto top = r.removeFromTop (92);
    top.removeFromLeft (240);

    auto place = [] (Combo& c, juce::Rectangle<int> b)
    {
        c.l.setBounds (b.removeFromTop (16));
        c.c.setBounds (b.reduced (0, 1));
    };
    auto row1 = top.removeFromTop (46);
    auto row2 = top;
    const int cw = 130;
    place (*combos[0], row1.removeFromLeft (cw).reduced (3, 0));
    place (*combos[1], row1.removeFromLeft (cw).reduced (3, 0));
    toggles[0]->b.setBounds (row1.removeFromLeft (150).withTrimmedTop (16));
    loadBtn.setBounds (row1.removeFromLeft (150).withTrimmedTop (16).reduced (3, 0));
    sampleLabel.setBounds (row1.withTrimmedTop (16));
    place (*combos[2], row2.removeFromLeft (cw + 20).reduced (3, 0));
    place (*combos[3], row2.removeFromLeft (cw - 40).reduced (3, 0));
    toggles[1]->b.setBounds (row2.removeFromLeft (120).withTrimmedTop (16));
    place (*combos[4], row2.removeFromLeft (cw - 30).reduced (3, 0));
    place (*combos[5], row2.removeFromLeft (cw + 10).reduced (3, 0));

    r.removeFromTop (8);
    auto kb = r.removeFromBottom (96);
    keyboard.setBounds (kb);
    r.removeFromBottom (8);

    const int cols = 9;
    const int cellW = r.getWidth() / cols;
    const int cellH = r.getHeight() / 2;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        juce::Rectangle<int> cell (r.getX() + (int) (i % cols) * cellW, r.getY() + (int) (i / cols) * cellH, cellW, cellH);
        knobs[i]->l.setBounds (cell.removeFromTop (16));
        knobs[i]->s.setBounds (cell.reduced (4, 0));
    }
}
