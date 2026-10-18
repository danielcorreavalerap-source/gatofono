#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
constexpr float kVow[4][3][3] = {
    { { 300, 2300, 3000 }, { 800, 1300, 2500 }, { 450, 800, 2600 } },   // Miau
    { { 280, 2200, 3000 }, { 850, 1250, 2500 }, { 350, 700, 2500 } },   // Nau
    { { 310, 2300, 3000 }, { 350, 2200, 3000 }, { 300, 2100, 2900 } },  // Miiii
    { { 600, 1500, 2600 }, { 850, 1300, 2500 }, { 750, 1200, 2500 } }   // Nyaa
};
constexpr float kQ[3] = { 6.f, 10.f, 10.f };
constexpr float kFGain[3] = { 1.f, 0.7f, 0.4f };

constexpr int mk (std::initializer_list<int> l) { int r = 0; for (int x : l) r |= 1 << x; return r; }
constexpr int kMask[7] = { 0xFFF, mk ({ 0, 2, 4, 5, 7, 9, 11 }), mk ({ 0, 2, 3, 5, 7, 8, 10 }), mk ({ 0, 3, 5, 7, 10 }),
                           mk ({ 0, 2, 4, 7, 9 }), mk ({ 0, 2, 3, 5, 7, 9, 10 }), mk ({ 0, 3, 5, 6, 7, 10 }) };

int quantizeNote (int note, int scale, int root)
{
    if (scale <= 0 || scale > 6) return note;
    const int mask = kMask[scale];
    for (int d = 0; d <= 6; ++d)
        for (int s : { -d, d })
        {
            const int rel = (((note + s - root) % 12) + 12) % 12;
            if (mask & (1 << rel)) return note + s;
        }
    return note;
}

inline float polyBlep (float t, float dt)
{
    if (t < dt)        { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt)  { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
}

GatofonoProcessor::GatofonoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GATOFONO", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout GatofonoProcessor::createLayout()
{
    using APF = juce::AudioParameterFloat;
    using APC = juce::AudioParameterChoice;
    using API = juce::AudioParameterInt;
    using APB = juce::AudioParameterBool;
    using PID = juce::ParameterID;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto F = [&] (const char* id, const char* name, float lo, float hi, float def, float skew = 1.f)
    {
        p.push_back (std::make_unique<APF> (PID { id, 1 }, u8 (name), juce::NormalisableRange<float> (lo, hi, 0.001f, skew), def));
    };
    auto C = [&] (const char* id, const char* name, std::initializer_list<const char*> items, int def)
    {
        juce::StringArray a;
        for (auto* it : items) a.add (u8 (it));
        p.push_back (std::make_unique<APC> (PID { id, 1 }, u8 (name), a, def));
    };
    auto I = [&] (const char* id, const char* name, int lo, int hi, int def)
    {
        p.push_back (std::make_unique<API> (PID { id, 1 }, u8 (name), lo, hi, def));
    };
    auto B = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<APB> (PID { id, 1 }, u8 (name), def));
    };

    C ("mode", "Fuente", { "Sintético", "Audio propio" }, 0);
    C ("vowel", "Maullido", { "Miau", "Ñau", "Miiii", "Nyaa" }, 0);
    B ("oneshot", "Gesto completo", false);
    F ("dur", "Duración", 0.1f, 3.f, 0.6f, 0.5f);
    F ("glide", "Glissando", 0.f, 1.f, 0.6f);
    F ("vib", "Vibrato", 0.f, 1.f, 0.4f);
    F ("vibrate", "Vel. vibrato", 3.f, 9.f, 6.f);
    F ("rough", "Aspereza", 0.f, 1.f, 0.3f);
    F ("tone", "Tono", 800.f, 12000.f, 8000.f, 0.4f);
    F ("formant", "Formante", 0.7f, 1.4f, 1.f);
    F ("human", "Humanizar", 0.f, 1.f, 0.2f);
    I ("cats", "Gatos", 1, 3, 1);
    F ("detune", "Desafine", 0.f, 50.f, 12.f);
    F ("width", "Ancho", 0.f, 1.f, 0.3f);
    I ("octave", "Octava", -2, 2, 1);
    C ("scale", "Escala", { "Libre", "Mayor", "Menor", "Pentatónica menor", "Pentatónica mayor", "Dórica", "Blues" }, 0);
    C ("root", "Tónica", { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" }, 0);
    I ("srcroot", "Nota del audio", 24, 96, 60);
    B ("arp", "Arpegiador", false);
    C ("arprate", "Ritmo", { "1/4", "1/8", "1/16", "1/8T" }, 1);
    C ("arppat", "Patrón", { "Arriba", "Abajo", "Ida y vuelta", "Aleatorio" }, 0);
    F ("arpgate", "Gate", 0.1f, 1.f, 0.6f);
    F ("revmix", "Reverb", 0.f, 1.f, 0.2f);
    F ("revsize", "Sala", 0.f, 1.f, 0.5f);
    F ("gain", "Volumen (dB)", -24.f, 6.f, 0.f);
    return { p.begin(), p.end() };
}

Params GatofonoProcessor::readParams() const
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    Params P;
    P.mode = (int) g ("mode");       P.vowel = (int) g ("vowel");   P.cats = (int) g ("cats");
    P.octave = (int) g ("octave");   P.scale = (int) g ("scale");   P.root = (int) g ("root");
    P.srcRoot = (int) g ("srcroot"); P.arpRate = (int) g ("arprate"); P.arpPat = (int) g ("arppat");
    P.formant = g ("formant"); P.dur = g ("dur");     P.glide = g ("glide");  P.vib = g ("vib");
    P.vibRate = g ("vibrate"); P.rough = g ("rough"); P.tone = g ("tone");    P.detune = g ("detune");
    P.human = g ("human");     P.width = g ("width"); P.arpGate = g ("arpgate");
    P.revMix = g ("revmix");   P.revSize = g ("revsize"); P.gain = g ("gain");
    P.oneShot = g ("oneshot") > 0.5f; P.arp = g ("arp") > 0.5f;
    return P;
}

uint32_t GatofonoProcessor::nextRand()
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

void GatofonoProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    for (auto& v : voices) v = Voice();
    arpClear();
}

void GatofonoProcessor::noteOn (int key, float vel, const Params& P)
{
    Voice* slot = nullptr;
    for (auto& v : voices) if (! v.active) { slot = &v; break; }
    if (slot == nullptr)
    {
        slot = &voices[0];
        for (auto& v : voices) if (v.age < slot->age) slot = &v;
    }
    Voice v;
    v.active = true; v.held = true; v.key = key;
    v.note = (float) (quantizeNote (key, P.scale, P.root) + 12 * P.octave);
    v.vel = juce::jlimit (0.2f, 1.f, vel);
    v.age = ++ageCounter;
    v.nOsc = juce::jlimit (1, 3, P.cats);
    v.sampleMode = P.mode == 1;
    v.pitchHuman = (rand01() - 0.5f) * 2.f * P.human * 0.4f;
    v.durScale = 1.f + (rand01() - 0.5f) * 2.f * P.human * 0.3f;
    v.pan = 0.5f + (rand01() - 0.5f) * P.width;
    if (v.nOsc > 1) for (int i = 0; i < 3; ++i) v.ph[i] = rand01();
    *slot = v;
}

void GatofonoProcessor::noteOff (int key)
{
    for (auto& v : voices) if (v.active && v.held && v.key == key) v.held = false;
}

void GatofonoProcessor::arpAdd (int key, float vel)
{
    if (arpCount >= 16) return;
    for (int i = 0; i < arpCount; ++i) if (arpKeys[(size_t) i] == key) return;
    int pos = arpCount;
    while (pos > 0 && arpKeys[(size_t) pos - 1] > key)
    {
        arpKeys[(size_t) pos] = arpKeys[(size_t) pos - 1];
        arpVels[(size_t) pos] = arpVels[(size_t) pos - 1];
        --pos;
    }
    arpKeys[(size_t) pos] = key; arpVels[(size_t) pos] = vel;
    ++arpCount;
    if (arpCount == 1) { arpIdx = -1; arpDir = 1; arpLeft = 0; }
}

void GatofonoProcessor::arpRemove (int key)
{
    int j = -1;
    for (int i = 0; i < arpCount; ++i) if (arpKeys[(size_t) i] == key) j = i;
    if (j < 0) return;
    for (int i = j; i < arpCount - 1; ++i)
    {
        arpKeys[(size_t) i] = arpKeys[(size_t) i + 1];
        arpVels[(size_t) i] = arpVels[(size_t) i + 1];
    }
    --arpCount;
    if (arpCount == 0) { if (arpCurKey >= 0) { noteOff (arpCurKey); arpCurKey = -1; } }
    else if (arpIdx >= arpCount) arpIdx = arpCount - 1;
}

void GatofonoProcessor::arpClear()
{
    arpCount = 0;
    if (arpCurKey >= 0) { noteOff (arpCurKey); arpCurKey = -1; }
}

void GatofonoProcessor::arpStep (const Params& P)
{
    if (arpCurKey >= 0) { noteOff (arpCurKey); arpCurKey = -1; }
    if (arpCount == 0) return;
    switch (P.arpPat)
    {
        case 0: arpIdx = (arpIdx + 1) % arpCount; break;
        case 1: arpIdx = (arpIdx <= 0 ? arpCount : arpIdx) - 1; break;
        case 2:
            if (arpCount == 1) { arpIdx = 0; break; }
            arpIdx += arpDir;
            if (arpIdx >= arpCount) { arpIdx = arpCount - 2; arpDir = -1; }
            else if (arpIdx < 0)    { arpIdx = 1; arpDir = 1; }
            break;
        default: arpIdx = (int) (nextRand() % (uint32_t) arpCount); break;
    }
    arpIdx = juce::jlimit (0, arpCount - 1, arpIdx);
    arpCurKey = arpKeys[(size_t) arpIdx];
    noteOn (arpCurKey, arpVels[(size_t) arpIdx], P);
    arpGateLeft = juce::jmax (1, (int) ((float) arpStepLen * P.arpGate));
}

void GatofonoProcessor::arpTick (const Params& P)
{
    if (arpGateLeft > 0 && --arpGateLeft == 0 && arpCurKey >= 0) { noteOff (arpCurKey); arpCurKey = -1; }
    if (arpCount == 0) return;
    if (--arpLeft <= 0)
    {
        arpLeft += arpStepLen;
        if (arpLeft <= 0) arpLeft = arpStepLen;
        arpStep (P);
    }
}

void GatofonoProcessor::handleMidi (const juce::MidiMessage& m, const Params& P)
{
    if (m.isNoteOn())
    {
        if (P.arp) arpAdd (m.getNoteNumber(), m.getFloatVelocity());
        else       noteOn (m.getNoteNumber(), m.getFloatVelocity(), P);
    }
    else if (m.isNoteOff())
    {
        if (P.arp) arpRemove (m.getNoteNumber());
        else       noteOff (m.getNoteNumber());
    }
    else if (m.isPitchWheel())
        bend = (float) (m.getPitchWheelValue() - 8192) / 8192.f * 2.f;
    else if (m.isController() && m.getControllerNumber() == 1)
        modWheel = (float) m.getControllerValue() / 127.f;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        arpClear();
        for (auto& v : voices) v.held = false;
    }
}

void GatofonoProcessor::renderVoice (Voice& v, const Params& P, const SampleData* smp, float& outL, float& outR)
{
    const float dt = 1.f / (float) sr;
    float s = 0.f;

    if (v.sampleMode)
    {
        if (smp == nullptr || smp->buf.getNumSamples() < 2) { v.active = false; return; }
        const int n = smp->buf.getNumSamples();
        const int i0 = (int) v.pos;
        if (i0 >= n - 1) { v.active = false; return; }
        const float fr = (float) (v.pos - i0);
        const float* d = smp->buf.getReadPointer (0);
        s = d[i0] * (1.f - fr) + d[i0 + 1] * fr;
        const float semis = v.note - (float) P.srcRoot + bend + v.pitchHuman;
        v.pos += std::exp2 (semis / 12.f) * smp->sr / sr;
        if (! v.held && ! P.oneShot)
        {
            v.envS -= dt / 0.08f;
            if (v.envS <= 0.f) { v.active = false; return; }
        }
        s *= juce::jmin (1.f, v.t / 0.003f) * v.envS * v.vel * 0.9f;
        v.t += dt;
    }
    else
    {
        const float durEff = juce::jmax (0.05f, P.dur * v.durScale);
        if (v.held && ! P.oneShot) { if (v.x < 0.4f) v.x = juce::jmin (0.4f, v.x + dt / durEff); }
        else v.x += dt / durEff;
        if (v.x >= 1.f) { v.active = false; return; }
        const float x = v.x;

        const float g = P.glide;
        const float c = x < 0.35f ? lerpf (1.f - 0.25f * g, 1.f + 0.08f * g, x / 0.35f)
                                  : lerpf (1.f + 0.08f * g, 1.f - 0.3f * g, (x - 0.35f) / 0.65f);

        v.lfo += juce::MathConstants<float>::twoPi * P.vibRate * dt;
        if (v.lfo > juce::MathConstants<float>::twoPi) v.lfo -= juce::MathConstants<float>::twoPi;
        const float vibAmt = juce::jmax (P.vib, modWheel);
        const float vibSemi = vibAmt * 0.6f * std::sin (v.lfo) * juce::jmin (1.f, v.t / 0.2f);

        const float f0 = 440.f * std::exp2 ((v.note - 69.f + v.pitchHuman + bend + vibSemi) / 12.f) * c;

        float src = 0.f;
        for (int i = 0; i < v.nOsc; ++i)
        {
            const float det = ((float) i - (float) (v.nOsc - 1) * 0.5f) * P.detune;
            const float inc = juce::jlimit (1e-5f, 0.45f, f0 * std::exp2 (det / 1200.f) * dt);
            v.ph[i] += inc;
            if (v.ph[i] >= 1.0) v.ph[i] -= 1.0;
            const float p = (float) v.ph[i];
            src += 2.f * p - 1.f - polyBlep (p, inc);
        }
        src /= std::sqrt ((float) v.nOsc);
        src += P.rough * 0.6f * (rand01() * 2.f - 1.f);

        const int vw = juce::jlimit (0, 3, P.vowel);
        const bool upd = (v.coefCount++ & 7) == 0;
        float sum = 0.f;
        for (int i = 0; i < 3; ++i)
        {
            if (upd)
            {
                float fr = x < 0.4f ? lerpf (kVow[vw][0][i], kVow[vw][1][i], x / 0.4f)
                                    : lerpf (kVow[vw][1][i], kVow[vw][2][i], (x - 0.4f) / 0.6f);
                fr = juce::jlimit (60.f, (float) sr * 0.45f, fr * P.formant);
                const float gg = std::tan (juce::MathConstants<float>::pi * fr / (float) sr);
                const float k = 1.f / kQ[i];
                v.a1[i] = 1.f / (1.f + gg * (gg + k));
                v.a2[i] = gg * v.a1[i];
                v.a3[i] = gg * v.a2[i];
                v.kk[i] = k;
            }
            const float v3 = src - v.ic2[i];
            const float v1 = v.a1[i] * v.ic1[i] + v.a2[i] * v3;
            const float v2 = v.ic2[i] + v.a2[i] * v.ic1[i] + v.a3[i] * v3;
            v.ic1[i] = 2.f * v1 - v.ic1[i];
            v.ic2[i] = 2.f * v2 - v.ic2[i];
            sum += v.kk[i] * v1 * kFGain[i];
        }
        v.lp += toneA * (sum - v.lp);
        const float tail = x > 0.7f ? (1.f - x) / 0.3f : 1.f;
        s = v.lp * juce::jmin (1.f, v.t / 0.04f) * tail * v.vel * 0.9f;
        v.t += dt;
    }

    const float a = v.pan * juce::MathConstants<float>::halfPi;
    outL += s * std::cos (a) * 1.4f;
    outR += s * std::sin (a) * 1.4f;
}

void GatofonoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    const Params P = readParams();

    std::shared_ptr<const SampleData> smp;
    {
        juce::SpinLock::ScopedTryLockType l (sampleLock);
        if (l.isLocked()) smp = sample;
    }

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    const double beats[] = { 1.0, 0.5, 0.25, 1.0 / 3.0 };
    arpStepLen = juce::jmax (1, (int) (sr * 60.0 / juce::jmax (20.0, bpm) * beats[juce::jlimit (0, 3, P.arpRate)]));
    toneA = 1.f - std::exp (-juce::MathConstants<float>::twoPi * P.tone / (float) sr);

    if (! P.arp && lastArp) arpClear();
    lastArp = P.arp;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    auto it = midi.begin();
    for (int i = 0; i < n; ++i)
    {
        while (it != midi.end() && (*it).samplePosition <= i)
        {
            handleMidi ((*it).getMessage(), P);
            ++it;
        }
        if (P.arp) arpTick (P);
        float l = 0.f, r = 0.f;
        for (auto& v : voices) if (v.active) renderVoice (v, P, smp.get(), l, r);
        L[i] = l; R[i] = r;
    }

    juce::Reverb::Parameters rp;
    rp.roomSize = P.revSize; rp.damping = 0.5f; rp.wetLevel = P.revMix * 0.5f;
    rp.dryLevel = 1.f - P.revMix * 0.3f; rp.width = 1.f; rp.freezeMode = 0.f;
    reverb.setParameters (rp);
    reverb.processStereo (L, R, n);
    buffer.applyGain (juce::Decibels::decibelsToGain (P.gain));
}

bool GatofonoProcessor::loadSample (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples < 2) return false;

    const int n = (int) juce::jmin<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 10.0));
    juce::AudioBuffer<float> tmp ((int) r->numChannels, n);
    r->read (&tmp, 0, n, 0, true, true);

    auto s = std::make_shared<SampleData>();
    s->buf.setSize (1, n);
    s->buf.clear();
    for (int c = 0; c < tmp.getNumChannels(); ++c)
        s->buf.addFrom (0, 0, tmp, c, 0, n, 1.f / (float) tmp.getNumChannels());
    const float mx = s->buf.getMagnitude (0, n);
    if (mx > 0.f) s->buf.applyGain (0.9f / mx);
    s->sr = r->sampleRate;
    s->name = f.getFileName();
    {
        juce::SpinLock::ScopedLockType l (sampleLock);
        sample = s;
        samplePath = f.getFullPathName();
    }
    return true;
}

juce::String GatofonoProcessor::getSampleName()
{
    juce::SpinLock::ScopedLockType l (sampleLock);
    return sample ? sample->name : juce::String();
}

void GatofonoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    {
        juce::SpinLock::ScopedLockType l (sampleLock);
        state.setProperty ("samplePath", samplePath, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void GatofonoProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (vt);
            const auto path = vt.getProperty ("samplePath").toString();
            if (path.isNotEmpty() && juce::File (path).existsAsFile()) loadSample (juce::File (path));
        }
}

juce::AudioProcessorEditor* GatofonoProcessor::createEditor() { return new GatofonoEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GatofonoProcessor(); }
