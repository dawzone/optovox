/*
  ==============================================================================

    OptoVox UI rebuilt to match the clean v4 HTML layout.
    - Native JUCE drawing (no WebView)
    - APVTS attachments for all controls
    - Simple meters (VU needle + L/R/GR bars)

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Small components
void OptoVoxAudioProcessorEditor::LedDot::paint (juce::Graphics& g)
{
    const auto c = bypassed ? Theme::red() : Theme::green();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (c.withAlpha (on ? 1.0f : 0.25f));
    g.fillEllipse (r);

    g.setColour (c.withAlpha (on ? 0.30f : 0.10f));
    g.drawEllipse (r.expanded (2.0f), 2.0f);
}

void OptoVoxAudioProcessorEditor::Badge::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::green());
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black);
    g.setFont (juce::Font (15.0f, juce::Font::bold));
    g.drawFittedText (text, getLocalBounds().reduced (6, 0), juce::Justification::centred, 1);
}

void OptoVoxAudioProcessorEditor::ValueReadout::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setFont (juce::Font (10.0f));
    g.setColour (Theme::green());
    const int valueW = (int) std::ceil (g.getCurrentFont().getStringWidthFloat (value));
    auto left = r.withTrimmedRight (juce::jmax (0, r.getWidth() - valueW - 12));
    auto right = r.withTrimmedLeft (left.getWidth());
    g.drawFittedText (value, left, juce::Justification::centredRight, 1);

    g.setFont (juce::Font (7.0f));
    g.setColour (Theme::dim());
    g.drawFittedText (unit, right, juce::Justification::centredLeft, 1);
}

void OptoVoxAudioProcessorEditor::KnobLookAndFeel::drawRotarySlider (
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (3.0f);
    auto cx = bounds.getCentreX();
    auto cy = bounds.getCentreY();
    auto r  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.42f;
    auto tr = r * 0.74f;

    const auto sa = rotaryStartAngle;
    const auto ea = rotaryEndAngle;
    const auto va = sa + sliderPosProportional * (ea - sa);

    // body
    {
        juce::ColourGradient grad (juce::Colour (0xFF18202E), cx - r * 0.2f, cy - r * 0.25f,
                                   juce::Colour (0xFF08090E), cx, cy + r, false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r);

        g.setColour (Theme::line());
        g.drawEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r, 1.5f);
    }

    // track
    {
        juce::Path p;
        p.addCentredArc (cx, cy, tr, tr, 0.0f, sa, ea, true);
        g.setColour (juce::Colour (0xFF1A2535));
        g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // value arc
    if (sliderPosProportional > 0.005f)
    {
        juce::Path p;
        p.addCentredArc (cx, cy, tr, tr, 0.0f, sa, va, true);
        g.setColour (Theme::green());
        g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // inner
    {
        auto ir = r * 0.55f;
        juce::ColourGradient grad (juce::Colour (0xFF1A2030), cx - ir * 0.3f, cy - ir * 0.3f,
                                   juce::Colour (0xFF090C14), cx, cy + ir, false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - ir, cy - ir, 2.0f * ir, 2.0f * ir);
        g.setColour (juce::Colour (0xFF1E2A3A));
        g.drawEllipse (cx - ir, cy - ir, 2.0f * ir, 2.0f * ir, 1.0f);

        // tick
        const auto ta = va - juce::MathConstants<float>::halfPi;
        const auto p1 = juce::Point<float> (cx + std::cos (ta) * ir * 0.48f, cy + std::sin (ta) * ir * 0.48f);
        const auto p2 = juce::Point<float> (cx + std::cos (ta) * ir * 0.86f, cy + std::sin (ta) * ir * 0.86f);
        g.setColour (Theme::green());
        g.drawLine ({ p1.x, p1.y, p2.x, p2.y }, 2.0f);
    }

    juce::ignoreUnused (slider);
}

void OptoVoxAudioProcessorEditor::Segmented::configure (std::initializer_list<juce::String> labels)
{
    buttons.clear();
    for (const auto& s : labels)
    {
        auto* b = buttons.add (new juce::TextButton (s));
        if (btnLnF != nullptr)
            b->setLookAndFeel (btnLnF);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (groupId);
        b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b->setColour (juce::TextButton::buttonOnColourId, Theme::gdim());
        b->setColour (juce::TextButton::textColourOffId, Theme::dim());
        b->setColour (juce::TextButton::textColourOnId, Theme::green());
        b->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        b->onClick = [this, b]
        {
            const int idx = buttons.indexOf (b);
            if (idx >= 0 && onSelect)
                onSelect (idx);
        };
        addAndMakeVisible (*b);
    }
    resized();
}

void OptoVoxAudioProcessorEditor::Segmented::setSelectedIndex (int idx, bool send)
{
    if (buttons.isEmpty())
        return;
    idx = juce::jlimit (0, buttons.size() - 1, idx);
    if (auto* b = buttons[idx])
        b->setToggleState (true, send ? juce::sendNotification : juce::dontSendNotification);
}

void OptoVoxAudioProcessorEditor::Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::line());
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
}

void OptoVoxAudioProcessorEditor::Segmented::resized()
{
    auto r = getLocalBounds();
    if (buttons.isEmpty())
        return;
    const int w = r.getWidth() / buttons.size();
    for (int i = 0; i < buttons.size(); ++i)
    {
        auto b = r.removeFromLeft (i == buttons.size() - 1 ? r.getWidth() : w);
        buttons[i]->setBounds (b);
    }
}

void OptoVoxAudioProcessorEditor::VUMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    auto face = r.removeFromTop (66.0f);
    g.setColour (juce::Colour (0xFF060910));
    g.fillRoundedRectangle (face, 3.0f);
    g.setColour (Theme::line());
    g.drawRoundedRectangle (face, 3.0f, 1.0f);

    auto drawTick = [&g] (float x1, float y1, float x2, float y2, juce::Colour c)
    {
        g.setColour (c);
        g.drawLine (x1, y1, x2, y2, 0.8f);
    };

    const float left = face.getX();
    const float top  = face.getY();
    drawTick (left + 9,  top + 62, left + 12, top + 56, Theme::green().withAlpha (0.25f));
    drawTick (left + 21, top + 54, left + 24, top + 48, Theme::green().withAlpha (0.25f));
    drawTick (left + 34, top + 48, left + 36, top + 43, Theme::green().withAlpha (0.25f));
    drawTick (left + 52, top + 46, left + 52, top + 40, Theme::green().withAlpha (0.50f));
    drawTick (left + 68, top + 48, left + 66, top + 43, Theme::yellow().withAlpha (0.40f));
    drawTick (left + 81, top + 54, left + 79, top + 49, Theme::red().withAlpha (0.40f));

    g.setFont (juce::Font (4.8f));
    g.setColour (Theme::green().withAlpha (0.35f));
    g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("\xE2\x88\x92")) + "20", { (int) (left + 2), (int) (top + 60), 24, 10 }, juce::Justification::left, 1);
    g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("\xE2\x88\x92")) + "10", { (int) (left + 14), (int) (top + 54), 24, 10 }, juce::Justification::left, 1);
    g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("\xE2\x88\x92")) + "7",  { (int) (left + 29), (int) (top + 48), 24, 10 }, juce::Justification::left, 1);
    g.setColour (Theme::green().withAlpha (0.55f));
    g.drawFittedText ("0", { (int) (left + 48), (int) (top + 48), 12, 10 }, juce::Justification::left, 1);
    g.setColour (Theme::yellow().withAlpha (0.45f));
    g.drawFittedText ("+3", { (int) (left + 62), (int) (top + 48), 24, 10 }, juce::Justification::left, 1);
    g.setColour (Theme::red().withAlpha (0.45f));
    g.drawFittedText ("+7", { (int) (left + 75), (int) (top + 54), 24, 10 }, juce::Justification::left, 1);

    auto pivot = juce::Point<float> (face.getCentreX(), face.getBottom() - 2.0f);
    const float needleLen = 54.0f;
    auto a = juce::degreesToRadians (needleDeg);
    auto tip = juce::Point<float> (pivot.x + std::sin (a) * needleLen,
                                   pivot.y - std::cos (a) * needleLen);

    juce::ColourGradient ng (Theme::green(), pivot.x, tip.y, Theme::green().withAlpha (0.4f), pivot.x, pivot.y, false);
    g.setGradientFill (ng);
    g.drawLine ({ pivot.x, pivot.y, tip.x, tip.y }, 1.5f);
    g.setColour (Theme::green());
    g.fillEllipse (pivot.x - 3.5f, pivot.y - 3.5f, 7.0f, 7.0f);

    // foot
    auto foot = r;
    foot.setHeight (8.0f);
    foot.setY (face.getBottom());
    g.setColour (Theme::surface());
    g.fillRoundedRectangle (foot, 3.0f);
    g.setColour (Theme::line());
    g.drawRoundedRectangle (foot, 3.0f, 1.0f);
    g.setFont (juce::Font (7.0f, juce::Font::bold));
    g.setColour (Theme::muted());
    g.drawFittedText ("VU", foot.toNearestInt(), juce::Justification::centred, 1);
}

void OptoVoxAudioProcessorEditor::BarMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0x80000000));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (Theme::line());
    g.drawRoundedRectangle (r, 2.0f, 1.0f);

    auto fill = r.reduced (1.0f);
    fill.setY (fill.getBottom() - fill.getHeight() * level01);
    fill.setHeight (fill.getHeight() * level01);
    if (fill.getHeight() <= 0.5f)
        return;

    if (kind == LR)
    {
        juce::ColourGradient grad (Theme::red(), fill.getCentreX(), fill.getY(), Theme::green(), fill.getCentreX(), fill.getBottom(), false);
        grad.addColour (0.28, Theme::yellow());
        grad.addColour (0.70, Theme::green());
        g.setGradientFill (grad);
    }
    else
    {
        juce::ColourGradient grad (juce::Colour (0xFFFF6820), fill.getCentreX(), fill.getY(), Theme::green(), fill.getCentreX(), fill.getBottom(), false);
        grad.addColour (0.45, Theme::yellow());
        grad.addColour (1.00, Theme::green());
        g.setGradientFill (grad);
    }

    g.fillRoundedRectangle (fill, 1.0f);
}

void OptoVoxAudioProcessorEditor::FooterGRTrack::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0x80000000));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (Theme::line());
    g.drawRoundedRectangle (r, 2.0f, 1.0f);

    auto fill = r.reduced (1.0f);
    fill.setWidth (fill.getWidth() * amount01);
    if (fill.getWidth() <= 0.5f)
        return;

    juce::ColourGradient grad (Theme::green(), fill.getX(), fill.getCentreY(), Theme::red(), fill.getRight(), fill.getCentreY(), false);
    grad.addColour (0.70, Theme::yellow());
    g.setGradientFill (grad);
    g.fillRoundedRectangle (fill, 2.0f);
}

//==============================================================================
OptoVoxAudioProcessorEditor::OptoVoxAudioProcessorEditor (OptoVoxAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (660, 420);

    // Header
    badge.setText ("OptoVox");
    addAndMakeVisible (badge);

    setupLabel (brandName, 11.0f, Theme::text(), juce::Justification::centredLeft);
    brandName.setText ("Optical Vocal Compressor", juce::dontSendNotification);
    brandName.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (brandName);

    addAndMakeVisible (statusLed);
    setupLabel (statusText, 8.0f, Theme::green(), juce::Justification::centredLeft);
    statusText.setText ("ACTIVE", juce::dontSendNotification);
    addAndMakeVisible (statusText);

    applyHeaderButtonStyle (resetButton);
    applyHeaderButtonStyle (bypassButton);

    resetButton.setLookAndFeel (&headerBtnLnF);
    bypassButton.setLookAndFeel (&headerBtnLnF);

    bypassButton.setComponentID ("bypass");
    addAndMakeVisible (resetButton);
    addAndMakeVisible (bypassButton);

    resetButton.onClick = [this] { resetAllParams(); };
    bypassButton.onClick = [this]
    {
        if (auto* param = audioProcessor.apvts.getParameter (OptoVoxParams::bypass))
        {
            const float current = param->getValue();
            param->setValueNotifyingHost (current > 0.5f ? 0.0f : 1.0f);
        }
    };

    // Knobs
    setupKnob (peakSlider);
    setupKnob (gainSlider);
    setupKnob (hfSlider);
    setupKnob (biasSlider);

    peakSlider.setLookAndFeel (&knobLnF);
    gainSlider.setLookAndFeel (&knobLnF);
    hfSlider.setLookAndFeel (&knobLnF);
    biasSlider.setLookAndFeel (&knobLnF);

    addAndMakeVisible (peakSlider);
    addAndMakeVisible (gainSlider);
    addAndMakeVisible (hfSlider);
    addAndMakeVisible (biasSlider);
    addAndMakeVisible (peakReadout);
    addAndMakeVisible (gainReadout);
    addAndMakeVisible (hfReadout);
    addAndMakeVisible (biasReadout);

    setupLabel (peakLabel, 7.0f, Theme::dim(), juce::Justification::centred);
    peakLabel.setText ("PEAK\nREDUCTION", juce::dontSendNotification);
    setupLabel (gainLabel, 7.0f, Theme::dim(), juce::Justification::centred);
    gainLabel.setText ("OUTPUT\nGAIN", juce::dontSendNotification);
    setupLabel (hfLabel, 7.0f, Theme::dim(), juce::Justification::centred);
    hfLabel.setText ("HF\nEQ", juce::dontSendNotification);
    setupLabel (biasLabel, 7.0f, Theme::dim(), juce::Justification::centred);
    biasLabel.setText ("TUBE\nBIAS", juce::dontSendNotification);
    addAndMakeVisible (peakLabel);
    addAndMakeVisible (gainLabel);
    addAndMakeVisible (hfLabel);
    addAndMakeVisible (biasLabel);

    // Left segmented groups
    setupLabel (modeKey, 7.0f, Theme::dim(), juce::Justification::centredLeft);
    modeKey.setText ("MODE", juce::dontSendNotification);
    setupLabel (ratioKey, 7.0f, Theme::dim(), juce::Justification::centredLeft);
    ratioKey.setText ("RATIO", juce::dontSendNotification);
    setupLabel (attackKey, 7.0f, Theme::dim(), juce::Justification::centredLeft);
    attackKey.setText ("ATTACK", juce::dontSendNotification);
    addAndMakeVisible (modeKey);
    addAndMakeVisible (ratioKey);
    addAndMakeVisible (attackKey);

    modeSeg.configure ({ "Comp", "Limit" });
    ratioSeg.configure ({ "2:1",
                          "4:1",
                          juce::String (juce::CharPointer_UTF8 ("\xE2\x88\x9E:1")) });
    attackSeg.configure ({ "Auto", "Fast", "Slow" });

    modeSeg.setButtonLookAndFeel (&segBtnLnF);
    ratioSeg.setButtonLookAndFeel (&segBtnLnF);
    attackSeg.setButtonLookAndFeel (&segBtnLnF);
    addAndMakeVisible (modeSeg);
    addAndMakeVisible (ratioSeg);
    addAndMakeVisible (attackSeg);

    // Center
    setupLabel (outputLabel, 7.0f, Theme::muted(), juce::Justification::centred);
    outputLabel.setText ("OUTPUT", juce::dontSendNotification);
    addAndMakeVisible (outputLabel);
    addAndMakeVisible (vu);
    addAndMakeVisible (barL);
    addAndMakeVisible (barR);
    addAndMakeVisible (barGR);
    setupLabel (grBig, 13.0f, Theme::green(), juce::Justification::centred);
    setupLabel (grSub, 6.5f, Theme::muted(), juce::Justification::centred);
    grSub.setText ("GAIN REDUCTION", juce::dontSendNotification);
    addAndMakeVisible (grBig);
    addAndMakeVisible (grSub);

    // Right segmented groups
    setupLabel (sideKey, 7.0f, Theme::dim(), juce::Justification::centredLeft);
    sideKey.setText ("SIDECHAIN", juce::dontSendNotification);
    setupLabel (outKey, 7.0f, Theme::dim(), juce::Justification::centredLeft);
    outKey.setText ("OUTPUT", juce::dontSendNotification);
    setupLabel (relKey, 7.0f, Theme::dim(), juce::Justification::centredLeft);
    relKey.setText ("RELEASE", juce::dontSendNotification);
    addAndMakeVisible (sideKey);
    addAndMakeVisible (outKey);
    addAndMakeVisible (relKey);

    sideSeg.configure ({ "Int", "Ext", "HF" });
    outSeg.configure ({ "St", "Mono", "M/S" });
    relSeg.configure ({ "Auto", "Fast", "Slow" });

    sideSeg.setButtonLookAndFeel (&segBtnLnF);
    outSeg.setButtonLookAndFeel (&segBtnLnF);
    relSeg.setButtonLookAndFeel (&segBtnLnF);
    addAndMakeVisible (sideSeg);
    addAndMakeVisible (outSeg);
    addAndMakeVisible (relSeg);

    // Footer
    setupLabel (fModeKey, 6.5f, Theme::muted(), juce::Justification::centredLeft);
    fModeKey.setText ("MODE", juce::dontSendNotification);
    setupLabel (fModeVal, 10.0f, Theme::green(), juce::Justification::centredLeft);
    fModeVal.setText ("Comp", juce::dontSendNotification);
    addAndMakeVisible (fModeKey);
    addAndMakeVisible (fModeVal);

    setupLabel (fRatioKey, 6.5f, Theme::muted(), juce::Justification::centredLeft);
    fRatioKey.setText ("RATIO", juce::dontSendNotification);
    setupLabel (fRatioVal, 10.0f, Theme::green(), juce::Justification::centredLeft);
    fRatioVal.setText ("4 : 1", juce::dontSendNotification);
    addAndMakeVisible (fRatioKey);
    addAndMakeVisible (fRatioVal);

    setupLabel (fGRKey, 6.5f, Theme::muted(), juce::Justification::centredLeft);
    fGRKey.setText ("GAIN REDUCTION", juce::dontSendNotification);
    setupLabel (fGRVal, 6.5f, Theme::green(), juce::Justification::centredRight);
    fGRVal.setText (juce::String (juce::CharPointer_UTF8 ("\xE2\x88\x92")) + "0.0 dB", juce::dontSendNotification);
    addAndMakeVisible (fGRKey);
    addAndMakeVisible (fGRVal);
    addAndMakeVisible (fGRTrack);

    setupLabel (fSerialKey, 6.5f, Theme::muted(), juce::Justification::centredLeft);
    fSerialKey.setText ("SERIAL", juce::dontSendNotification);
    setupLabel (fSerialVal, 10.0f, Theme::dim(), juce::Justification::centredLeft);
    fSerialVal.setText (juce::String (juce::CharPointer_UTF8 ("SN \xC2\xB7 1172")), juce::dontSendNotification);
    addAndMakeVisible (fSerialKey);
    addAndMakeVisible (fSerialVal);

    // Attachments
    peakAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, OptoVoxParams::peak, peakSlider);
    gainAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, OptoVoxParams::gain, gainSlider);
    hfAttachment   = std::make_unique<SliderAttachment> (audioProcessor.apvts, OptoVoxParams::hf,   hfSlider);
    biasAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, OptoVoxParams::bias, biasSlider);
    bypassAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, OptoVoxParams::bypass, bypassButton);

    buildChoiceAttachments();

    // Listen to key params for footer updates
    audioProcessor.apvts.addParameterListener (OptoVoxParams::mode, this);
    audioProcessor.apvts.addParameterListener (OptoVoxParams::ratio, this);
    audioProcessor.apvts.addParameterListener (OptoVoxParams::bypass, this);

    updateReadouts();
    syncFooterFromParams();
    updateStatusChip();

    startTimerHz (30);
}

OptoVoxAudioProcessorEditor::~OptoVoxAudioProcessorEditor()
{
    audioProcessor.apvts.removeParameterListener (OptoVoxParams::mode, this);
    audioProcessor.apvts.removeParameterListener (OptoVoxParams::ratio, this);
    audioProcessor.apvts.removeParameterListener (OptoVoxParams::bypass, this);

    peakSlider.setLookAndFeel (nullptr);
    gainSlider.setLookAndFeel (nullptr);
    hfSlider.setLookAndFeel (nullptr);
    biasSlider.setLookAndFeel (nullptr);

    resetButton.setLookAndFeel (nullptr);
    bypassButton.setLookAndFeel (nullptr);

    modeSeg.setButtonLookAndFeel (nullptr);
    ratioSeg.setButtonLookAndFeel (nullptr);
    attackSeg.setButtonLookAndFeel (nullptr);
    sideSeg.setButtonLookAndFeel (nullptr);
    outSeg.setButtonLookAndFeel (nullptr);
    relSeg.setButtonLookAndFeel (nullptr);
}

//==============================================================================
void OptoVoxAudioProcessorEditor::setupKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 0.75f,
                           juce::MathConstants<float>::pi * 2.25f,
                           true);
    s.setMouseDragSensitivity (160);
    s.setDoubleClickReturnValue (true, s.getValue());
}

void OptoVoxAudioProcessorEditor::setupLabel (juce::Label& l, float size, juce::Colour c, juce::Justification just)
{
    l.setFont (juce::Font (size));
    l.setColour (juce::Label::textColourId, c);
    l.setJustificationType (just);
}

void OptoVoxAudioProcessorEditor::applyHeaderButtonStyle (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    b.setColour (juce::TextButton::buttonOnColourId, Theme::gdim());
    b.setColour (juce::TextButton::textColourOffId, Theme::dim());
    b.setColour (juce::TextButton::textColourOnId, Theme::green());
    b.setToggleState (false, juce::dontSendNotification);
    b.setClickingTogglesState (false);
}

//==============================================================================
void OptoVoxAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg());

    auto shell = getLocalBounds().reduced (10).toFloat();
    g.setColour (Theme::surface());
    g.fillRoundedRectangle (shell, 8.0f);
    g.setColour (Theme::line());
    g.drawRoundedRectangle (shell, 8.0f, 1.0f);

    // top green hairline
    g.setColour (Theme::green().withAlpha (0.4f));
    g.drawLine (shell.getX() + shell.getWidth() * 0.10f, shell.getY() + 1.0f,
                shell.getRight() - shell.getWidth() * 0.10f, shell.getY() + 1.0f, 1.0f);
}

void OptoVoxAudioProcessorEditor::resized()
{
    auto outer = getLocalBounds().reduced (10);
    auto shell = outer.reduced (0);

    // Header
    auto header = shell.removeFromTop (52);
    header.reduce (12, 8);

    auto leftHeader = header.removeFromLeft (260);
    badge.setBounds (leftHeader.removeFromLeft (84).withHeight (24).withY (header.getY() + 10));
    brandName.setBounds (leftHeader.reduced (10, 6));

    auto midHeader = header.removeFromLeft (160);
    auto chip = midHeader.withSizeKeepingCentre (120, 22);
    statusLed.setBounds (chip.removeFromLeft (14).withSizeKeepingCentre (10, 10));
    statusText.setBounds (chip.reduced (2, 0));

    auto rightHeader = header;
    resetButton.setBounds (rightHeader.removeFromRight (82).withHeight (22).withY (header.getY() + 10));
    rightHeader.removeFromRight (8);
    bypassButton.setBounds (rightHeader.removeFromRight (82).withHeight (22).withY (header.getY() + 10));

    // Main
    auto main = shell;
    auto footer = main.removeFromBottom (66);
    auto mainTop = main;

    // Split main into L / C / R
    auto left = mainTop.removeFromLeft ((mainTop.getWidth() - 130) / 2);
    auto center = mainTop.removeFromLeft (130);
    auto right = mainTop;

    // Column padding
    left.reduce (10, 12);
    right.reduce (10, 12);
    center.reduce (8, 10);

    // Left column: knob pair + toggles
    {
        auto knobRow = left.removeFromTop (160);
        auto k1 = knobRow.removeFromLeft (knobRow.getWidth() / 2);
        auto k2 = knobRow;

        auto layoutKnob = [] (juce::Rectangle<int> area, juce::Label& lab, juce::Slider& s, ValueReadout& v)
        {
            auto label = area.removeFromTop (26);
            lab.setBounds (label);
            auto knob = area.removeFromTop (90);
            s.setBounds (knob.withSizeKeepingCentre (72, 72));
            v.setBounds (area.removeFromTop (18).withTrimmedTop (2));
        };

        layoutKnob (k1, peakLabel, peakSlider, peakReadout);
        layoutKnob (k2, gainLabel, gainSlider, gainReadout);

        left.removeFromTop (12);

        auto toggles = left;
        const int rowH = 44;
        auto row1 = toggles.removeFromTop (rowH);
        auto row2 = toggles.removeFromTop (rowH);
        auto row3 = toggles.removeFromTop (rowH);

        auto placeRow = [] (juce::Rectangle<int> row, juce::Label& key, Segmented& seg)
        {
            key.setBounds (row.removeFromLeft (56));
            seg.setBounds (row.reduced (2, 8));
        };

        placeRow (row1, modeKey, modeSeg);
        placeRow (row2, ratioKey, ratioSeg);
        placeRow (row3, attackKey, attackSeg);
    }

    // Center column
    {
        outputLabel.setBounds (center.removeFromTop (18));
        center.removeFromTop (2);

        auto vuArea = center.removeFromTop (90);
        vu.setBounds (vuArea.withSizeKeepingCentre (104, 74));
        center.removeFromTop (10);

        auto barsArea = center.removeFromTop (70);
        auto colW = barsArea.getWidth() / 3;
        auto b1 = barsArea.removeFromLeft (colW);
        auto b2 = barsArea.removeFromLeft (colW);
        auto b3 = barsArea;
        barL.setBounds (b1.withSizeKeepingCentre (10, 52));
        barR.setBounds (b2.withSizeKeepingCentre (10, 52));
        barGR.setBounds (b3.withSizeKeepingCentre (10, 52));

        center.removeFromTop (4);
        grBig.setBounds (center.removeFromTop (18));
        grSub.setBounds (center.removeFromTop (12));
    }

    // Right column: knob pair + toggles
    {
        auto knobRow = right.removeFromTop (160);
        auto k1 = knobRow.removeFromLeft (knobRow.getWidth() / 2);
        auto k2 = knobRow;

        auto layoutKnob = [] (juce::Rectangle<int> area, juce::Label& lab, juce::Slider& s, ValueReadout& v)
        {
            auto label = area.removeFromTop (26);
            lab.setBounds (label);
            auto knob = area.removeFromTop (90);
            s.setBounds (knob.withSizeKeepingCentre (72, 72));
            v.setBounds (area.removeFromTop (18).withTrimmedTop (2));
        };

        layoutKnob (k1, hfLabel, hfSlider, hfReadout);
        layoutKnob (k2, biasLabel, biasSlider, biasReadout);

        right.removeFromTop (12);

        auto toggles = right;
        const int rowH = 44;
        auto row1 = toggles.removeFromTop (rowH);
        auto row2 = toggles.removeFromTop (rowH);
        auto row3 = toggles.removeFromTop (rowH);

        auto placeRow = [] (juce::Rectangle<int> row, juce::Label& key, Segmented& seg)
        {
            key.setBounds (row.removeFromLeft (56));
            seg.setBounds (row.reduced (2, 8));
        };

        placeRow (row1, sideKey, sideSeg);
        placeRow (row2, outKey, outSeg);
        placeRow (row3, relKey, relSeg);
    }

    // Footer strip layout (5 columns where GR bar spans 2)
    {
        auto f = footer;
        f.setHeight (footer.getHeight());
        f.reduce (0, 0);

        auto col1 = f.removeFromLeft (f.getWidth() / 5);
        auto col2 = f.removeFromLeft (f.getWidth() / 4); // after removing col1, adjust
        auto col3 = f.removeFromLeft (f.getWidth() / 2); // GR spans 2 columns
        auto col4 = f;

        auto placeStat = [] (juce::Rectangle<int> c, juce::Label& key, juce::Label& val)
        {
            c.reduce (10, 10);
            key.setBounds (c.removeFromTop (12));
            val.setBounds (c.removeFromTop (18));
        };

        placeStat (col1, fModeKey, fModeVal);
        placeStat (col2, fRatioKey, fRatioVal);

        // GR bar block
        {
            auto c = col3;
            c.reduce (10, 10);
            auto top = c.removeFromTop (12);
            fGRKey.setBounds (top.removeFromLeft (top.getWidth() - 60));
            fGRVal.setBounds (top);

            auto track = c.removeFromTop (10).withTrimmedTop (4);
            fGRTrack.setBounds (track.withHeight (4));
        }

        placeStat (col4, fSerialKey, fSerialVal);
    }
}

//==============================================================================
void OptoVoxAudioProcessorEditor::timerCallback()
{
    inDb.store (audioProcessor.getInputRmsDb());
    outDb.store (audioProcessor.getOutputRmsDb());
    grDb.store (audioProcessor.getGainReductionDb());

    updateReadouts();

    // meters
    const float out = outDb.load();
    const float in  = inDb.load();
    const float gr  = grDb.load();

    // Map output dB to needle degrees roughly matching HTML: -42 .. +56
    const float clamped = juce::jlimit (-30.0f, 8.0f, out);
    const float t = (clamped + 30.0f) / 38.0f;
    const float needle = juce::jmap (t, -42.0f, 56.0f);
    vu.setNeedleDegrees (needle);

    auto dbTo01 = [] (float db, float minDb, float maxDb)
    {
        db = juce::jlimit (minDb, maxDb, db);
        return (db - minDb) / (maxDb - minDb);
    };

    barL.setLevel01 (dbTo01 (in,  -60.0f, 0.0f));
    barR.setLevel01 (dbTo01 (out, -60.0f, 0.0f));
    barGR.setLevel01 (juce::jlimit (0.0f, 1.0f, (-gr) / 20.0f));

    fGRTrack.setAmount01 (juce::jlimit (0.0f, 1.0f, std::abs (gr) / 20.0f));

    updateStatusChip();
}

void OptoVoxAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);

    if (parameterID == OptoVoxParams::mode || parameterID == OptoVoxParams::ratio)
        juce::MessageManager::callAsync ([this] { syncFooterFromParams(); });

    if (parameterID == OptoVoxParams::bypass)
        juce::MessageManager::callAsync ([this] { updateStatusChip(); });
}

//==============================================================================
void OptoVoxAudioProcessorEditor::updateReadouts()
{
    const auto peak = (int) std::round (peakSlider.getValue());
    const auto gain = gainSlider.getValue();
    const auto hf   = (int) std::round (hfSlider.getValue());
    const auto bias = (int) std::round (biasSlider.getValue());

    peakReadout.setValueText (juce::String (peak), "%");
    gainReadout.setValueText (juce::String (gain, 1), "dB");
    hfReadout.setValueText   (juce::String (hf), "%");
    biasReadout.setValueText (juce::String (bias), "%");

    const float gr = grDb.load();
    // grDb is negative or 0 in processor; format as −X.X dB
    const float grAbs = std::abs (gr);
    const auto grText = juce::String (juce::CharPointer_UTF8 ("\xE2\x88\x92")) + juce::String (grAbs, 1) + " dB";
    grBig.setText (grText, juce::dontSendNotification);
    fGRVal.setText (grText, juce::dontSendNotification);
}

void OptoVoxAudioProcessorEditor::updateStatusChip()
{
    const bool bypassed = audioProcessor.apvts.getRawParameterValue (OptoVoxParams::bypass)->load() > 0.5f;
    statusLed.setOn (true, bypassed);
    statusText.setText (bypassed ? "BYPASS" : "ACTIVE", juce::dontSendNotification);
    statusText.setColour (juce::Label::textColourId, bypassed ? Theme::red() : Theme::green());

    bypassButton.setColour (juce::TextButton::textColourOffId, bypassed ? Theme::red() : Theme::dim());
}

static void setParamToDefault (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float defaultValuePlain)
{
    if (auto* p = apvts.getParameter (id))
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp != nullptr)
            p->setValueNotifyingHost (rp->convertTo0to1 (defaultValuePlain));
    }
}

static void setChoiceToIndex (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int idx)
{
    if (auto* p = apvts.getParameter (id))
    {
        auto* cp = dynamic_cast<juce::AudioParameterChoice*> (p);
        if (cp != nullptr)
            p->setValueNotifyingHost (cp->convertTo0to1 ((float) idx));
    }
}

void OptoVoxAudioProcessorEditor::resetAllParams()
{
    setParamToDefault (audioProcessor.apvts, OptoVoxParams::peak, 40.0f);
    setParamToDefault (audioProcessor.apvts, OptoVoxParams::gain, 20.0f);
    setParamToDefault (audioProcessor.apvts, OptoVoxParams::hf,   50.0f);
    setParamToDefault (audioProcessor.apvts, OptoVoxParams::bias, 35.0f);

    setChoiceToIndex (audioProcessor.apvts, OptoVoxParams::mode, 0);      // COMP
    setChoiceToIndex (audioProcessor.apvts, OptoVoxParams::ratio, 1);     // 4:1
    setChoiceToIndex (audioProcessor.apvts, OptoVoxParams::attack, 0);    // AUTO
    setChoiceToIndex (audioProcessor.apvts, OptoVoxParams::release, 0);   // AUTO
    setChoiceToIndex (audioProcessor.apvts, OptoVoxParams::sidechain, 0); // INT
    setChoiceToIndex (audioProcessor.apvts, OptoVoxParams::outmode, 0);   // ST

    setParamToDefault (audioProcessor.apvts, OptoVoxParams::bypass, 0.0f);
}

void OptoVoxAudioProcessorEditor::buildChoiceAttachments()
{
    auto makeChoiceAttach = [this] (const juce::String& id, Segmented& seg)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter (id)))
        {
            seg.onSelect = [this, p, id] (int idx)
            {
                auto* base = audioProcessor.apvts.getParameter (id);
                base->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
            };

            auto attach = std::make_unique<juce::ParameterAttachment> (*p,
                [this, &seg] (float newValue)
                {
                    const int idx = (int) std::round (newValue);
                    juce::MessageManager::callAsync ([&seg, idx] { seg.setSelectedIndex (idx, false); });
                },
                nullptr);

            attach->sendInitialUpdate();
            return attach;
        }
        return std::unique_ptr<juce::ParameterAttachment>();
    };

    modeAttach      = makeChoiceAttach (OptoVoxParams::mode, modeSeg);
    ratioAttach     = makeChoiceAttach (OptoVoxParams::ratio, ratioSeg);
    attackAttach    = makeChoiceAttach (OptoVoxParams::attack, attackSeg);
    releaseAttach   = makeChoiceAttach (OptoVoxParams::release, relSeg);
    sidechainAttach = makeChoiceAttach (OptoVoxParams::sidechain, sideSeg);
    outmodeAttach   = makeChoiceAttach (OptoVoxParams::outmode, outSeg);
}

void OptoVoxAudioProcessorEditor::syncFooterFromParams()
{
    auto* modeP = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter (OptoVoxParams::mode));
    auto* ratioP = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter (OptoVoxParams::ratio));

    if (modeP)
    {
        const int idx = modeP->getIndex();
        fModeVal.setText (idx == 0 ? "Comp" : "Limit", juce::dontSendNotification);
    }

    if (ratioP)
    {
        const int idx = ratioP->getIndex();
        if (idx == 0) fRatioVal.setText ("2 : 1", juce::dontSendNotification);
        else if (idx == 1) fRatioVal.setText ("4 : 1", juce::dontSendNotification);
        else fRatioVal.setText ("∞ : 1", juce::dontSendNotification);
    }

    // GR track amount is pushed from timerCallback
}
