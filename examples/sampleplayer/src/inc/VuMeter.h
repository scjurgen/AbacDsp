#pragma once
#include <juce_audio_processors/juce_audio_processors.h>


class MeterBackground : public juce::Component
{
  public:
    MeterBackground()
    {
        sAngle = 0.0f;
        eAngle = 0.0f;
        minValue = 0;
        maxValue = 0;
        step = 20;
        mode = 3;
        backgroundLightGrey = juce::Colour(Constants::Colors::bg_LightGrey);
        backgroundApp = juce::Colour(Constants::Colors::bg_App);
        setBufferedToImage(true);
    }

    void prepare(const float& s, const float& e)
    {
        sAngle = s;
        eAngle = e;
    }

    void paint(juce::Graphics& g)
    {
        const auto bounds = meterArea.toFloat();
        const float centreX = bounds.getX() + bounds.getWidth() * 0.5f;
        const float centreY = bounds.getY() + bounds.getHeight();
        const float needleLength = juce::jmin(bounds.getWidth() * 0.7f, bounds.getHeight() * 0.7f);

        g.setColour(backgroundApp);
        g.fillRoundedRectangle(meterArea.toFloat(), 1);

        g.setColour(backgroundLightGrey);
        drawIndicators(g, centreX, centreY, needleLength);
    }

    void resized()
    {
        meterArea = getLocalBounds().reduced(3);
        repaint();
    }

    void drawIndicators(juce::Graphics& g, float centreX, float centreY, float length) const
    {
        const auto indices = (abs(maxValue - minValue) / step) + 1;
        // DBG("Indices: " << indices);
        int val = minValue;

        for (int i = 0; i < indices; ++i)
        {
            float mapped = juce::jmap(static_cast<float>(val), static_cast<float>(minValue),
                                      static_cast<float>(maxValue), sAngle, eAngle);
            mapped -= mapped > 2 * juce::MathConstants<float>::pi ? juce::MathConstants<float>::twoPi : 0.0f;
            // calc path head
            const float x2 = centreX + sin(mapped) * length;
            const float y2 = centreY - cos(mapped) * length;
            const float rX = centreX - x2;
            const float rY = centreY - y2;
            const float rLength = sqrt(juce::square(rX) + juce::square(rY));
            const float nX = rX / rLength;
            const float nY = rY / rLength;
            const float xCof = nX * 7;
            const float yCof = nY * 7;

            g.drawArrow({x2, y2, x2 - xCof, y2 - yCof}, 2.0f, 0, 0);
            // g.drawSingleLineText(String(val), x2-5, y2);
            val += step;
        }
    }

    void setMode(int m)
    {
        minValue = m == 3 ? -30 : -50;
        mode = m;
        // DBG("MeterBG setting mode: " << m << " | minValue: " << minValue);
        repaint();
    }


  private:
    juce::Rectangle<int> meterArea;
    juce::Colour backgroundLightGrey, backgroundApp;
    int mode;
    int minValue, maxValue;
    float sAngle, eAngle;
    int step;
};


class MeterNeedle : public juce::Component
{
  public:
    MeterNeedle()
    {
        valueInDecibel = 0.0f;
        mode = 3;
        minValue = 0;
        maxValue = 0;
        sAngle = 0.0f;
        eAngle = 0.0f;
        statusOutline = juce::Colour(Constants::Colors::statusOutline);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = area.toFloat();
        const float centreX = bounds.getX() + bounds.getWidth() * 0.5f;
        const float centreY = bounds.getY() + bounds.getHeight();
        const float needleLength = juce::jmin(bounds.getWidth() * 0.75f, bounds.getHeight() * 0.75f);

        g.setColour(statusOutline);
        redrawNeedle(g, centreX, centreY, needleLength);
    }

    void resized() override
    {
        area = getLocalBounds().reduced(3);
        repaint();
    }

    void update(const float& val)
    {
        if (std::abs(val - valueInDecibel) < 1E-7f)
        {
            valueInDecibel = val;
            repaint();
        }
    }

    void redrawNeedle(juce::Graphics& g, float centreX, float centreY, float length) const
    {
        // DBG("min: " << minValue << " - max: " << maxValue << " | startAngle: " << sAngle << " - endAngle: " <<
        // eAngle);
        float val = std::clamp(valueInDecibel, static_cast<float>(minValue), static_cast<float>(maxValue));
        float mapped = juce::jmap(val, static_cast<float>(minValue), static_cast<float>(maxValue), sAngle, eAngle);
        mapped -= mapped > 2 * juce::MathConstants<float>::pi ? juce::MathConstants<float>::twoPi : 0.0f;
        const float x2 = centreX + sin(mapped) * length;
        const float y2 = centreY - cos(mapped) * length;
        // DBG("Drawing from> " << centreX << "|" << centreY << " to " << x2 << "|" << y2 << " || Mapped: " << mapped <<
        // " in Deg: " << radiansToDegrees(mapped));
        g.drawArrow({centreX, centreY, x2, y2}, 2.0f, 0, 0);
    }

    void prepare(const float& s, const float& e)
    {
        sAngle = s;
        eAngle = e;
    }

    void setMode(int m)
    {
        minValue = m == 3 ? -30 : -50;
        mode = m;
        repaint();
        // DBG("MeterNeedle setting mode: " << m << " | minValue: " << minValue);
    }

  private:
    juce::Rectangle<int> area;
    juce::Colour statusOutline;
    float valueInDecibel;
    int mode;
    int minValue, maxValue;
    float sAngle, eAngle;
};


class Meter : public juce::Component
{
  public:
    enum Mode
    {
        IN = 1,
        OUT,
        GR
    };

    Meter()
        : meterMode(GR)
        , valueInDecibel(-50)
    {
        startAngle = static_cast<float>(5.0f / 3.0f * juce::MathConstants<float>::pi);
        endAngle = static_cast<float>(7.0f / 3.0f * juce::MathConstants<float>::pi);
        meterBg.prepare(startAngle, endAngle);
        meterBg.setMode(GR);
        needle.prepare(startAngle, endAngle);
        needle.setMode(GR);
        addAndMakeVisible(modeBox);
        modeBox.addItem("RMS", 1);
        modeBox.addItem("Peak", 2);
        modeBox.addItem("Peak & RMS", 3);
        modeBox.setJustificationType(juce::Justification::centred);
        modeBox.setSelectedId(3);
        modeBox.onChange = [this] { modeBoxChanged(); };
        addAndMakeVisible(meterBg);
        addAndMakeVisible(needle);
        addAndMakeVisible(modeBox);

        backgroundDarkGrey = juce::Colour(Constants::Colors::bg_DarkGrey);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(backgroundDarkGrey);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto boxHeight = (bounds.getHeight() / 20) * 2;
        auto boxArea = bounds.removeFromBottom(boxHeight);
        const juce::FlexItem::Margin comboMargin =
            juce::FlexItem::Margin(6.0f, bounds.getWidth() / 2 - 80, 6.0f, bounds.getWidth() / 2 - 80);

        juce::FlexBox mBox;
        mBox.flexWrap = juce::FlexBox::Wrap::noWrap;
        mBox.flexDirection = juce::FlexBox::Direction::row;
        mBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        mBox.items.add(juce::FlexItem(modeBox).withFlex(1).withMargin(comboMargin));
        mBox.performLayout(boxArea.toFloat());

        meterBg.setBounds(bounds);
        needle.setBounds(bounds);
    }

    void setMode(int m)
    {
        meterBg.setMode(m);
        needle.setMode(m);
        meterMode = m;
    }

    void modeBoxChanged()
    {
        const auto selected = modeBox.getSelectedId();
        setMode(selected);
    }

    void update(const float& val)
    {
        if (std::abs(val - valueInDecibel) < 1E-7f)
        {
            needle.update(val);
        }
    }

    int getMode() const
    {
        return meterMode;
    }

    float getValue() const
    {
        return valueInDecibel;
    }


  private:
    MeterBackground meterBg;
    MeterNeedle needle;
    juce::ComboBox modeBox;
    juce::Colour backgroundDarkGrey;
    int meterMode;
    float valueInDecibel;
    float startAngle, endAngle;
};
