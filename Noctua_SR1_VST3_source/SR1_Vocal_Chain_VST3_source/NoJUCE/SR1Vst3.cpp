// SR1 Vocal Chain - plugin VST3 construido directamente sobre el SDK de Steinberg (sin JUCE).
// Interfaz: la que provee el DAW (editor generico de parametros, agrupados por "units").
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/vst/utility/stringconvert.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/vsttypes.h"
#include "base/source/fstreamer.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

#include "../Source/SR1Engine.h"
#include "../Source/Generated/dsp_wasm.h"
#include "../Source/Generated/param_table.h"
#include "../Source/Generated/plugin_info.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

static int numChoices (const ParamInfo& p)
{
    int n = 1;
    for (const char* c = p.choices; *c; ++c) if (*c == '|') ++n;
    return n;
}
static std::string choiceName (const ParamInfo& p, int idx)
{
    std::string s (p.choices), cur; int i = 0;
    for (char c : s) { if (c == '|') { if (i++ == idx) return cur; cur.clear(); } else cur += c; }
    return cur;
}

//------------------------------------------------------------------------
class SR1Param : public Parameter
{
public:
    SR1Param (const ParamInfo& pi, ParamID id, UnitID unit) : Parameter(), pi (pi)
    {
        StringConvert::convert (pi.label, info.title);
        StringConvert::convert (pi.label, info.shortTitle);
        StringConvert::convert (pi.unit, info.units);
        info.id = id;
        info.unitId = unit;
        info.flags = ParameterInfo::kCanAutomate;
        if (pi.kind == ParamInfo::Bool)        info.stepCount = 1;
        else if (pi.kind == ParamInfo::Choice) info.stepCount = numChoices (pi) - 1;
        else                                   info.stepCount = 0;
        info.defaultNormalizedValue = toNormalized (pi.defV);
        valueNormalized = info.defaultNormalizedValue;
    }

    ParamValue toPlain (ParamValue n) const override
    {
        n = std::min (1.0, std::max (0.0, n));
        const double a = pi.minV, b = pi.maxV;
        if (info.stepCount > 0) return std::floor (n * info.stepCount + 0.5) + a;
        if (!std::strcmp (pi.curve, "exponential")) return a * std::pow (b / a, n);
        if (!std::strcmp (pi.curve, "squared"))     return a + (b - a) * n * n;
        return a + (b - a) * n;
    }
    ParamValue toNormalized (ParamValue v) const override
    {
        const double a = pi.minV, b = pi.maxV;
        v = std::min (b, std::max (a, v));
        if (info.stepCount > 0) return (std::floor (v - a + 0.5)) / info.stepCount;
        if (!std::strcmp (pi.curve, "exponential")) return std::log (v / a) / std::log (b / a);
        if (!std::strcmp (pi.curve, "squared"))     return std::sqrt ((v - a) / (b - a));
        return (v - a) / (b - a);
    }
    void toString (ParamValue n, String128 out) const override
    {
        const double v = toPlain (n);
        char buf[64];
        if (pi.kind == ParamInfo::Bool)        std::snprintf (buf, sizeof buf, "%s", v >= 0.5 ? "On" : "Off");
        else if (pi.kind == ParamInfo::Choice) std::snprintf (buf, sizeof buf, "%s", choiceName (pi, (int) std::lround (v)).c_str());
        else if (!std::strcmp (pi.unit, "hz"))   { if (v >= 1000) std::snprintf (buf, sizeof buf, "%.2f kHz", v / 1000); else std::snprintf (buf, sizeof buf, "%.0f Hz", v); }
        else if (!std::strcmp (pi.unit, "db"))   std::snprintf (buf, sizeof buf, "%.1f dB", v);
        else if (!std::strcmp (pi.unit, "ms"))   std::snprintf (buf, sizeof buf, v >= 100 ? "%.0f ms" : "%.1f ms", v);
        else if (!std::strcmp (pi.unit, "s"))    std::snprintf (buf, sizeof buf, "%.2f s", v);
        else if (!std::strcmp (pi.unit, "ratio")) std::snprintf (buf, sizeof buf, "%.1f:1", v);
        else if (!std::strcmp (pi.unit, "%"))    std::snprintf (buf, sizeof buf, "%.0f %%", v);
        else std::snprintf (buf, sizeof buf, "%.2f", v);
        StringConvert::convert (std::string (buf), out);
    }
    bool fromString (const TChar* s, ParamValue& n) const override
    {
        std::string str = StringConvert::convert (s);
        if (pi.kind != ParamInfo::Float)
        {
            for (int i = 0; i < (pi.kind == ParamInfo::Bool ? 2 : numChoices (pi)); ++i)
            {
                std::string nm = pi.kind == ParamInfo::Bool ? (i ? "On" : "Off") : choiceName (pi, i);
                if (nm == str) { n = toNormalized (pi.minV + i); return true; }
            }
        }
        double v = std::atof (str.c_str());
        if (str.find ("k") != std::string::npos || str.find ("K") != std::string::npos) v *= 1000.0;
        n = toNormalized (v);
        return true;
    }
private:
    ParamInfo pi;
};

//------------------------------------------------------------------------
class SR1Effect : public SingleComponentEffect
{
public:
    static FUnknown* createInstance (void*) { return (IAudioProcessor*) new SR1Effect(); }

    tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE
    {
        tresult r = SingleComponentEffect::initialize (context);
        if (r != kResultOk) return r;
        params.clear();

        addAudioInput (u"Input", SpeakerArr::kStereo);
        addAudioOutput (u"Output", SpeakerArr::kStereo);

        for (int g = 0; g < kNumGroups; ++g)
        {
            auto name = StringConvert::convert (std::string (kGroupNames[g]));
            addUnit (new Unit (name.c_str(), (UnitID) (g + 1), kRootUnitId));
        }
        for (int i = 0; i < kNumParams; ++i)
        {
            auto* p = new SR1Param (kParams[i], (ParamID) i, (UnitID) (kParams[i].group + 1));
            parameters.addParameter (p);
            params.push_back (p);
        }
        return kResultOk;
    }

    tresult PLUGIN_API terminate() SMTG_OVERRIDE
    {
        params.clear();   // el controlador libera los parametros; evitar punteros colgantes
        return SingleComponentEffect::terminate();
    }

    tresult PLUGIN_API setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                           SpeakerArrangement* outputs, int32 numOuts) SMTG_OVERRIDE
    {
        if (numIns == 1 && numOuts == 1 && inputs[0] == outputs[0] &&
            (inputs[0] == SpeakerArr::kStereo || inputs[0] == SpeakerArr::kMono))
        {
            removeAudioBusses();
            addAudioInput (u"Input", inputs[0]);
            addAudioOutput (u"Output", outputs[0]);
            return kResultTrue;
        }
        return kResultFalse;
    }

    tresult PLUGIN_API setupProcessing (ProcessSetup& setup) SMTG_OVERRIDE
    {
        sampleRate = setup.sampleRate;
        engine.load (kDspWasm, kDspWasmSize, (float) sampleRate, kNumParams);
        scratch.assign ((size_t) SR1Engine::kMaxBlock * 2, 0.0f);
        return SingleComponentEffect::setupProcessing (setup);
    }

    tresult PLUGIN_API setActive (TBool state) SMTG_OVERRIDE
    {
        if (state && engine.isReady()) engine.reset();
        return kResultOk;
    }

    tresult PLUGIN_API canProcessSampleSize (int32 s) SMTG_OVERRIDE { return s == kSample32 ? kResultTrue : kResultFalse; }
    uint32 PLUGIN_API getTailSamples() SMTG_OVERRIDE { return (uint32) (sampleRate * SR1_TAIL_SECONDS); }

    tresult PLUGIN_API process (ProcessData& data) SMTG_OVERRIDE
    {
        if (data.inputParameterChanges)
        {
            const int32 nq = data.inputParameterChanges->getParameterCount();
            for (int32 q = 0; q < nq; ++q)
                if (auto* queue = data.inputParameterChanges->getParameterData (q))
                {
                    const int32 np = queue->getPointCount();
                    ParamValue v; int32 off;
                    if (np > 0 && queue->getPoint (np - 1, off, v) == kResultTrue)
                        setParamNormalized (queue->getParameterId(), v);
                }
        }

        if (data.numSamples <= 0 || data.numInputs < 1 || data.numOutputs < 1) return kResultOk;

        for (int i = 0; i < kNumParams; ++i)
            engine.setParam (kParams[i].wasmId, (float) params[(size_t) i]->toPlain (getParamNormalized ((ParamID) i)));

        AudioBusBuffers& in = data.inputs[0];
        AudioBusBuffers& out = data.outputs[0];
        const int nIn = in.numChannels, nOut = out.numChannels;
        if (nIn < 1 || nOut < 1 || !in.channelBuffers32 || !out.channelBuffers32) return kResultOk;

        const float* inL = in.channelBuffers32[0];
        const float* inR = nIn > 1 ? in.channelBuffers32[1] : in.channelBuffers32[0];
        float* outL = out.channelBuffers32[0];
        float* outR = nOut > 1 ? out.channelBuffers32[1] : scratch.data();   // mono: descartar R

        for (int pos = 0; pos < data.numSamples; pos += SR1Engine::kMaxBlock)
        {
            const int n = std::min (SR1Engine::kMaxBlock, data.numSamples - pos);
            float* oR = nOut > 1 ? outR + pos : scratch.data();
            engine.process (inL + pos, inR + pos, outL + pos, oR, n);
        }
        out.silenceFlags = 0;
        return kResultOk;
    }

    tresult PLUGIN_API setState (IBStream* state) SMTG_OVERRIDE
    {
        if (!state) return kResultFalse;
        IBStreamer s (state, kLittleEndian);
        int32 version = 0, count = 0;
        if (!s.readInt32 (version) || !s.readInt32 (count) || version != 1) return kResultFalse;
        for (int32 i = 0; i < count; ++i)
        {
            double v;
            if (!s.readDouble (v)) break;
            if (i < kNumParams) setParamNormalized ((ParamID) i, v);
        }
        return kResultOk;
    }
    tresult PLUGIN_API getState (IBStream* state) SMTG_OVERRIDE
    {
        if (!state) return kResultFalse;
        IBStreamer s (state, kLittleEndian);
        s.writeInt32 (1);
        s.writeInt32 (kNumParams);
        for (int i = 0; i < kNumParams; ++i) s.writeDouble (getParamNormalized ((ParamID) i));
        return kResultOk;
    }
    tresult PLUGIN_API setComponentState (IBStream* state) SMTG_OVERRIDE { return setState (state); }

private:
    SR1Engine engine;
    double sampleRate = 44100.0;
    std::vector<SR1Param*> params;
    std::vector<float> scratch;
};

//------------------------------------------------------------------------
bool InitModule () { return true; }
bool DeinitModule () { return true; }

#define SR1_INLINE_UID(...) INLINE_UID (__VA_ARGS__)

BEGIN_FACTORY_DEF ("SR1", "https://www.example.com", "mailto:info@example.com")
    DEF_CLASS2 (SR1_INLINE_UID (SR1_UID),
                PClassInfo::kManyInstances, kVstAudioEffectClass, SR1_PLUGIN_NAME,
                0, PlugType::kFx, "1.0.0", kVstVersionString, SR1Effect::createInstance)
END_FACTORY
