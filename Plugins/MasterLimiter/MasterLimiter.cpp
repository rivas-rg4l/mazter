#include "MasterEditor.h"

//==============================================================================
// MASTER LIMITER: limitador con look-ahead (1.5 ms), ganancia de entrada,
// techo (ceiling) y release.  Reporta su latencia al host.
//==============================================================================
class MasterLimiterProcessor : public MasterProcessorBase
{
public:
    MasterLimiterProcessor()
        : MasterProcessorBase ("Master Limiter", juce::Colour (0xffd0a054), createLayout())
    {
    }

    void prepareToPlay (double sampleRate, int) override
    {
        limiter.prepare (sampleRate, 1.5f);
        setLatencySamples (limiter.latency());
        grMax.store (0.0f);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        measureIn (buffer);

        limiter.setParams (param ("ceiling"), param ("release"));
        const float inGain = MasterDsp::dbToLin (param ("gain"));

        const int n = buffer.getNumSamples();
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);
        float worst = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float a = l[i] * inGain, b = r[i] * inGain;
            limiter.process (a, b);
            l[i] = a;
            r[i] = b;
            worst = juce::jmax (worst, -limiter.currentGainDb());
        }

        grMax.store (juce::jmax (grMax.load(), worst));
        measureOut (buffer);
    }

protected:
    void addExtraMeters (std::vector<MeterReading>& out) const override
    {
        const float gr = grMax.exchange (0.0f);
        MeterReading m;
        m.label = "GR";
        m.value = juce::jlimit (0.0f, 1.0f, gr / 12.0f);
        m.text  = "-" + juce::String (gr, 1) + " dB";
        out.push_back (m);
    }

private:
    static Layout createLayout()
    {
        Layout l;
        l.add (makeFloat ("gain",    "Input Gain",  0.0f,  24.0f,  0.0f, " dB", 1));
        l.add (makeFloat ("ceiling", "Ceiling",   -12.0f,   0.0f, -0.3f, " dB", 1));
        l.add (makeFloat ("release", "Release",    10.0f,1000.0f, 100.0f, " ms", 0, 0.4f));
        return l;
    }

    MasterDsp::Limiter limiter;
    mutable std::atomic<float> grMax { 0.0f };
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasterLimiterProcessor(); }
