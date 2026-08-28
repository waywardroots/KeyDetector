#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Shared dark "mixing console" palette.
namespace console
{
    constexpr juce::uint32 bgTop     = 0xff2b3038; // panel gradient (top)
    constexpr juce::uint32 bgBottomC = 0xff14171c; // panel gradient (bottom)
    constexpr juce::uint32 barTop    = 0xff1b1f25; // title bar
    constexpr juce::uint32 barBottom = 0xff101318;
    constexpr juce::uint32 panel     = 0xff1b1f26; // section panel
    constexpr juce::uint32 screen    = 0xff0a0c10; // recessed display background
    constexpr juce::uint32 edge      = 0xff363c46; // borders / bevel highlight
    constexpr juce::uint32 shadow    = 0xff05070a;
    constexpr juce::uint32 button     = 0xff2a3038;
    constexpr juce::uint32 knobTop    = 0xff3a414b;
    constexpr juce::uint32 knobBottom = 0xff1b1f26;
    constexpr juce::uint32 text       = 0xffc4cad2;
    constexpr juce::uint32 textDim    = 0xff727b86;
    constexpr juce::uint32 accent     = 0xffe6a23c; // amber controls
    constexpr juce::uint32 meter      = 0xff35d0a5; // green meter/trace
}

//==============================================================================
class ConsoleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ConsoleLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (console::bgBottomC));
        setColour (juce::Label::textColourId,                 juce::Colour (console::text));
        setColour (juce::Slider::textBoxTextColourId,         juce::Colour (console::text));
        setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colour (console::screen));
        setColour (juce::ComboBox::textColourId,              juce::Colour (console::text));
        setColour (juce::ComboBox::backgroundColourId,        juce::Colour (console::screen));
        setColour (juce::ComboBox::outlineColourId,           juce::Colour (console::edge));
        setColour (juce::ComboBox::arrowColourId,             juce::Colour (console::accent));
        setColour (juce::TextButton::textColourOnId,          juce::Colours::black);
        setColour (juce::TextButton::textColourOffId,         juce::Colour (console::text));
        setColour (juce::PopupMenu::backgroundColourId,       juce::Colour (console::panel));
        setColour (juce::PopupMenu::textColourId,             juce::Colour (console::text));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (console::accent).withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId,  juce::Colours::white);
    }

    //==============================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float pos, float startAngle, float endAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto  centre = bounds.getCentre();
        const float angle  = startAngle + pos * (endAngle - startAngle);
        const float trackW = 3.0f;
        const float arcR   = radius - trackW * 0.5f - 1.0f;

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (console::shadow));
        g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (juce::Colour (console::accent));
        g.strokePath (val, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float kr = arcR - trackW - 2.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (console::knobTop),    centre.x, centre.y - kr,
                                                 juce::Colour (console::knobBottom), centre.x, centre.y + kr, false));
        g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (juce::Colour (console::edge));
        g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

        juce::Path pointer;
        const float pw = 2.6f;
        pointer.addRoundedRectangle (-pw * 0.5f, -kr, pw, kr * 0.55f, 1.0f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
        g.setColour (juce::Colour (console::accent));
        g.fillPath (pointer);

        g.setColour (juce::Colour (0xff10131a));
        g.fillEllipse (centre.x - 2.5f, centre.y - 2.5f, 5.0f, 5.0f);
    }

    //==============================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        juce::Colour base = on ? juce::Colour (console::accent) : juce::Colour (console::button);
        juce::Colour top = base.brighter (down ? 0.0f : (over ? 0.18f : 0.10f));
        juce::Colour bot = base.darker (down ? 0.05f : 0.22f);

        g.setGradientFill (juce::ColourGradient (top, 0, r.getY(), bot, 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawLine (r.getX() + 3.0f, r.getY() + 1.0f, r.getRight() - 3.0f, r.getY() + 1.0f, 1.0f);

        g.setColour (on ? juce::Colour (console::accent).darker (0.5f) : juce::Colour (console::edge));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    {
        return juce::Font (juce::FontOptions ((float) juce::jmin (15, h - 6), juce::Font::bold));
    }

    //==============================================================================
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto bounds = b.getLocalBounds();
        const float d = 13.0f;
        const bool  hasText = b.getButtonText().isNotEmpty();
        auto led = hasText
            ? juce::Rectangle<float> (bounds.getX() + 1.0f, bounds.getCentreY() - d * 0.5f, d, d)
            : juce::Rectangle<float> (bounds.getCentreX() - d * 0.5f, bounds.getCentreY() - d * 0.5f, d, d);
        const bool on = b.getToggleState();

        if (on)
        {
            g.setColour (juce::Colour (console::accent).withAlpha (0.30f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (juce::Colour (console::shadow));
        g.fillEllipse (led.expanded (1.0f));
        g.setColour (on ? juce::Colour (console::accent) : juce::Colour (0xff3a4048));
        g.fillEllipse (led.reduced (2.0f));
        g.setColour (juce::Colour (console::edge));
        g.drawEllipse (led, 1.0f);

        if (hasText)
        {
            g.setColour (juce::Colour (console::text));
            g.setFont (juce::Font (juce::FontOptions (13.0f)));
            g.drawText (b.getButtonText(), bounds.getX() + (int) d + 8, bounds.getY(),
                        bounds.getWidth() - (int) d - 8, bounds.getHeight(),
                        juce::Justification::centredLeft, false);
        }
    }

    //==============================================================================
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
        g.setColour (juce::Colour (console::screen));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour (console::edge));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        auto arrow = juce::Rectangle<float> ((float) width - 16.0f, 0.0f, 14.0f, (float) height);
        const auto c = arrow.getCentre();
        juce::Path p;
        p.startNewSubPath (c.x - 4.0f, c.y - 2.0f);
        p.lineTo (c.x, c.y + 3.0f);
        p.lineTo (c.x + 4.0f, c.y - 2.0f);
        g.setColour (juce::Colour (console::accent));
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }
};
