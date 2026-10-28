#include "MasterEditor.h"

//==============================================================================
// MASTER COMP: compresor de bus estereo con rodilla suave, sidechain HPF,
// mezcla paralela y makeup (manual o automatico)
//==============================================================================
class MasterCompProcessor : public MasterProcessorBase
{
public:
    MasterCompProcessor()
        : MasterProcessorBase ("Master Comp", juce::Colour (0xff4fb3bf), createLayout())
    {
    }

    void prepareToPlay (double sampleRate, int) override
    {
        comp.prepare (sampleRate);
        grMax.store (0.0f);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        measureIn (buffer);

        const float thr   = param ("thr");
        const float ratio = param ("ratio");
        const float makeup = paramBool ("auto") ? juce::jmax (0.0f, -thr * (1.0f - 1.0f / ratio) * 0.5f)
                                                : param ("makeup");

        comp.setParams (thr, ratio, param ("attack"), param ("release"), param ("knee"),
                        makeup, param ("mix") * 0.01f, param ("scHpf"));

        const int n = buffer.getNumSamples();
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);
        float worst = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float g = comp.process (l[i], r[i]);
            worst = juce::jmax (worst, -g);
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
        m.value = juce::jlimit (0.0f, 1.0f, gr / 20.0f);
        m.text  = "-" + juce::String (gr, 1) + " dB";
        out.push_back (m);
    }

private:
    static Layout createLayout()
    {
        Layout l;
        l.add (makeFloat ("thr",     "Threshold", -40.0f,   0.0f, -12.0f, " dB", 1));
        l.add (makeFloat ("ratio",   "Ratio",       1.0f,  20.0f,   2.0f, ":1",  1, 0.5f));
        l.add (makeFloat ("attack",  "Attack",      0.1f, 100.0f,  10.0f, " ms", 1, 0.4f));
        l.add (makeFloat ("release", "Release",    10.0f,1000.0f, 120.0f, " ms", 0, 0.4f));
        l.add (makeFloat ("knee",    "Knee",        0.0f,  12.0f,   6.0f, " dB", 1));
        l.add (makeFloat ("makeup",  "Makeup",      0.0f,  24.0f,   0.0f, " dB", 1));
        l.add (makeFloat ("mix",     "Mix",         0.0f, 100.0f, 100.0f, " %",  0));
        l.add (makeFloat ("scHpf",   "SC HPF",     20.0f, 400.0f,  20.0f, " Hz", 0, 0.4f));
        l.add (makeBool  ("auto",    "Auto Makeup", false));
        return l;
    }

    MasterDsp::Compressor comp;
    mutable std::atomic<float> grMax { 0.0f };
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasterCompProcessor(); }
