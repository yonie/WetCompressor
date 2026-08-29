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
// A continuous dB parameter that prints what the panel prints - including the
// -inf at the bottom of the travel, which is a real position and not a rounding
// artefact.
//------------------------------------------------------------------------
class GainParameter : public Vst::Parameter
{
public:
    GainParameter(const Vst::TChar* title, Vst::ParamID id, double lo, double hi)
    : lo(lo), hi(hi)
    {
        Vst::ParameterInfo& i = info;
        UString(i.title, str16BufferSize(Vst::String128)).assign(title);
        i.id = id;
        i.stepCount = 0;                       // continuous
        i.defaultNormalizedValue = 0.5;        // twelve o'clock = 0 dB
        i.unitId = Vst::kRootUnitId;
        i.flags = Vst::ParameterInfo::kCanAutomate;
        setNormalized(i.defaultNormalizedValue);
    }

    void toString(Vst::ParamValue normalized, Vst::String128 string) const SMTG_OVERRIDE
    {
        char text[64];
        if (normalized <= CompRange::kMuteBelow)
            std::snprintf(text, sizeof(text), "-inf dB");
        else
            std::snprintf(text, sizeof(text), "%+.1f dB",
                          CompRange::normToDb(normalized, lo, hi));
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
        double t = (want - lo) / (hi - lo);
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        normalized = t;
        return true;
    }

private:
    double lo, hi;
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
        reinterpret_cast<const Vst::TChar*>(u"Input"), kInputParam,
        CompRange::kInputMinDb, CompRange::kInputMaxDb));
    parameters.addParameter(new GainParameter(
        reinterpret_cast<const Vst::TChar*>(u"Output"), kOutputParam,
        CompRange::kOutputMinDb, CompRange::kOutputMaxDb));
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

    double d = 0.0;
    int32 v = 0;
    if (streamer.readDouble(d)) setParamNormalized(kInputParam, d);
    if (streamer.readDouble(d)) setParamNormalized(kOutputParam, d);
    if (streamer.readInt32(v))
    {
        if (v < 0) v = 0;
        if (v > CompRange::kModeCount - 1) v = CompRange::kModeCount - 1;
        setParamNormalized(kModeParam,
                           static_cast<double>(v) / (CompRange::kModeCount - 1));
    }

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
