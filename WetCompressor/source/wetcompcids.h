//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace Yonie {

//------------------------------------------------------------------------
static const Steinberg::FUID kWetCompProcessorUID (0x5C24A9E1, 0x8B7346D0, 0x91FE2A5C, 0x63047BD8);
static const Steinberg::FUID kWetCompControllerUID (0x1F86D30B, 0x4A9E52C7, 0xBD1058E3, 0x2C97F640);

#define WetCompVST3Category "Fx|Dynamics"

//------------------------------------------------------------------------
// Parameter IDs
//
// Three controls, because the circuit has three: how hard you drive it, how
// fast it lets go, and how much you put back. There is no threshold and no
// ratio because the original has neither.
//------------------------------------------------------------------------
enum WetCompParams : Steinberg::Vst::ParamID
{
    kInputParam   = 0,    // drive into the cell, -24..+24 dB, continuous
    kOutputParam  = 1,    // makeup, -24..+24 dB, continuous
    kModeParam    = 2,    // FAST / NORMAL / SLOW

    // Output-only, for the five LED strips. The panel paints IN and OUT as
    // stereo pairs, so each channel gets its own; GR is one strip because there
    // is one sidechain.
    kInputMeterL  = 3,
    kInputMeterR  = 4,
    kGRMeter      = 5,
    kOutputMeterL = 6,
    kOutputMeterR = 7,

    kParamCount   = 8
};

//------------------------------------------------------------------------
} // namespace Yonie
