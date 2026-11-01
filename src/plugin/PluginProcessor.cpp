#include "PluginProcessor.h"
#include "../editor/PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout FetlinePounceProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const int v = 1; // parameter version

    // Ranges follow the faceplate knobs. Input: knob 0..10 = 0..30 dB. Output: knob 5 = 0 dB, 4 dB per step.
    // Attack and Release are exponential and run backwards (clockwise is faster): the normalised
    // value is the knob position divided by 10.
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::input, v }, "Input",
        NormalisableRange<float> (0.0f, fp::kInputMaxDb, 0.01f), 4.5f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, v }, "Output",
        NormalisableRange<float> (fp::kOutputMinDb, fp::kOutputMaxDb, 0.01f), 1.2f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " dB"; })));
    const auto timeRange = [] (float lo, float hi, bool descending)
    {
        const float ratio = hi / lo;
        if (descending)   // normalised 0 is the slowest (hi), 1 the fastest (lo)
            return NormalisableRange<float> (lo, hi,
                [hi, ratio] (float, float, float n) { return hi * std::pow (1.0f / ratio, n); },
                [hi, ratio] (float, float, float x) { return std::log (x / hi) / std::log (1.0f / ratio); },
                [lo, hi] (float, float, float x) { return jlimit (lo, hi, x); });
        return NormalisableRange<float> (lo, hi);
    };
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::attack, v }, "Attack",
        timeRange (fp::kAttackMinUs, fp::kAttackMaxUs, true), fp::attackUsFromKnob (6.0f),
        AudioParameterFloatAttributes().withLabel ("us").withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + " us"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::release, v }, "Release",
        timeRange (fp::kReleaseMinMs, fp::kReleaseMaxMs, true), fp::releaseMsFromKnob (4.0f),
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + " ms"; })));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::ratio, v }, "Ratio", StringArray { "4:1", "8:1", "12:1", "20:1" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::all, v }, "All Buttons", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, v }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + " %"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::hp, v }, "Listen HP", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::meter, v }, "Meter", StringArray { "GR", "+8", "+4", "Off" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, v }, "Bypass", false));
    return { p.begin(), p.end() };
}

FetlinePounceProcessor::FetlinePounceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FETLINEPOUNCE", createLayout())
{
    pInput   = apvts.getRawParameterValue (ids::input);
    pOutput  = apvts.getRawParameterValue (ids::output);
    pAttack  = apvts.getRawParameterValue (ids::attack);
    pRelease = apvts.getRawParameterValue (ids::release);
    pRatio   = apvts.getRawParameterValue (ids::ratio);
    pAll     = apvts.getRawParameterValue (ids::all);
    pMix     = apvts.getRawParameterValue (ids::mix);
    pHp      = apvts.getRawParameterValue (ids::hp);
    pMeter   = apvts.getRawParameterValue (ids::meter);
    pBypass  = apvts.getRawParameterValue (ids::bypass);
}

fp::Params FetlinePounceProcessor::readParams() const
{
    fp::Params q;
    q.inputDb    = pInput->load();
    q.outputDb   = pOutput->load();
    q.attackUs   = pAttack->load();
    q.releaseMs  = pRelease->load();
    q.ratioIndex = jlimit (0, 3, (int) pRatio->load());
    q.all        = pAll->load() > 0.5f;
    q.mix        = pMix->load() * 0.01f;
    q.hp         = pHp->load() > 0.5f;
    q.bypass     = pBypass->load() > 0.5f;
    return q;
}

bool FetlinePounceProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}

var FetlinePounceProcessor::getSetting (const char* key, const var& fallback) const
{
    return apvts.state.getProperty (key, fallback);
}

void FetlinePounceProcessor::setSetting (const char* key, const var& v)
{
    apvts.state.setProperty (key, v, nullptr);
}

void FetlinePounceProcessor::prepareToPlay (double sampleRate, int)
{
    core.setParams (readParams());
    core.prepare (sampleRate);
    setLatencySamples (core.latencySamples());   // 30 samples below 80 kHz (2x oversampling), 0 at 88.2 kHz and above
}

void FetlinePounceProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numCh = jmin (2, buffer.getNumChannels());
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    if (numCh == 0) return;
    const int n = buffer.getNumSamples();

    core.setParams (readParams());
    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    float inPk = 0.0f, outPk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float a = std::abs (l[i]), b = r != nullptr ? std::abs (r[i]) : 0.0f;
        if (a == a) inPk = jmax (inPk, a);          // NaN never raises the meter
        if (b == b) inPk = jmax (inPk, b);
    }
    core.clearGrPeak();
    core.process (l, r, n);
    for (int i = 0; i < n; ++i)
    {
        outPk = jmax (outPk, std::abs (l[i]));
        if (r != nullptr) outPk = jmax (outPk, std::abs (r[i]));
    }
    grMeterDb.store (core.grDb());
    raiseTo (grPeakDb, core.grPeakDb());
    raiseTo (inPeak, std::isfinite (inPk) ? inPk : 0.0f);
    raiseTo (outPeak, outPk);
    outRmsDb.store (core.outRmsDb());
}

AudioProcessorEditor* FetlinePounceProcessor::createEditor()
{
    return new FetlinePounceEditor (*this);
}

void FetlinePounceProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void FetlinePounceProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml)); // unknown keys are ignored by APVTS
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FetlinePounceProcessor(); }
