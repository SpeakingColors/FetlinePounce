#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../core/FetlinePounce.h"
#include <atomic>

// Stable parameter IDs. Never rename these (saved projects use them).
namespace ids
{
    inline constexpr const char* input = "input";
    inline constexpr const char* output = "output";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* ratio = "ratio";
    inline constexpr const char* all = "all";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* hp = "hp";
    inline constexpr const char* meter = "meter";
    inline constexpr const char* bypass = "bypass";
}

// Plugin-wide settings kept in the saved state (not presets, not automatable).
namespace setting
{
    inline constexpr const char* look = "look";       // "A", "B", "C"
    inline constexpr const char* tips = "tips";       // info bubbles
    inline constexpr const char* power = "power";     // power saving redraw
    inline constexpr const char* scale = "scale";     // window scale, 1.0 = 100 %
    inline constexpr const char* preset = "preset";   // name of the loaded preset
}

class FetlinePounceProcessor : public juce::AudioProcessor
{
public:
    FetlinePounceProcessor();
    ~FetlinePounceProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FetlinePounce"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Meter data for the editor (written on the audio thread, read by the editor timer).
    std::atomic<float> grMeterDb { 0.0f };   // gain reduction, positive dB (keeps moving in bypass, so the needle still shows what the limiter would do)
    std::atomic<float> grPeakDb { 0.0f };    // largest gain reduction since the editor last read it (exchange to 0)
    std::atomic<float> inPeak { 0.0f };      // linear peak since the editor last read it (exchange to 0), input
    std::atomic<float> outPeak { 0.0f };     // same for the output
    std::atomic<float> outRmsDb { -120.0f }; // output level (300 ms) for the +8 and +4 meter modes

    // Settings stored in the state tree.
    juce::var getSetting (const char* key, const juce::var& fallback) const;
    void setSetting (const char* key, const juce::var& v);
    static void raiseTo (std::atomic<float>& a, float v) { float c = a.load(); while (v > c && ! a.compare_exchange_weak (c, v)) {} }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    fp::Params readParams() const;

    fp::Pounce core;
    std::atomic<float>* pInput; std::atomic<float>* pOutput; std::atomic<float>* pAttack; std::atomic<float>* pRelease;
    std::atomic<float>* pRatio; std::atomic<float>* pAll; std::atomic<float>* pMix; std::atomic<float>* pHp;
    std::atomic<float>* pMeter; std::atomic<float>* pBypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FetlinePounceProcessor)
};
