#pragma once

#include "MasterBase.h"

//==============================================================================
// Aspecto comun del pack: grafito + un color de acento por plugin
//==============================================================================
class MasterLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit MasterLookAndFeel (juce::Colour accentIn) : accent (accentIn)
    {
        setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffe6e9ef));
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId,               juce::Colour (0xff9aa3b2));
        setColour (juce::ComboBox::backgroundColourId,      juce::Colour (0xff12151a));
        setColour (juce::ComboBox::textColourId,            juce::Colour (0xffe6e9ef));
        setColour (juce::ComboBox::outlineColourId,         accent.withAlpha (0.6f));
        setColour (juce::ComboBox::arrowColourId,           accent);
        setColour (juce::PopupMenu::backgroundColourId,     juce::Colour (0xff1b1f26));
        setColour (juce::PopupMenu::textColourId,           juce::Colour (0xffe6e9ef));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.55f));
        setColour (juce::ToggleButton::textColourId,        juce::Colour (0xffe6e9ef));
        setColour (juce::ToggleButton::tickColourId,        accent);
        setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff4a5160));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (8.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float arcR = radius - 3.0f;

        // Fondo
        g.setColour (juce::Colour (0xff12151a));
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        // Arco de valor
        juce::Path track, value;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);

        g.setColour (juce::Colour (0xff2a303b));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Puntero
        juce::Path pointer;
        const float len = radius * 0.55f;
        pointer.addRoundedRectangle (-1.5f, -arcR + 6.0f, 3.0f, len, 1.5f);
        g.setColour (juce::Colour (0xffe6e9ef));
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

private:
    juce::Colour accent;
};

//==============================================================================
// Editor automatico: crea una perilla / interruptor / menu por cada parametro
// y dibuja los medidores del plugin.
//==============================================================================
class MasterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MasterEditor (MasterProcessorBase& p)
        : juce::AudioProcessorEditor (p), proc (p), laf (p.getAccent())
    {
        setLookAndFeel (&laf);

        for (auto* prm : proc.getParameters())
        {
            if (auto* f = dynamic_cast<juce::AudioParameterFloat*> (prm))        addKnob (*f);
            else if (auto* b = dynamic_cast<juce::AudioParameterBool*> (prm))    addToggle (*b);
            else if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm))  addChoice (*c);
        }

        std::vector<MeterReading> tmp;
        proc.getMeters (tmp);
        numMeters = (int) tmp.size();
        held.assign ((size_t) numMeters, 0.0f);
        readings = tmp;

        const int count = (int) cells.size();
        cols = juce::jlimit (1, 6, count);
        rows = (count + cols - 1) / cols;

        const int w = juce::jmax (560, cols * cellW + 40);
        const int h = headerH + rows * cellH + meterH() + 24;
        setSize (w, h);
        startTimerHz (30);
    }

    ~MasterEditor() override
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        const auto accent = proc.getAccent();
        const float w = (float) getWidth(), h = (float) getHeight();

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff171a20), 0.0f, 0.0f,
                                                 juce::Colour (0xff0f1116), 0.0f, h, false));
        g.fillAll();

        g.setColour (accent);
        g.fillRect (0.0f, 0.0f, w, 2.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        g.drawText (proc.getPluginName().toUpperCase(), 20, 10, getWidth() - 160, 30, juce::Justification::centredLeft, false);

        g.setColour (juce::Colour (0xff8b93a3));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("PERREOTRAP  |  MASTER PACK", getWidth() - 240, 10, 220, 30, juce::Justification::centredRight, false);

        // Panel de medidores
        const int my = headerH + rows * cellH + 6;
        g.setColour (juce::Colour (0xff1d2128));
        g.fillRoundedRectangle (14.0f, (float) my, w - 28.0f, (float) meterH() - 6.0f, 8.0f);

        for (int i = 0; i < (int) readings.size(); ++i)
        {
            const auto& m = readings[(size_t) i];
            const int y = my + 10 + i * 22;
            const int labelW = 40, textW = 80;
            const juce::Rectangle<float> bar ((float) (14 + 12 + labelW), (float) y + 3.0f,
                                              w - 28.0f - 24.0f - (float) labelW - (float) textW, 12.0f);

            g.setColour (juce::Colour (0xff9aa3b2));
            g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
            g.drawText (m.label, 14 + 12, y, labelW, 18, juce::Justification::centredLeft, false);

            g.setColour (juce::Colour (0xff12151a));
            g.fillRoundedRectangle (bar, 3.0f);

            const float v = held[(size_t) i];
            g.setColour (accent);

            if (m.centred)
            {
                const float cx = bar.getCentreX();
                const float x2 = bar.getX() + v * bar.getWidth();
                g.fillRect (juce::Rectangle<float> (juce::jmin (cx, x2), bar.getY(), std::abs (x2 - cx), bar.getHeight()));
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.fillRect (cx - 0.5f, bar.getY(), 1.0f, bar.getHeight());
            }
            else
            {
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * v), 3.0f);
            }

            g.setColour (juce::Colour (0xffe6e9ef));
            g.setFont (juce::Font (juce::FontOptions (11.5f)));
            g.drawText (m.text, (int) bar.getRight() + 8, y, textW, 18, juce::Justification::centredLeft, false);
        }
    }

    void resized() override
    {
        const int gridW = cols * cellW;
        const int x0 = (getWidth() - gridW) / 2;

        for (int i = 0; i < (int) cells.size(); ++i)
        {
            auto& c = cells[(size_t) i];
            juce::Rectangle<int> r (x0 + (i % cols) * cellW, headerH + (i / cols) * cellH, cellW, cellH);
            r = r.reduced (4, 0);

            c.label->setBounds (r.removeFromTop (20));

            if (c.kind == Kind::knob)
                c.comp->setBounds (r);
            else if (c.kind == Kind::toggle)
                c.comp->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 96), 28));
            else
                c.comp->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 104), 26));
        }
    }

private:
    enum class Kind { knob, toggle, choice };

    struct Cell
    {
        Kind kind = Kind::knob;
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::Label> label;
    };

    static constexpr int cellW = 116, cellH = 150, headerH = 52;

    int meterH() const { return numMeters * 22 + 20; }

    void addLabel (Cell& c, const juce::String& text)
    {
        c.label = std::make_unique<juce::Label> ("", text);
        c.label->setJustificationType (juce::Justification::centred);
        c.label->setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        addAndMakeVisible (*c.label);
    }

    void addKnob (juce::AudioParameterFloat& p)
    {
        Cell c;
        c.kind = Kind::knob;
        addLabel (c, p.getName (24).toUpperCase());

        auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 18);

        auto* pp = &p;
        s->textFromValueFunction = [pp] (double v)
        {
            return pp->getText (pp->convertTo0to1 ((float) v), 16) + pp->getLabel();
        };
        s->valueFromTextFunction = [pp] (const juce::String& t)
        {
            return (double) pp->convertFrom0to1 (pp->getValueForText (t));
        };

        sliderAtt.push_back (std::make_unique<juce::SliderParameterAttachment> (p, *s));
        s->setDoubleClickReturnValue (true, (double) p.convertFrom0to1 (p.getDefaultValue()));

        addAndMakeVisible (*s);
        c.comp = std::move (s);
        cells.push_back (std::move (c));
    }

    void addToggle (juce::AudioParameterBool& p)
    {
        Cell c;
        c.kind = Kind::toggle;
        addLabel (c, "");

        auto b = std::make_unique<juce::ToggleButton> (p.getName (24));
        buttonAtt.push_back (std::make_unique<juce::ButtonParameterAttachment> (p, *b));
        addAndMakeVisible (*b);
        c.comp = std::move (b);
        cells.push_back (std::move (c));
    }

    void addChoice (juce::AudioParameterChoice& p)
    {
        Cell c;
        c.kind = Kind::choice;
        addLabel (c, p.getName (24).toUpperCase());

        auto box = std::make_unique<juce::ComboBox>();
        box->addItemList (p.choices, 1);
        comboAtt.push_back (std::make_unique<juce::ComboBoxParameterAttachment> (p, *box));
        addAndMakeVisible (*box);
        c.comp = std::move (box);
        cells.push_back (std::move (c));
    }

    void timerCallback() override
    {
        readings.clear();
        proc.getMeters (readings);

        if (held.size() != readings.size())
            held.assign (readings.size(), 0.0f);

        for (size_t i = 0; i < readings.size(); ++i)
            held[i] = readings[i].centred ? readings[i].value
                                          : juce::jmax (readings[i].value, held[i] - 0.025f);

        repaint (0, headerH + rows * cellH, getWidth(), meterH() + 12);
    }

    MasterProcessorBase& proc;
    MasterLookAndFeel laf;

    std::vector<Cell> cells;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttonAtt;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> comboAtt;

    std::vector<MeterReading> readings;
    std::vector<float> held;
    int numMeters = 0, cols = 1, rows = 1;
};

inline juce::AudioProcessorEditor* MasterProcessorBase::createEditor()
{
    return new MasterEditor (*this);
}
