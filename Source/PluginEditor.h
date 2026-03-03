/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class OptoVoxAudioProcessorEditor  : public juce::AudioProcessorEditor
                                 , private juce::Timer
                                 , private juce::AudioProcessorValueTreeState::Listener
{
public:
    OptoVoxAudioProcessorEditor (OptoVoxAudioProcessor&);
    ~OptoVoxAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    //==============================================================================
    // Theme based on the v4 HTML mock.
    struct Theme
    {
        static juce::Colour bg()      { return juce::Colour (0xFF0C0F15); }
        static juce::Colour surface() { return juce::Colour (0xFF111720); }
        static juce::Colour surface2(){ return juce::Colour (0xFF0E131C); }
        static juce::Colour line()    { return juce::Colour (0xFF1C2535); }
        static juce::Colour green()   { return juce::Colour (0xFF00E676); }
        static juce::Colour text()    { return juce::Colour (0xFFB8C8DC); }
        static juce::Colour dim()     { return juce::Colour (0xFF48607A); }
        static juce::Colour muted()   { return juce::Colour (0xFF28384E); }
        static juce::Colour red()     { return juce::Colour (0xFFFF4458); }
        static juce::Colour yellow()  { return juce::Colour (0xFFF5C400); }
        static juce::Colour gdim()    { return juce::Colour (0x1700E676); } // ~0.09 alpha
    };

    //==============================================================================
    // Small UI building blocks.
    class LedDot : public juce::Component
    {
    public:
        void setOn (bool isOn, bool isBypassed) { on = isOn; bypassed = isBypassed; repaint(); }
        void paint (juce::Graphics& g) override;

    private:
        bool on = true;
        bool bypassed = false;
    };

    class Badge : public juce::Component
    {
    public:
        void setText (juce::String t) { text = std::move (t); repaint(); }
        void paint (juce::Graphics& g) override;

    private:
        juce::String text { "OptoVox" };
    };

    class ValueReadout : public juce::Component
    {
    public:
        void setValueText (juce::String v, juce::String u) { value = std::move (v); unit = std::move (u); repaint(); }
        void paint (juce::Graphics& g) override;

    private:
        juce::String value { "0" }, unit { "" };
    };

    class KnobLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                               juce::Slider& slider) override;
    };

    // JUCE TextButton does not expose an outlineColourId or a per-button setFont().
    // We mimic the HTML v4 look with a small LookAndFeel that draws the border and
    // returns a compact font.
    class ButtonLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit ButtonLookAndFeel (float fontPx) : fontSizePx (fontPx) {}

        juce::Font getTextButtonFont (juce::TextButton&, int) override
        {
            return juce::Font (fontSizePx);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                   const juce::Colour& backgroundColour,
                                   bool isMouseOverButton, bool isButtonDown) override
        {
            juce::ignoreUnused (backgroundColour);
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);

            const bool isOn = b.getToggleState();
            auto fill = isOn ? Theme::gdim() : juce::Colours::transparentBlack;
            auto stroke = Theme::line();

            if (isOn || isMouseOverButton || isButtonDown)
            {
                // Special-case the bypass button to go red when engaged.
                stroke = (b.getComponentID() == "bypass") ? Theme::red() : Theme::green();
            }

            g.setColour (fill);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (stroke.withAlpha (isOn ? 0.85f : 0.60f));
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
        }

    private:
        float fontSizePx = 8.0f;
    };

    class Segmented : public juce::Component
    {
    public:
        void configure (std::initializer_list<juce::String> labels);
        void setButtonLookAndFeel (juce::LookAndFeel* lf) { btnLnF = lf; for (auto* b : buttons) b->setLookAndFeel (lf); }
        void setSelectedIndex (int idx, bool send = false);
        void paint (juce::Graphics& g) override;
        void resized() override;

        std::function<void(int)> onSelect;

    private:
        juce::OwnedArray<juce::TextButton> buttons;
        int groupId = 7001;
        juce::LookAndFeel* btnLnF = nullptr;
    };

    class VUMeter : public juce::Component
    {
    public:
        void setNeedleDegrees (float deg) { needleDeg = deg; repaint(); }
        void paint (juce::Graphics& g) override;

    private:
        float needleDeg = -42.0f;
    };

    class BarMeter : public juce::Component
    {
    public:
        enum Kind { LR, GR };
        explicit BarMeter (Kind k) : kind (k) {}
        void setLevel01 (float v) { level01 = juce::jlimit (0.0f, 1.0f, v); repaint(); }
        void paint (juce::Graphics& g) override;

    private:
        Kind kind;
        float level01 = 0.0f;
    };

    class FooterGRTrack : public juce::Component
    {
    public:
        void setAmount01 (float v) { amount01 = juce::jlimit (0.0f, 1.0f, v); repaint(); }
        void paint (juce::Graphics& g) override;

    private:
        float amount01 = 0.0f;
    };

    //==============================================================================
    OptoVoxAudioProcessor& audioProcessor;
    KnobLookAndFeel knobLnF;

    ButtonLookAndFeel headerBtnLnF { 8.0f };
    ButtonLookAndFeel segBtnLnF    { 7.0f };

    // Header
    Badge badge;
    juce::Label brandName;
    LedDot statusLed;
    juce::Label statusText;
    juce::TextButton resetButton { "Reset" };
    juce::TextButton bypassButton { "Bypass" };

    // Main - Left knobs
    juce::Label peakLabel, gainLabel, hfLabel, biasLabel;
    juce::Slider peakSlider, gainSlider, hfSlider, biasSlider;
    ValueReadout peakReadout, gainReadout, hfReadout, biasReadout;

    // Left toggles
    juce::Label modeKey, ratioKey, attackKey;
    Segmented modeSeg, ratioSeg, attackSeg;

    // Center
    juce::Label outputLabel;
    VUMeter vu;
    BarMeter barL { BarMeter::LR }, barR { BarMeter::LR }, barGR { BarMeter::GR };
    juce::Label grBig, grSub;

    // Right toggles
    juce::Label sideKey, outKey, relKey;
    Segmented sideSeg, outSeg, relSeg;

    // Footer
    juce::Label fModeKey, fModeVal;
    juce::Label fRatioKey, fRatioVal;
    juce::Label fGRKey, fGRVal;
    FooterGRTrack fGRTrack;
    juce::Label fSerialKey, fSerialVal;

    // Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> peakAttachment, gainAttachment, hfAttachment, biasAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    std::unique_ptr<juce::ParameterAttachment> modeAttach, ratioAttach, attackAttach, releaseAttach, sidechainAttach, outmodeAttach;

    // Cached meter values (updated on timer)
    std::atomic<float> inDb { -100.0f }, outDb { -100.0f }, grDb { 0.0f };

    // Helpers
    void setupKnob (juce::Slider& s);
    void setupLabel (juce::Label& l, float size, juce::Colour c, juce::Justification just);
    void applyHeaderButtonStyle (juce::TextButton& b);
    void updateReadouts();
    void updateStatusChip();
    void resetAllParams();
    void buildChoiceAttachments();
    void syncFooterFromParams();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoVoxAudioProcessorEditor)
};
