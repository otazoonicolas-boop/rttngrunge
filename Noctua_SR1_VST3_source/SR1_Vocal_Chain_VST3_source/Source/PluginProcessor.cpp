#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Generated/dsp_wasm.h"
#include "Generated/param_table.h"
#include <cmath>

using namespace juce;

static String formatValue (const ParamInfo& p, float v)
{
    String u (p.unit);
    if (u == "hz")    return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz";
    if (u == "db")    return String (v, 1) + " dB";
    if (u == "ms")    return v >= 100.0f ? String (roundToInt (v)) + " ms" : String (v, 1) + " ms";
    if (u == "s")     return String (v, 2) + " s";
    if (u == "ratio") return String (v, 1) + ":1";
    if (u == "%")     return String (roundToInt (v)) + " %";
    return String (v, 1);
}

AudioProcessorValueTreeState::ParameterLayout SR1Processor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamInfo& p = kParams[i];
        const ParameterID pid (p.id, 1);

        if (p.kind == ParamInfo::Bool)
        {
            layout.add (std::make_unique<AudioParameterBool> (pid, p.label, p.defV >= 0.5f));
        }
        else if (p.kind == ParamInfo::Choice)
        {
            layout.add (std::make_unique<AudioParameterChoice> (pid, p.label,
                        StringArray::fromTokens (String (p.choices), "|", ""), (int) p.defV));
        }
        else
        {
            const String curve (p.curve);
            NormalisableRange<float> range (p.minV, p.maxV);

            if (curve == "exponential")
                range = NormalisableRange<float> (p.minV, p.maxV,
                    [] (float s, float e, float n) { return s * std::pow (e / s, n); },
                    [] (float s, float e, float v) { return std::log (jmax (v, s) / s) / std::log (e / s); });
            else if (curve == "squared")
                range = NormalisableRange<float> (p.minV, p.maxV,
                    [] (float s, float e, float n) { return s + (e - s) * n * n; },
                    [] (float s, float e, float v) { return std::sqrt (jlimit (0.0f, 1.0f, (v - s) / (e - s))); });

            const ParamInfo info = p;
            layout.add (std::make_unique<AudioParameterFloat> (pid, p.label, range, p.defV,
                AudioParameterFloatAttributes().withStringFromValueFunction ([info] (float v, int) { return formatValue (info, v); })));
        }
    }
    return layout;
}

SR1Processor::SR1Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
        raw[i] = apvts.getRawParameterValue (kParams[i].id);
}

bool SR1Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::stereo() || out == AudioChannelSet::mono()) && l.getMainInputChannelSet() == out;
}

void SR1Processor::prepareToPlay (double sr, int)
{
    if (! engine.isReady() || sr != currentSR)
    {
        engine.load (kDspWasm, kDspWasmSize, (float) sr, kNumParams);
        currentSR = sr;
    }
    else
        engine.reset();

    scratch.setSize (2, SR1Engine::kMaxBlock, false, false, true);
}

void SR1Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels();
    const int total = buffer.getNumSamples();
    if (numCh == 0 || total == 0) return;

    for (int i = 0; i < kNumParams; ++i)
        engine.setParam (kParams[i].wasmId, raw[i]->load());

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int pos = 0; pos < total; pos += SR1Engine::kMaxBlock)
    {
        const int n = jmin (SR1Engine::kMaxBlock, total - pos);
        if (R != nullptr)
            engine.process (L + pos, R + pos, L + pos, R + pos, n);
        else
        {
            float* tmpR = scratch.getWritePointer (0);
            float* tmpO = scratch.getWritePointer (1);
            FloatVectorOperations::copy (tmpR, L + pos, n);
            engine.process (L + pos, tmpR, L + pos, tmpO, n);   // mono: se descarta el canal R
        }
    }
}

AudioProcessorEditor* SR1Processor::createEditor() { return new SR1Editor (*this); }

void SR1Processor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SR1Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SR1Processor(); }
