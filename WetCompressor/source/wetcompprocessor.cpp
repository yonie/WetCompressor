//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "wetcompprocessor.h"
#include "wetcompcids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Steinberg;

namespace Yonie {

//------------------------------------------------------------------------
WetCompProcessor::WetCompProcessor()
{
    setControllerClass(kWetCompControllerUID);
}

//------------------------------------------------------------------------
WetCompProcessor::~WetCompProcessor() {}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompProcessor::initialize(FUnknown* context)
{
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk)
        return result;

    addAudioInput(STR16("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompProcessor::terminate()
{
    return AudioEffect::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompProcessor::setActive(TBool state)
{
    if (state)
    {
        engine.reset();
        inPeakL = inPeakR = outPeakL = outPeakR = 0.0f;
        oldInL = oldInR = oldOutL = oldOutR = oldGR = 0.0f;
        grDisplay = 0.0f;
    }
    return AudioEffect::setActive(state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompProcessor::setupProcessing(Vst::ProcessSetup& newSetup)
{
    engine.prepare(newSetup.sampleRate, newSetup.maxSamplesPerBlock);
    return AudioEffect::setupProcessing(newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompProcessor::canProcessSampleSize(int32 symbolicSampleSize)
{
    if (symbolicSampleSize == Vst::kSample32)
        return kResultTrue;
    return kResultFalse;
}

//------------------------------------------------------------------------
// Normalised parameter value -> discrete knob position.
static int toStep(Steinberg::Vst::ParamValue normalized, int stepCount)
{
    if (stepCount <= 1)
        return 0;
    int idx = static_cast<int>(normalized * (stepCount - 1) + 0.5);
    if (idx < 0) idx = 0;
    if (idx > stepCount - 1) idx = stepCount - 1;
    return idx;
}

//------------------------------------------------------------------------
void WetCompProcessor::applyParameter(Vst::ParamID id, Vst::ParamValue value)
{
    switch (id)
    {
        case kInputParam:  pending.input  = toStep(value, CompRange::kSteps); break;
        case kOutputParam: pending.output = toStep(value, CompRange::kSteps); break;
        case kModeParam:   pending.mode   = toStep(value, CompRange::kModeCount); break;
        default: break;
    }
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompProcessor::process(Vst::ProcessData& data)
{
    //--- parameter changes ------------------------------------------------
    if (data.inputParameterChanges)
    {
        const int32 numChanged = data.inputParameterChanges->getParameterCount();
        for (int32 index = 0; index < numChanged; ++index)
        {
            if (auto* queue = data.inputParameterChanges->getParameterData(index))
            {
                const int32 numPoints = queue->getPointCount();
                if (numPoints <= 0)
                    continue;
                Vst::ParamValue value;
                int32 sampleOffset;
                // Last point in the block. The engine glides the two gains
                // itself, so sample-accurate application would buy nothing.
                if (queue->getPoint(numPoints - 1, sampleOffset, value) == kResultTrue)
                    applyParameter(queue->getParameterId(), value);
            }
        }
        engine.setSettings(pending);
    }

    //--- audio ------------------------------------------------------------
    if (data.numInputs == 0 || data.numOutputs == 0 || data.numSamples <= 0)
        return kResultOk;

    Vst::AudioBusBuffers& input = data.inputs[0];
    Vst::AudioBusBuffers& output = data.outputs[0];

    if (input.numChannels < 2 || output.numChannels < 2)
    {
        for (int32 c = 0; c < output.numChannels; ++c)
            std::memset(output.channelBuffers32[c], 0,
                        data.numSamples * sizeof(Vst::Sample32));
        output.silenceFlags = ((uint64)1 << output.numChannels) - 1;
        return kResultOk;
    }

    float* inL = input.channelBuffers32[0];
    float* inR = input.channelBuffers32[1];
    float* outL = output.channelBuffers32[0];
    float* outR = output.channelBuffers32[1];

    for (int32 i = 0; i < data.numSamples; ++i)
    {
        updatePeak(inL[i], inPeakL);
        updatePeak(inR[i], inPeakR);
    }

    engine.processStereo(inL, inR, outL, outR, data.numSamples);

    for (int32 i = 0; i < data.numSamples; ++i)
    {
        updatePeak(outL[i], outPeakL);
        updatePeak(outR[i], outPeakR);
    }

    // GR reads downwards from 0, so the value the strip wants is how much of
    // its 22 dB range is being used. Rises instantly, falls slowly.
    {
        const float gr = engine.gainReductionDb() / CompEngine::kMeterRangeDb;
        grDisplay = gr > grDisplay ? gr : grDisplay * kGRDecay;
        if (grDisplay > 1.0f) grDisplay = 1.0f;
    }

    //--- meters -----------------------------------------------------------
    if (data.outputParameterChanges)
    {
        auto sendMeter = [&](Vst::ParamID id, float value, float& oldValue) {
            if (oldValue != value)
            {
                int32 index = 0;
                if (auto* queue = data.outputParameterChanges->addParameterData(id, index))
                {
                    int32 pointIndex = 0;
                    queue->addPoint(0, value, pointIndex);
                }
                oldValue = value;
            }
        };

        sendMeter(kInputMeterL, inPeakL.load(), oldInL);
        sendMeter(kInputMeterR, inPeakR.load(), oldInR);
        sendMeter(kOutputMeterL, outPeakL.load(), oldOutL);
        sendMeter(kOutputMeterR, outPeakR.load(), oldOutR);
        sendMeter(kGRMeter, grDisplay, oldGR);
    }

    output.silenceFlags = 0;
    return kResultOk;
}

//------------------------------------------------------------------------
void WetCompProcessor::updatePeak(float sample, std::atomic<float>& peak)
{
    const float absSample = std::abs(sample);
    const float currentPeak = peak.load();
    if (absSample > currentPeak)
        peak.store(absSample);                    // attack: instant
    else
        peak.store(currentPeak * kMeterDecay);    // decay: exponential
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompProcessor::setState(IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);

    int32 version = 0;
    if (!streamer.readInt32(version))
        return kResultFalse;

    // The grid the knob indices were written on. Version 2 says so; version 1
    // was written by v1.0.0 only, which had twenty-one positions.
    int32 savedSteps = 0;
    if (version >= 2)
        streamer.readInt32(savedSteps);
    if (savedSteps <= 1)
        savedSteps = CompRange::kLegacySteps21;

    CompEngine::Settings s;
    int32 v = 0;
    // Knob: rescaled onto the current grid. MODE is not a knob - it is three
    // named settings that have never been anything but three, so it is read
    // straight and must never be rescaled.
    auto rdKnob = [&](int& dst) {
        if (streamer.readInt32(v))
            dst = CompRange::rescaleStep(v, savedSteps);
    };
    auto rdPlain = [&](int& dst, int count) {
        if (streamer.readInt32(v))
        {
            if (v < 0) v = 0;
            if (v > count - 1) v = count - 1;
            dst = v;
        }
    };
    rdKnob(s.input);
    rdKnob(s.output);
    rdPlain(s.mode, CompRange::kModeCount);

    pending = s;
    engine.setSettings(s);
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompProcessor::getState(IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);

    // Versioned from the start, so a later parameter addition can still load an
    // old session. Version 2 also writes the step count, so the indices below
    // can be read back onto whatever grid a later build uses - see the note in
    // compengine.h about what happened to WetEQ without this.
    streamer.writeInt32(2);
    streamer.writeInt32(CompRange::kSteps);

    const CompEngine::Settings& s = engine.settings();
    streamer.writeInt32(s.input);
    streamer.writeInt32(s.output);
    streamer.writeInt32(s.mode);

    return kResultOk;
}

//------------------------------------------------------------------------
} // namespace Yonie
