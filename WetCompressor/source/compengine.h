//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//
// WetCompressor's DSP. No VST3 headers, so tools/comptest can compile and
// measure this directly without loading a plugin host.
//
// Signal chain, per channel - four amplifiers, in the order an 1176 has them:
//
//   in -> INPUT (drive) -> input transformer -> FET gain cell
//      -> class-A preamp -> push-pull output stage + output transformer
//      -> OUTPUT (makeup) -> out
//
// with ONE sidechain across both channels, tapped from the output of the gain
// cell, and each of the four stages contributing its own noise and its own leak
// into the channel beside it.
//
// Nothing is resampled, quantised or band-limited. Same call as WetEQ: an 1176
// is transformers, a FET and two amplifiers - it has no sample rate and no word
// length, so modelling one with sampler artefacts puts a digital fingerprint on
// a device that never had one. The lo-fi core is authentic on WetDelay and
// WetReverb, which model DIGITAL units. It is not authentic here.
//
// INPUT is not a trim and OUTPUT is not a level: on an 1176 the threshold is a
// fixed property of the circuit and INPUT is how far you drive the signal past
// it. More INPUT is more compression, and OUTPUT puts back what that cost. That
// is why there is no threshold knob and no ratio knob - the panel has exactly
// the controls the circuit has.
//------------------------------------------------------------------------

#pragma once

#include "wetcore.h"
#include "fetcore.h"

namespace Yonie {

//------------------------------------------------------------------------
// Parameter ranges. These are the values printed on the panel.
//------------------------------------------------------------------------
namespace CompRange {

// INPUT and OUTPUT are STEPPED, like every other control in the line. Coarse on
// purpose: it forces a decision instead of inviting a 0.5 dB fiddle, which is
// the same reasoning as WetEQ's nine detents and WetDelay's six delay times.
//
// ELEVEN positions, EVENLY spaced, +/-30 dB in 6 dB steps. Odd, so the centre
// detent is a real position: 0 dB at twelve o'clock, unity reachable, and boost
// mirroring cut exactly.
//
// Ronald, 2026-08-29: "more like a mastering comp with like 9 or 11 settings
// only. to force choice... as this is only a single input button, a bit more
// fine control is ok, so 11 or 13 choices also works."
//
// The panel art prints an eleven-value scale of its own (-inf, -24 ... +24,
// +inf) with the crowded spacing of an audio taper, and the first version of
// this matched it detent for detent. That was over-fitting - Ronald, the same
// day: "you DONT need to exactly match the UI here. its fine to just have
// evenly spaced steps. better even. the UI is 'inspiration' but it doesnt all
// have to be perfectly chirurical." An even 6 dB step is what a mastering
// control does, and it means every detent is the same size in the ear as well
// as on the panel.
constexpr int kSteps  = 11;
constexpr double kStepDbSize = 6.0;
constexpr double kStepDbMin  = -30.0;

constexpr int kCentreStep = kSteps / 2;      // 5 -> 0 dB, twelve o'clock

inline double stepDb(int step)
{
    if (step < 0) step = 0;
    if (step > kSteps - 1) step = kSteps - 1;
    return kStepDbMin + kStepDbSize * step;
}

// FAST / NORMAL / SLOW. There is no attack or release knob because the three
// buttons pick a point in the range the hardware's two pots cover, and three
// well-chosen points is the WET answer to a pair of continuous controls - the
// same reasoning as WetDelay's six fixed delay times.
enum Mode { kFast = 0, kNormal = 1, kSlow = 2, kModeCount = 3 };

} // namespace CompRange

//------------------------------------------------------------------------
// CompEngine
//------------------------------------------------------------------------
class CompEngine
{
public:
    CompEngine();

    struct Settings
    {
        // Discrete knob positions, index based, because every WET control is a
        // stepped encoder rather than a continuous pot. Derived from the step
        // count rather than written as literals: a fresh insert has to be
        // audibly neutral, and a literal is what lets that drift.
        int input  = CompRange::kCentreStep;    // 0 dB
        int output = CompRange::kCentreStep;    // 0 dB
        int mode   = CompRange::kNormal;

        bool operator==(const Settings& o) const
        {
            return input == o.input && output == o.output && mode == o.mode;
        }
        bool operator!=(const Settings& o) const { return !(*this == o); }
    };

    void prepare(double hostSampleRate, int maxBlockSize);
    void reset();

    void setSettings(const Settings& s);
    const Settings& settings() const { return current; }

    void processStereo(const float* inL, const float* inR,
                       float* outL, float* outR, int numSamples);

    // Peak gain reduction over the block just processed, in dB (positive means
    // reducing). The GR meter reads this.
    float gainReductionDb() const { return grPeakDb; }

    double inputDb()  const { return CompRange::stepDb(current.input); }
    double outputDb() const { return CompRange::stepDb(current.output); }

    // The panel prints the GR scale 0 to -22.
    static constexpr float kMeterRangeDb = 22.0f;

    // The threshold is a CONSTANT, because on an 1176 it is: the detector fires
    // where the circuit's fixed bias says it does, and the only way past it is
    // to drive the input harder. -20 dBFS puts a normally levelled track just
    // touching it with INPUT at twelve o'clock.
    static constexpr float kThresholdDb = -20.0f;

    // Closed-loop ratio, fixed at 4:1 - the ratio the panel does not offer,
    // because the panel has no ratio buttons. kBeta is the loop gain that
    // produces it: out = T + (in - T) / (1 + beta).
    static constexpr float kRatio = 4.0f;
    static constexpr float kBeta  = kRatio - 1.0f;

    // Test hooks, so the harness can isolate the compressor from the analog
    // stages and measure a ratio curve against the intended one.
    void setSaturationEnabled(bool on) { saturate = on; }
    void setCrosstalkEnabled(bool on)  { crosstalk = on; }
    void setHissEnabled(bool on)       { hiss = on; }
    void setToleranceEnabled(bool on);

    // The three button positions, as sidechain networks. Public so the test
    // harness can print them next to what it measures.
    static Sidechain::Times timesFor(int mode);

private:
    void updateFromSettings();

    Settings current;
    double hostRate = 44100.0;

    //--- the four stages, per channel ---------------------------------------
    //
    // Named rather than held in an array, because unlike WetEQ's six identical
    // filter stages these are four DIFFERENT circuits and the detector taps
    // between the second and the third. A generic loop would hide that.
    struct Chain
    {
        Transformer   iron;     // input transformer
        FETGainCell   cell;     // the gain cell
        ClassAStage   preamp;   // single-ended, second harmonic
        PushPullStage outAmp;   // push-pull + output iron, third harmonic
        Transformer   outIron;

        void reset()
        {
            iron.reset(); cell.reset(); preamp.reset();
            outAmp.reset(); outIron.reset();
        }
    };
    Chain chainL, chainR;

    // ONE sidechain for both channels. A pair of 1176s doing stereo is strapped
    // together for exactly this reason: two independent detectors move the
    // image every time one channel is louder. The detector sees the louder of
    // the two and both cells follow it.
    Sidechain sidechain;

    // The loop is always one sample behind the signal, because the detector is
    // downstream of the cell. That is not a latency to compensate - it IS the
    // overshoot that lets a transient through.
    float lastGain = 1.0f;

    // The knobs are stepped, so a change is a JUMP, and a jump is a click - the
    // gain leaps by a factor of two in one detent. What moves smoothly is the
    // value behind the detent, over a few milliseconds: short enough that the
    // change still feels instant, long enough that nothing steps.
    float inGain = 1.0f, inGainTarget = 1.0f;
    float outGain = 1.0f, outGainTarget = 1.0f;
    static constexpr double kGlideMs = 20.0;
    float glideCoeff = 0.01f;

    float grPeakDb = 0.0f;

    // Component tolerance, as in WetEQ: real parts are not matched, so the two
    // channels of one unit are never quite the same channel twice. Applied to
    // the drive each stage sees and to the transformer corners.
    static constexpr double kTolerance = 0.025;
    double tolL[4] = {1, 1, 1, 1};
    double tolR[4] = {1, 1, 1, 1};

    // The warm half of the noise floor: one low-passed source per channel, on
    // top of the four white stage sources.
    OnePoleFilter hissTiltL, hissTiltR;
    StageNoise tiltNoiseL, tiltNoiseR;

    bool saturate = true;
    bool crosstalk = true;
    bool hiss = true;

    // Per-stage crosstalk and noise. The totals are the line's: about -40 dB of
    // bleed and -88 dBFS of hiss. Injected at every stage rather than once at
    // the input, so the bleed a later stage sees has already been through the
    // stages before it - which is what happens in a chassis where the two
    // channels run the length of the board side by side.
    static constexpr float kStageBleed = 0.0026f;   // 4 stages -> about -40 dB
    static constexpr float kStageHiss  = 3.5e-5f;   // 4 uncorrelated -> -88 dBFS
    static constexpr double kHissTiltHz = 1800.0;   // corner of the warm half
    // A one-pole at 1.8 kHz throws away most of white noise's power, so the
    // tilted source has to be driven harder than the flat ones to contribute
    // anything at all.
    static constexpr float  kTiltHiss  = 2.6e-4f;

    //--- stage constants ----------------------------------------------------
    //
    // Input iron: saturates in the bottom two octaves, loses a little at the
    // very top. Output iron is bigger and looser - it holds the low end better
    // and rolls off sooner, which is the classic transformer-coupled top.
    static constexpr double kInIronFluxHz = 120.0;
    static constexpr double kInIronTopHz  = 21000.0;
    static constexpr float  kInIronDrive  = 1.15f;

    static constexpr double kOutIronFluxHz = 90.0;
    static constexpr double kOutIronTopHz  = 17000.0;
    static constexpr float  kOutIronDrive  = 1.05f;

    // Class-A preamp. bias below 1 makes the positive half compress sooner,
    // which is what puts second above third.
    static constexpr float kPreHeadroom = 2.10f;
    static constexpr float kPreBias     = 0.72f;

    // Push-pull output. Runs out well before a converter does, which is why the
    // unit is a colour box as well as a compressor.
    static constexpr float kOutHeadroom = 1.35f;
    // Device mismatch in the output pair, not a crossover: the stage is class A.
    static constexpr float kCrossover   = 0.012f;
};

//------------------------------------------------------------------------
} // namespace Yonie
