#include "MasterEditor.h"

//==============================================================================
// MASTER IMAGER: ancho estereo (mid/side), ganancia de mid y side, graves en
// mono por debajo de una frecuencia y monitor mono.  Medidor de correlacion.
//==============================================================================
class MasterImagerProcessor : public MasterProcessorBase
{
public:
    MasterImagerProcessor()
        : MasterProcessorBase ("Master Imager", juce::Colour (0xff8a94b8), createLayout())
    {
    }

    void prepareToPlay (double sampleRate, int) override
    {
        sr = sampleRate;
        sideHp.reset();
        lastMono = -1.0f;
        corr.store (0.0f);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;

        if (buffer.getNumChannels() < 2)
            return;

        measureIn (buffer);

        const float width   = param ("width") * 0.01f;
        const float midG    = MasterDsp::dbToLin (param ("mid"));
        const float sideG   = MasterDsp::dbToLin (param ("side"));
        const float outG    = MasterDsp::dbToLin (param ("out"));
        const float monoHz  = param ("monoBelow");
        const bool  monoAll = paramBool ("mono");
        const bool  hpOn    = monoHz > 20.5f;

        if (hpOn && monoHz != lastMono)
        {
            sideHp.makeHighPass (sr, monoHz);
            lastMono = monoHz;
        }

        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        double sLR = 0.0, sLL = 0.0, sRR = 0.0;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float m = (l[i] + r[i]) * 0.5f;
            float s = (l[i] - r[i]) * 0.5f;

            if (hpOn)
                s = sideHp.process (s);

            s = monoAll ? 0.0f : s * width * sideG;

            l[i] = (m * midG + s) * outG;
            r[i] = (m * midG - s) * outG;

            sLR += (double) l[i] * r[i];
            sLL += (double) l[i] * l[i];
            sRR += (double) r[i] * r[i];
        }

        const double denom = std::sqrt (sLL * sRR);
        corr.store (denom > 1.0e-12 ? (float) (sLR / denom) : 0.0f);
        measureOut (buffer);
    }

protected:
    void addExtraMeters (std::vector<MeterReading>& out) const override
    {
        const float c = corr.load();
        MeterReading m;
        m.label   = "CORR";
        m.centred = true;
        m.value   = juce::jlimit (0.0f, 1.0f, (c + 1.0f) * 0.5f);
        m.text    = juce::String (c, 2);
        out.push_back (m);
    }

private:
    static Layout createLayout()
    {
        Layout l;
        l.add (makeFloat ("width",     "Width",      0.0f, 200.0f, 100.0f, " %",  0));
        l.add (makeFloat ("mid",       "Mid Gain",  -6.0f,   6.0f,   0.0f, " dB", 1));
        l.add (makeFloat ("side",      "Side Gain", -6.0f,   6.0f,   0.0f, " dB", 1));
        l.add (makeFloat ("monoBelow", "Mono Below", 20.0f, 300.0f,  20.0f, " Hz", 0, 0.5f));
        l.add (makeFloat ("out",       "Output",    -6.0f,   6.0f,   0.0f, " dB", 1));
        l.add (makeBool  ("mono",      "Mono Check", false));
        return l;
    }

    double sr = 44100.0;
    float lastMono = -1.0f;
    MasterDsp::Biquad sideHp;
    std::atomic<float> corr { 0.0f };
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasterImagerProcessor(); }
