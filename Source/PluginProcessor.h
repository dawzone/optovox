/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
// Parameter IDs live in one place so the UI + DSP never drift.
namespace OptoVoxParams
{
    static constexpr const char* peak      = "peak";      // 0..100
    static constexpr const char* gain      = "gain";      // 0..40 dB
    static constexpr const char* hf        = "hf";        // 0..100
    static constexpr const char* bias      = "bias";      // 0..100
    static constexpr const char* mode      = "mode";      // COMP / LIMIT
    static constexpr const char* ratio     = "ratio";     // 2:1 / 4:1 / inf
    static constexpr const char* attack    = "attack";    // AUTO / FAST / SLOW
    static constexpr const char* release   = "release";   // AUTO / FAST / SLOW
    static constexpr const char* sidechain = "sidechain"; // INT / EXT / HF
    static constexpr const char* outmode   = "outmode";   // ST / MONO / MS
    static constexpr const char* bypass    = "bypass";    // bool
}

//==============================================================================
/**
*/
class OptoVoxAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    OptoVoxAudioProcessor();
    ~OptoVoxAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Lightweight meters for the UI.
    float getInputRmsDb() const noexcept  { return inRmsDb.load(); }
    float getOutputRmsDb() const noexcept { return outRmsDb.load(); }
    float getGainReductionDb() const noexcept { return grDb.load(); }

private:
    //==============================================================================
    // --- DSP state (intentionally simple v0.1 scaffolding) ---
    double currentSampleRate = 44100.0;

    // Sidechain pre-emphasis helper (simple one-pole lowpass used to derive HF content)
    struct OnePoleLP
    {
        float a = 0.0f;
        float z = 0.0f;
        void reset() noexcept { z = 0.0f; }
        void setCutoffHz (double sr, float hz) noexcept
        {
            const float x = std::exp (-2.0f * juce::MathConstants<float>::pi * hz / (float) sr);
            a = x;
        }
        float process (float v) noexcept
        {
            z = a * z + (1.0f - a) * v;
            return z;
        }
    } scLowpassL, scLowpassR;

    float env = 0.0f;
    float envAttackCoeff = 0.0f;
    float envReleaseCoeff = 0.0f;

    float grSmoothedDb = 0.0f;
    float grAttackCoeff = 0.0f;
    float grReleaseCoeff = 0.0f;

    std::atomic<float> inRmsDb { -100.0f };
    std::atomic<float> outRmsDb { -100.0f };
    std::atomic<float> grDb { 0.0f };

    void updateTimeConstants();
    static float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
    static float gainToDb (float g) noexcept
    {
        const float eps = 1.0e-9f;
        return 20.0f * std::log10 (juce::jmax (g, eps));
    }
    static float satTube (float x, float bias01) noexcept;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoVoxAudioProcessor)
};
