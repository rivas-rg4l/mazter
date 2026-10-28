#include "MasterEditor.h"

//==============================================================================
// MASTER EQ: HPF + low shelf + 2 campanas + high shelf + salida
//==============================================================================
class MasterEQProcessor : public MasterProcessorBase
{
public:
    MasterEQProcessor()
        : MasterProcessorBase ("Master EQ", juce::Colour (0xff5b8fc9), createLayout())
    {
    }

    void prepareToPlay (double sampleRate, int) override
    {
        sr = sampleRate;

        for (auto& ch : bands)
            for (auto& b : ch)
                b.reset();

        for (auto& v : last)
            v = -1.0e9f;

        updateCoefficients();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        measureIn (buffer);
        updateCoefficients();

        const float outGain = MasterDsp::dbToLin (param ("out"));
        const bool hpfOn = param ("hpf") > 20.5f;
        const int numCh = juce::jmin (2, buffer.getNumChannels());

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto& f = bands[(size_t) ch];

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float x = data[i];

                if (hpfOn)
                    x = f[0].process (x);

                x = f[1].process (x);
                x = f[2].process (x);
                x = f[3].process (x);
                x = f[4].process (x);
                data[i] = x * outGain;
            }
        }

        measureOut (buffer);
    }

private:
    enum { hpf, ls, b1, b2, hs, numBands };

    static Layout createLayout()
    {
        Layout l;
        l.add (makeFloat ("hpf",    "HPF",          20.0f,   400.0f,   20.0f, " Hz", 0, 0.35f));
        l.add (makeFloat ("lsFreq", "Low Shelf",    30.0f,   500.0f,  100.0f, " Hz", 0, 0.5f));
        l.add (makeFloat ("lsGain", "LS Gain",     -12.0f,    12.0f,    0.0f, " dB", 1));
        l.add (makeFloat ("b1Freq", "Low-Mid",     100.0f,  3000.0f,  400.0f, " Hz", 0, 0.4f));
        l.add (makeFloat ("b1Gain", "LM Gain",     -12.0f,    12.0f,    0.0f, " dB", 1));
        l.add (makeFloat ("b1Q",    "LM Q",          0.3f,     4.0f,    1.0f, "",    2));
        l.add (makeFloat ("b2Freq", "High-Mid",    500.0f,  8000.0f, 2500.0f, " Hz", 0, 0.4f));
        l.add (makeFloat ("b2Gain", "HM Gain",     -12.0f,    12.0f,    0.0f, " dB", 1));
        l.add (makeFloat ("b2Q",    "HM Q",          0.3f,     4.0f,    1.0f, "",    2));
        l.add (makeFloat ("hsFreq", "High Shelf", 2000.0f, 16000.0f, 8000.0f, " Hz", 0, 0.5f));
        l.add (makeFloat ("hsGain", "HS Gain",     -12.0f,    12.0f,    0.0f, " dB", 1));
        l.add (makeFloat ("out",    "Output",      -12.0f,    12.0f,    0.0f, " dB", 1));
        return l;
    }

    // Recalcula solo lo que cambio desde el bloque anterior
    void updateCoefficients()
    {
        const float v[] = { param ("hpf"), param ("lsFreq"), param ("lsGain"),
                            param ("b1Freq"), param ("b1Gain"), param ("b1Q"),
                            param ("b2Freq"), param ("b2Gain"), param ("b2Q"),
                            param ("hsFreq"), param ("hsGain") };

        bool changed = false;

        for (size_t i = 0; i < last.size(); ++i)
        {
            if (v[i] != last[i])
            {
                last[i] = v[i];
                changed = true;
            }
        }

        if (! changed)
            return;

        for (auto& ch : bands)
        {
            ch[hpf].makeHighPass (sr, v[0]);
            ch[ls].makeLowShelf  (sr, v[1], 0.7071, v[2]);
            ch[b1].makePeak      (sr, v[3], v[5], v[4]);
            ch[b2].makePeak      (sr, v[6], v[8], v[7]);
            ch[hs].makeHighShelf (sr, v[9], 0.7071, v[10]);
        }
    }

    double sr = 44100.0;
    std::array<std::array<MasterDsp::Biquad, numBands>, 2> bands;
    std::array<float, 11> last {};
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasterEQProcessor(); }
