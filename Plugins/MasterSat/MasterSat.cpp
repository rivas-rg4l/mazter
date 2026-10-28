#include "MasterEditor.h"

//==============================================================================
// MASTER SAT: saturacion con 4 curvas (Tape, Tube, Soft, Hard), tono, mezcla
// y salida.  Trabaja con sobremuestreo 4x para reducir aliasing.
//==============================================================================
class MasterSatProcessor : public MasterProcessorBase
{
public:
    MasterSatProcessor()
        : MasterProcessorBase ("Master Sat", juce::Colour (0xff7fae8b), createLayout())
    {
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        sr = sampleRate;
        oversampler.initProcessing ((size_t) juce::jmax (1, samplesPerBlock));
        oversampler.reset();
        setLatencySamples ((int) std::lround (oversampler.getLatencyInSamples()));

        for (auto& d : dc) d = {};
        lp.fill (0.0f);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;

        if (buffer.getNumChannels() < 2)
            return;

        measureIn (buffer);

        const int type     = paramChoice ("type");
        const float drive  = MasterDsp::dbToLin (param ("drive"));
        const float comp   = 1.0f / std::sqrt (drive);                 // compensacion suave de nivel
        const float mix    = param ("mix") * 0.01f;
        const float tone   = param ("tone") * 0.01f;
        const float outG   = MasterDsp::dbToLin (param ("out"));

        // Filtro de tono (un polo, a la frecuencia sobremuestreada)
        const float a = 1.0f - std::exp (-2.0f * (float) MasterDsp::pi * 1200.0f / (float) (sr * 4.0));

        juce::dsp::AudioBlock<float> block (buffer);
        auto up = oversampler.processSamplesUp (block);

        for (size_t ch = 0; ch < 2; ++ch)
        {
            auto* d = up.getChannelPointer (ch);

            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const float dry = d[i];
                float x = dry * drive;

                switch (type)
                {
                    case 0:  x = MasterDsp::shapeTape (x); break;
                    case 1:  x = MasterDsp::shapeTube (x); break;
                    case 2:  x = MasterDsp::shapeSoft (x); break;
                    default: x = MasterDsp::shapeHard (x); break;
                }

                x = dc[ch].process (x * comp);

                // Tono: inclina el balance graves / agudos del sonido saturado
                lp[ch] += a * (x - lp[ch]);
                x = lp[ch] * (1.0f - 0.6f * tone) + (x - lp[ch]) * (1.0f + 0.6f * tone);

                d[i] = dry + (x - dry) * mix;
            }
        }

        oversampler.processSamplesDown (block);
        buffer.applyGain (outG);
        measureOut (buffer);
    }

private:
    static Layout createLayout()
    {
        Layout l;
        l.add (makeChoice ("type",  "Type", juce::StringArray { "Tape", "Tube", "Soft", "Hard" }, 0));
        l.add (makeFloat  ("drive", "Drive",   0.0f,  24.0f,   6.0f, " dB", 1));
        l.add (makeFloat  ("tone",  "Tone", -100.0f, 100.0f,   0.0f, "",    0));
        l.add (makeFloat  ("mix",   "Mix",     0.0f, 100.0f, 100.0f, " %",  0));
        l.add (makeFloat  ("out",   "Output", -12.0f,  12.0f,   0.0f, " dB", 1));
        return l;
    }

    double sr = 44100.0;
    juce::dsp::Oversampling<float> oversampler { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    std::array<MasterDsp::DcBlocker, 2> dc;
    std::array<float, 2> lp { { 0.0f, 0.0f } };
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasterSatProcessor(); }
