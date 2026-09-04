//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "wetcompcontroller.h"
#include "wetcompcids.h"
#include "compengine.h"
#include "customviewcreator.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "vstgui/plugin-bindings/vst3editor.h"

#include <cmath>
#include <cstdio>

using namespace Steinberg;

namespace Yonie {

namespace {

//------------------------------------------------------------------------
// A stepped dB parameter that prints exactly what the panel prints, including
// the two end stops. The strings come from the same CompRange table the DSP
// uses, so the readout and the gain can never disagree.
//------------------------------------------------------------------------
class GainParameter : public Vst::Parameter
{
public:
    GainParameter(const Vst::TChar* title, Vst::ParamID id)
    {
        Vst::ParameterInfo& i = info;
        UString(i.title, str16BufferSize(Vst::String128)).assign(title);
        i.id = id;
        i.stepCount = CompRange::kSteps - 1;
        i.defaultNormalizedValue =
            static_cast<double>(CompRange::kCentreStep) / (CompRange::kSteps - 1);
        i.unitId = Vst::kRootUnitId;
        i.flags = Vst::ParameterInfo::kCanAutomate;
        setNormalized(i.defaultNormalizedValue);
    }

    static int stepOf(Vst::ParamValue normalized)
    {
        int idx = static_cast<int>(normalized * (CompRange::kSteps - 1) + 0.5);
        if (idx < 0) idx = 0;
        if (idx > CompRange::kSteps - 1) idx = CompRange::kSteps - 1;
        return idx;
    }

    void toString(Vst::ParamValue normalized, Vst::String128 string) const SMTG_OVERRIDE
    {
        char text[64];
        std::snprintf(text, sizeof(text), "%+.0f dB",
                      CompRange::stepDb(stepOf(normalized)));
        UString(string, str16BufferSize(Vst::String128)).fromAscii(text);
    }

    bool fromString(const Vst::TChar* string, Vst::ParamValue& normalized) const SMTG_OVERRIDE
    {
        if (!string)
            return false;
        UString wrapper(const_cast<Vst::TChar*>(string), str16BufferSize(Vst::String128));
        double want = 0.0;
        if (!wrapper.scanFloat(want))
            return false;

        // Snap whatever was typed to the nearest detent.
        int best = 0;
        double bestErr = 1e30;
        for (int i = 0; i < CompRange::kSteps; ++i)
        {
            const double err = std::abs(CompRange::stepDb(i) - want);
            if (err < bestErr) { bestErr = err; best = i; }
        }
        normalized = static_cast<double>(best) / (CompRange::kSteps - 1);
        return true;
    }
};

//------------------------------------------------------------------------
class ModeParameter : public Vst::Parameter
{
public:
    ModeParameter(const Vst::TChar* title, Vst::ParamID id)
    {
        Vst::ParameterInfo& i = info;
        UString(i.title, str16BufferSize(Vst::String128)).assign(title);
        i.id = id;
        i.stepCount = CompRange::kModeCount - 1;
        i.defaultNormalizedValue =
            static_cast<double>(CompRange::kNormal) / (CompRange::kModeCount - 1);
        i.unitId = Vst::kRootUnitId;
        i.flags = Vst::ParameterInfo::kCanAutomate | Vst::ParameterInfo::kIsList;
        setNormalized(i.defaultNormalizedValue);
    }

    void toString(Vst::ParamValue normalized, Vst::String128 string) const SMTG_OVERRIDE
    {
        static const char* names[] = { "FAST", "NORMAL", "SLOW" };
        int idx = static_cast<int>(normalized * (CompRange::kModeCount - 1) + 0.5);
        if (idx < 0) idx = 0;
        if (idx > CompRange::kModeCount - 1) idx = CompRange::kModeCount - 1;
        UString(string, str16BufferSize(Vst::String128)).fromAscii(names[idx]);
    }
};

} // namespace

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompController::initialize(FUnknown* context)
{
    tresult result = EditControllerEx1::initialize(context);
    if (result != kResultOk)
        return result;

    registerCustomViews();

    parameters.addParameter(new GainParameter(
        reinterpret_cast<const Vst::TChar*>(u"Input"), kInputParam));
    parameters.addParameter(new GainParameter(
        reinterpret_cast<const Vst::TChar*>(u"Output"), kOutputParam));
    parameters.addParameter(new ModeParameter(
        reinterpret_cast<const Vst::TChar*>(u"Mode"), kModeParam));

    // Meter feeds. Read-only so a host never tries to automate them.
    parameters.addParameter(STR16("Input Meter L"), nullptr, 0, 0,
                            Vst::ParameterInfo::kIsReadOnly, kInputMeterL);
    parameters.addParameter(STR16("Input Meter R"), nullptr, 0, 0,
                            Vst::ParameterInfo::kIsReadOnly, kInputMeterR);
    parameters.addParameter(STR16("Gain Reduction"), nullptr, 0, 0,
                            Vst::ParameterInfo::kIsReadOnly, kGRMeter);
    parameters.addParameter(STR16("Output Meter L"), nullptr, 0, 0,
                            Vst::ParameterInfo::kIsReadOnly, kOutputMeterL);
    parameters.addParameter(STR16("Output Meter R"), nullptr, 0, 0,
                            Vst::ParameterInfo::kIsReadOnly, kOutputMeterR);

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompController::terminate()
{
    return EditControllerEx1::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompController::setComponentState(IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);

    int32 version = 0;
    if (!streamer.readInt32(version))
        return kResultFalse;

    // Same read as WetCompProcessor::setState, and it has to stay the same: a
    // host hands the identical stream to both, and a disagreement shows up as a
    // UI that does not match what you hear.
    int32 savedSteps = 0;
    if (version >= 2)
        streamer.readInt32(savedSteps);
    if (savedSteps <= 1)
        savedSteps = CompRange::kLegacySteps21;

    int32 v = 0;
    auto restoreKnob = [&](Vst::ParamID id) {
        if (streamer.readInt32(v))
        {
            const int idx = CompRange::rescaleStep(v, savedSteps);
            setParamNormalized(id, static_cast<double>(idx) / (CompRange::kSteps - 1));
        }
    };
    auto restorePlain = [&](Vst::ParamID id, int count) {
        if (streamer.readInt32(v))
        {
            if (v < 0) v = 0;
            if (v > count - 1) v = count - 1;
            setParamNormalized(id, count > 1 ? static_cast<double>(v) / (count - 1) : 0.0);
        }
    };
    restoreKnob(kInputParam);
    restoreKnob(kOutputParam);
    restorePlain(kModeParam, CompRange::kModeCount);

    return kResultOk;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API WetCompController::createView(FIDString name)
{
    if (FIDStringsEqual(name, Vst::ViewType::kEditor))
    {
        auto* editor = new VSTGUI::VST3Editor(this, "view", "wetcompeditor.uidesc");

        // Discrete zoom steps, as WetEQ. The assets are baked at 1x, so
        // anything above 125% interpolates and goes soft.
        editor->setAllowedZoomFactors({0.75, 1.0, 1.25});
        return editor;
    }
    return nullptr;
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompController::setState(IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetCompController::getState(IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
} // namespace Yonie
