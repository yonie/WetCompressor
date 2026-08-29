//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "compengine.h"
#include "wetcompcids.h"
#include <atomic>

namespace Yonie {

//------------------------------------------------------------------------
class WetCompProcessor : public Steinberg::Vst::AudioEffect
{
public:
    WetCompProcessor();
    ~WetCompProcessor() SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance(void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new WetCompProcessor;
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;

protected:
    void applyParameter(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue value);

    CompEngine engine;
    CompEngine::Settings pending;

    // Four peak meters, one per painted strip, plus the gain-reduction feed.
    std::atomic<float> inPeakL{0.0f}, inPeakR{0.0f};
    std::atomic<float> outPeakL{0.0f}, outPeakR{0.0f};
    float oldInL = 0.0f, oldInR = 0.0f, oldOutL = 0.0f, oldOutR = 0.0f;
    float oldGR = 0.0f;

    // The GR meter falls back to rest more slowly than it moves, so a fast
    // release does not make the strip flicker. The needle on a real unit has
    // mass; this is the LED equivalent.
    float grDisplay = 0.0f;
    static constexpr float kGRDecay = 0.82f;

    static constexpr float kMeterDecay = 0.9995f;

    void updatePeak(float sample, std::atomic<float>& peak);
};

//------------------------------------------------------------------------
} // namespace Yonie
