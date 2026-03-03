/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout OptoVoxAudioProcessor::createParameterLayout()
{
    using APVTS = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        OptoVoxParams::peak,
        "Peak Reduction",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f),
        40.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        OptoVoxParams::gain,
        "Output Gain",
        juce::NormalisableRange<float> (0.0f, 40.0f, 0.01f),
        20.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        OptoVoxParams::hf,
        "HF EQ",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f),
        50.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        OptoVoxParams::bias,
        "Tube Bias",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f),
        35.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        OptoVoxParams::mode,
        "Mode",
        juce::StringArray { "COMP", "LIMIT" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        OptoVoxParams::ratio,
        "Ratio",
        juce::StringArray { "2:1", "4:1", "INF" },
        1));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        OptoVoxParams::attack,
        "Attack",
        juce::StringArray { "AUTO", "FAST", "SLOW" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        OptoVoxParams::release,
        "Release",
        juce::StringArray { "AUTO", "FAST", "SLOW" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        OptoVoxParams::sidechain,
        "Sidechain",
        juce::StringArray { "INT", "EXT", "HF" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        OptoVoxParams::outmode,
        "Output Mode",
        juce::StringArray { "ST", "MONO", "MS" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        OptoVoxParams::bypass,
        "Bypass",
        false));

    return { params.begin(), params.end() };
}

void OptoVoxAudioProcessor::updateTimeConstants()
{
    const auto atkChoice = apvts.getRawParameterValue (OptoVoxParams::attack)->load();
    const auto relChoice = apvts.getRawParameterValue (OptoVoxParams::release)->load();

    // Simple scaffolding values. We'll refine after the UI + meters feel right.
    float atkMs = 12.0f;
    float relMs = 250.0f;

    if ((int) atkChoice == 1) atkMs = 5.0f;
    if ((int) atkChoice == 2) atkMs = 25.0f;

    if ((int) relChoice == 1) relMs = 120.0f;
    if ((int) relChoice == 2) relMs = 450.0f;

    // Program-dependent feel: in AUTO we let release be a bit longer and smoother.
    if ((int) relChoice == 0) relMs = 320.0f;

    const auto sr = juce::jmax (1.0, currentSampleRate);
    envAttackCoeff  = std::exp (-1.0f / (float) (0.001 * atkMs * sr));
    envReleaseCoeff = std::exp (-1.0f / (float) (0.001 * relMs * sr));

    // GR smoothing (separate from env to avoid twitchy visuals)
    grAttackCoeff   = std::exp (-1.0f / (float) (0.001 * (atkMs * 1.5f) * sr));
    grReleaseCoeff  = std::exp (-1.0f / (float) (0.001 * (relMs * 1.2f) * sr));

    scLowpassL.setCutoffHz (sr, 900.0f);
    scLowpassR.setCutoffHz (sr, 900.0f);
}

float OptoVoxAudioProcessor::satTube (float x, float bias01) noexcept
{
    // bias01: 0..1. At 0 => essentially clean, at 1 => audible warmth.
    const float drive = 1.0f + 5.0f * bias01;            // 1..6
    const float asym  = (bias01 - 0.5f) * 0.18f;         // mild asymmetry
    const float pre   = drive * (x + asym);
    const float y0    = std::tanh (pre);
    const float yRef  = std::tanh (drive * asym);
    const float y     = (y0 - yRef);

    // Slight compensation so perceived loudness doesn't explode when driving.
    const float comp = 1.0f / (0.75f + 0.25f * drive);
    return y * comp;
}

//==============================================================================
OptoVoxAudioProcessor::OptoVoxAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
     , apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

OptoVoxAudioProcessor::~OptoVoxAudioProcessor()
{
}

//==============================================================================
const juce::String OptoVoxAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool OptoVoxAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool OptoVoxAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool OptoVoxAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double OptoVoxAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int OptoVoxAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int OptoVoxAudioProcessor::getCurrentProgram()
{
    return 0;
}

void OptoVoxAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String OptoVoxAudioProcessor::getProgramName (int index)
{
    return {};
}

void OptoVoxAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void OptoVoxAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate;

    env = 0.0f;
    grSmoothedDb = 0.0f;
    scLowpassL.reset();
    scLowpassR.reset();

    updateTimeConstants();
}

void OptoVoxAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool OptoVoxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void OptoVoxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const bool bypassed = apvts.getRawParameterValue (OptoVoxParams::bypass)->load() > 0.5f;
    const float peak    = apvts.getRawParameterValue (OptoVoxParams::peak)->load();
    const float gainDb  = apvts.getRawParameterValue (OptoVoxParams::gain)->load();
    const float hf      = apvts.getRawParameterValue (OptoVoxParams::hf)->load();
    const float bias    = apvts.getRawParameterValue (OptoVoxParams::bias)->load();

    const int modeChoice  = (int) apvts.getRawParameterValue (OptoVoxParams::mode)->load();
    const int ratioChoice = (int) apvts.getRawParameterValue (OptoVoxParams::ratio)->load();

    // Update time constants if attack/release changed (cheap)
    updateTimeConstants();

    const float peak01 = juce::jlimit (0.0f, 1.0f, peak / 100.0f);
    const float hf01   = juce::jlimit (0.0f, 1.0f, hf   / 100.0f);
    const float bias01 = juce::jlimit (0.0f, 1.0f, bias / 100.0f);

    // Max GR target derived from Peak Reduction knob (classic feel)
    const float maxGrDb = 20.0f * std::pow (peak01, 1.6f); // 0..~20 dB

    // Threshold moves with Peak Reduction. Higher Peak Reduction => lower threshold.
    const float thresholdDb = -6.0f - 24.0f * peak01; // -6 .. -30 dB

    float ratio = 4.0f;
    if (ratioChoice == 0) ratio = 2.0f;
    if (ratioChoice == 1) ratio = 4.0f;
    if (ratioChoice == 2) ratio = 20.0f; // "infinite-ish"
    if (modeChoice == 1)  ratio = juce::jmax (ratio, 12.0f); // LIMIT pushes ratio higher

    const float makeup = dbToGain (gainDb);

    // Meter accumulators
    double inSqSum = 0.0;
    double outSqSum = 0.0;

    const int numSamples = buffer.getNumSamples();
    const int chs = juce::jmax (1, totalNumInputChannels);

    if (bypassed)
    {
        for (int ch = 0; ch < chs; ++ch)
        {
            const float* r = buffer.getReadPointer (ch);
            for (int n = 0; n < numSamples; ++n)
                inSqSum += (double) r[n] * (double) r[n];
        }

        const float inRms = std::sqrt ((float) (inSqSum / (double) (numSamples * chs)));
        inRmsDb.store (gainToDb (inRms));
        outRmsDb.store (gainToDb (inRms));
        grDb.store (0.0f);
        return;
    }

    // Process sample-by-sample for now (clear scaffolding). We'll vectorise later.
    for (int n = 0; n < numSamples; ++n)
    {
        float inL = buffer.getSample (0, n);
        float inR = (chs > 1) ? buffer.getSample (1, n) : inL;

        // Input meters
        inSqSum += (double) inL * (double) inL;
        if (chs > 1) inSqSum += (double) inR * (double) inR;

        // Tube stage (optional colour, should be near-clean at 0)
        inL = satTube (inL, bias01);
        inR = satTube (inR, bias01);

        // Detector signal (stereo linked)
        const float mono = 0.5f * (inL + inR);
        const float lp = 0.5f * (scLowpassL.process (mono) + scLowpassR.process (mono));
        const float hfComp = mono - lp;                 // crude HF component
        const float det = mono + hf01 * 0.85f * hfComp; // emphasis

        const float x2 = det * det;
        const float coeff = (x2 > env) ? envAttackCoeff : envReleaseCoeff;
        env = coeff * env + (1.0f - coeff) * x2;

        const float envRms = std::sqrt (juce::jmax (env, 1.0e-12f));
        const float levelDb = 20.0f * std::log10 (envRms + 1.0e-9f);

        // Gain computer
        const float overDb = levelDb - thresholdDb;
        const float grTarget = (overDb > 0.0f) ? (overDb * (1.0f - 1.0f / ratio)) : 0.0f;
        const float grLimited = juce::jlimit (0.0f, maxGrDb, grTarget);

        const float grCoeff = (grLimited > grSmoothedDb) ? grAttackCoeff : grReleaseCoeff;
        grSmoothedDb = grCoeff * grSmoothedDb + (1.0f - grCoeff) * grLimited;

        const float g = dbToGain (-grSmoothedDb);
        float outL = inL * g * makeup;
        float outR = inR * g * makeup;

        // Output meters
        outSqSum += (double) outL * (double) outL;
        if (chs > 1) outSqSum += (double) outR * (double) outR;

        buffer.setSample (0, n, outL);
        if (chs > 1)
            buffer.setSample (1, n, outR);
    }

    const float inRms  = std::sqrt ((float) (inSqSum  / (double) (numSamples * chs)));
    const float outRms = std::sqrt ((float) (outSqSum / (double) (numSamples * chs)));

    inRmsDb.store (gainToDb (inRms));
    outRmsDb.store (gainToDb (outRms));
    grDb.store (-grSmoothedDb);
}

//==============================================================================
bool OptoVoxAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* OptoVoxAudioProcessor::createEditor()
{
    return new OptoVoxAudioProcessorEditor (*this);
}

//==============================================================================
void OptoVoxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos (destData, true);
    apvts.state.writeToStream (mos);
}

void OptoVoxAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto vt = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (vt.isValid())
        apvts.replaceState (vt);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OptoVoxAudioProcessor();
}
