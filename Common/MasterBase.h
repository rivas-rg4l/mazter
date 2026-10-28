#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Dsp.h"

//==============================================================================
// Lectura de un medidor (la dibuja el editor)
//==============================================================================
struct MeterReading
{
    juce::String label;
    juce::String text;
    float value   = 0.0f;     // 0..1
    bool  centred = false;    // true = la barra parte del centro (ej. correlacion)
};

//==============================================================================
// Base comun de los plugins del pack: buses estereo, estado, parametros y
// medidores de entrada/salida.  Cada plugin solo define sus parametros y su DSP.
//==============================================================================
class MasterProcessorBase : public juce::AudioProcessor
{
public:
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    MasterProcessorBase (const juce::String& nameIn, juce::Colour accentIn, Layout layout)
        : juce::AudioProcessor (BusesProperties()
                                    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "STATE", std::move (layout)),
          pluginName (nameIn),
          accent (accentIn)
    {
    }

    //--------------------------------------------------------------------------
    const juce::String& getPluginName() const noexcept { return pluginName; }
    juce::Colour getAccent() const noexcept            { return accent; }

    const juce::String getName() const override        { return pluginName; }
    bool acceptsMidi() const override                  { return false; }
    bool producesMidi() const override                 { return false; }
    bool isMidiEffect() const override                 { return false; }
    double getTailLengthSeconds() const override       { return 0.0; }

    int getNumPrograms() override                      { return 1; }
    int getCurrentProgram() override                   { return 0; }
    void setCurrentProgram (int) override              {}
    const juce::String getProgramName (int) override   { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void releaseResources() override                   {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    bool hasEditor() const override                    { return true; }
    juce::AudioProcessorEditor* createEditor() override;      // se define en MasterEditor.h

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    //--------------------------------------------------------------------------
    // Medidores: IN y OUT automaticos + los extra de cada plugin
    //--------------------------------------------------------------------------
    void getMeters (std::vector<MeterReading>& out) const
    {
        out.push_back (peakMeter ("IN",  peakIn.exchange (0.0f)));
        out.push_back (peakMeter ("OUT", peakOut.exchange (0.0f)));
        addExtraMeters (out);
    }

    juce::AudioProcessorValueTreeState apvts;

protected:
    virtual void addExtraMeters (std::vector<MeterReading>&) const {}

    float param (const char* id) const        { return apvts.getRawParameterValue (id)->load(); }
    bool  paramBool (const char* id) const    { return param (id) > 0.5f; }
    int   paramChoice (const char* id) const  { return (int) (param (id) + 0.5f); }

    void measureIn (const juce::AudioBuffer<float>& b) noexcept
    {
        peakIn.store (std::max (peakIn.load(), b.getMagnitude (0, b.getNumSamples())));
    }

    void measureOut (const juce::AudioBuffer<float>& b) noexcept
    {
        peakOut.store (std::max (peakOut.load(), b.getMagnitude (0, b.getNumSamples())));
    }

    //--------------------------------------------------------------------------
    // Ayudas para declarar parametros
    //--------------------------------------------------------------------------
    static std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name,
                                                                 float lo, float hi, float def,
                                                                 const juce::String& unit = {},
                                                                 int decimals = 1, float skew = 1.0f)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (lo, hi, 0.0f, skew), def,
            juce::AudioParameterFloatAttributes()
                .withLabel (unit)
                .withStringFromValueFunction ([decimals] (float v, int) { return juce::String (v, decimals); }));
    }

    static std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id, const juce::String& name, bool def)
    {
        return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
    }

    static std::unique_ptr<juce::AudioParameterChoice> makeChoice (const juce::String& id, const juce::String& name,
                                                                   const juce::StringArray& items, int def)
    {
        return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
    }

private:
    static MeterReading peakMeter (const juce::String& label, float lin)
    {
        MeterReading m;
        m.label = label;
        const float db = MasterDsp::linToDb (lin);
        m.value = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        m.text  = lin < 0.0001f ? juce::String ("-inf") : juce::String (db, 1) + " dB";
        return m;
    }

    const juce::String pluginName;
    const juce::Colour accent;
    mutable std::atomic<float> peakIn { 0.0f }, peakOut { 0.0f };
};
