//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "compengine.h"

#include <algorithm>
#include <random>

namespace Yonie {

//------------------------------------------------------------------------
CompEngine::CompEngine()
{
}

//------------------------------------------------------------------------
// The three buttons, as sidechain networks.
//
// The 1176's own ranges are 20 us to 800 us of attack and 50 ms to 1.1 s of
// release, both continuous. Three points across that is the WET answer: pick
// the three that are actually different from each other and print them on the
// panel.
//
// Every position carries TWO release constants. The fast one is the timing cap
// through its fixed resistor; the slow one is the same cap through the
// detector's own impedance, which only carries charge in proportion to how full
// the cap got. slowWeight is how much of the recovery goes that way, and it
// rises with the setting: FAST is nearly all quick recovery, SLOW leans on the
// long tail. That is the program dependence, and it comes out of the network
// rather than being modulated onto a release time.
//------------------------------------------------------------------------
Sidechain::Times CompEngine::timesFor(int mode)
{
    switch (mode)
    {
        case CompRange::kFast:
            //  50 us attack: grabs the transient itself, not what follows it.
            return { 0.05,  60.0,  260.0, 0.28 };
        case CompRange::kSlow:
            // 800 us attack: the whole leading edge is through before the cell
            // moves, which is the setting that makes drums bigger, not smaller.
            return { 0.80, 400.0, 1500.0, 0.52 };
        case CompRange::kNormal:
        default:
            return { 0.30, 150.0,  700.0, 0.40 };
    }
}

//------------------------------------------------------------------------
void CompEngine::prepare(double hostSampleRate, int maxBlockSize)
{
    (void)maxBlockSize;
    hostRate = hostSampleRate > 0.0 ? hostSampleRate : 44100.0;

    // Draw this unit's component tolerances once. A given build of the plugin
    // is a given unit off the line: the values were soldered in at the factory
    // and do not wander while you use it.
    {
        std::mt19937 tol(0xC0FFEE);
        std::uniform_real_distribution<double> spread(-kTolerance, kTolerance);
        for (int i = 0; i < 4; ++i)
        {
            tolL[i] = 1.0 + spread(tol);
            tolR[i] = 1.0 + spread(tol);
        }
    }

    // A different seed per stage per channel, so the eight noise sources are
    // uncorrelated. Sharing one generator makes the whole unit hiss in unison,
    // which reads as a single added noise rather than as a floor.
    {
        uint32_t sd = 0x5EED0001u;
        chainL.iron.setNoiseSeed(sd += 0x9E3779B9u);
        chainL.cell.setNoiseSeed(sd += 0x9E3779B9u);
        chainL.preamp.setNoiseSeed(sd += 0x9E3779B9u);
        chainL.outAmp.setNoiseSeed(sd += 0x9E3779B9u);
        chainL.outIron.setNoiseSeed(sd += 0x9E3779B9u);
        chainR.iron.setNoiseSeed(sd += 0x9E3779B9u);
        chainR.cell.setNoiseSeed(sd += 0x9E3779B9u);
        chainR.preamp.setNoiseSeed(sd += 0x9E3779B9u);
        chainR.outAmp.setNoiseSeed(sd += 0x9E3779B9u);
        chainR.outIron.setNoiseSeed(sd += 0x9E3779B9u);
        tiltNoiseL.seed(sd += 0x9E3779B9u);
        tiltNoiseR.seed(sd += 0x9E3779B9u);
    }

    // Transformer corners carry the tolerance too: two irons wound on the same
    // machine still measure a few percent apart.
    chainL.iron.prepare(hostRate, kInIronFluxHz * tolL[0], kInIronTopHz * tolL[0]);
    chainR.iron.prepare(hostRate, kInIronFluxHz * tolR[0], kInIronTopHz * tolR[0]);
    chainL.outIron.prepare(hostRate, kOutIronFluxHz * tolL[3], kOutIronTopHz * tolL[3]);
    chainR.outIron.prepare(hostRate, kOutIronFluxHz * tolR[3], kOutIronTopHz * tolR[3]);

    sidechain.prepare(hostRate);

    hissTiltL.setCoefficients(hostRate, kHissTiltHz, OnePoleFilter::Type::LowPass);
    hissTiltR.setCoefficients(hostRate, kHissTiltHz, OnePoleFilter::Type::LowPass);

    glideCoeff = static_cast<float>(1.0 - std::exp(-1.0 / (kGlideMs * 0.001 * hostRate)));

    updateFromSettings();
    // Snap rather than glide on prepare: the glide exists to hide a knob move,
    // not to fade the plugin in every time the host starts.
    inGain = inGainTarget;
    outGain = outGainTarget;

    reset();
}

//------------------------------------------------------------------------
void CompEngine::reset()
{
    chainL.reset();
    chainR.reset();
    sidechain.reset();
    hissTiltL.reset();
    hissTiltR.reset();
    lastGain = 1.0f;
    grPeakDb = 0.0f;
}

//------------------------------------------------------------------------
void CompEngine::setToleranceEnabled(bool on)
{
    if (!on)
        for (int i = 0; i < 4; ++i) { tolL[i] = 1.0; tolR[i] = 1.0; }
    prepare(hostRate, 0);
}

//------------------------------------------------------------------------
void CompEngine::setSettings(const Settings& s)
{
    if (s != current)
    {
        current = s;
        updateFromSettings();
    }
}

//------------------------------------------------------------------------
void CompEngine::updateFromSettings()
{
    auto knobGain = [](int step) -> float {
        return static_cast<float>(std::pow(10.0, CompRange::stepDb(step) / 20.0));
    };

    inGainTarget  = knobGain(current.input);
    outGainTarget = knobGain(current.output);
    sidechain.setTimes(timesFor(current.mode));
}

//------------------------------------------------------------------------
void CompEngine::processStereo(const float* inL, const float* inR,
                               float* outL, float* outR, int numSamples)
{
    if (numSamples <= 0)
        return;

    const float bleed = crosstalk ? kStageBleed : 0.0f;
    const float hs = hiss ? kStageHiss : 0.0f;

    float grPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        inGain  += (inGainTarget  - inGain)  * glideCoeff;
        outGain += (outGainTarget - outGain) * glideCoeff;

        float l = inL[i] * inGain;
        float r = inR[i] * inGain;

        //--- stage 1: input transformer ------------------------------------
        {
            const float pl = l, pr = r;
            l = chainL.iron.process(pl, kInIronDrive * static_cast<float>(tolL[0]),
                                    bleed * pr, hs);
            r = chainR.iron.process(pr, kInIronDrive * static_cast<float>(tolR[0]),
                                    bleed * pl, hs);
        }

        //--- stage 2: the gain cell, with the loop one sample behind --------
        //
        // lastGain was computed from the PREVIOUS sample's output. The detector
        // cannot see this sample until it has been through the cell, so the
        // leading edge of every transient gets through at whatever gain the
        // cell was already sitting at. That overshoot is the point.
        const float g = lastGain;
        {
            const float pl = l, pr = r;
            l = chainL.cell.process(pl, g, bleed * pr, hs);
            r = chainR.cell.process(pr, g, bleed * pl, hs);
        }

        //--- detector, tapped from the cell's output ------------------------
        //
        // Peak, not RMS: the 1176's detector is a rectifier. Linked across the
        // pair, so a loud left channel pulls the right one down with it and the
        // image stays put.
        {
            const float rect = std::max(std::fabs(l), std::fabs(r));

            // How much reduction the loop is asking for, in dB. Below the fixed
            // threshold it asks for none - and because the detector is fed from
            // the cell's output, this is a CLOSED-LOOP request: in steady state
            // it settles at out = T + (in - T) / (1 + beta), i.e. a 4:1 ratio,
            // without a ratio ever appearing as a number in the signal path.
            float request = 0.0f;
            if (rect > 1e-7f)
            {
                const float over = 20.0f * std::log10(rect) - kThresholdDb;
                if (over > 0.0f)
                    request = kBeta * over;
            }

            const float cv = sidechain.process(request);
            lastGain = FETGainCell::gainFor(cv);
            if (cv > grPeak)
                grPeak = cv;
        }

        //--- stage 3: class-A preamp, single-ended (second harmonic) --------
        if (saturate)
        {
            const float pl = l, pr = r;
            l = chainL.preamp.process(pl, kPreHeadroom * static_cast<float>(tolL[1]),
                                      kPreBias, bleed * pr, hs);
            r = chainR.preamp.process(pr, kPreHeadroom * static_cast<float>(tolR[1]),
                                      kPreBias, bleed * pl, hs);
        }

        //--- stage 4: push-pull output amp (third) into the output iron -----
        if (saturate)
        {
            const float pl = l, pr = r;
            l = chainL.outAmp.process(pl, kOutHeadroom * static_cast<float>(tolL[2]),
                                      kCrossover, bleed * pr, hs);
            r = chainR.outAmp.process(pr, kOutHeadroom * static_cast<float>(tolR[2]),
                                      kCrossover, bleed * pl, hs);

            const float ql = l, qr = r;
            l = chainL.outIron.process(ql, kOutIronDrive * static_cast<float>(tolL[3]),
                                       0.0f, 0.0f);
            r = chainR.outIron.process(qr, kOutIronDrive * static_cast<float>(tolR[3]),
                                       0.0f, 0.0f);
        }

        //--- the warm half of the noise ------------------------------------
        //
        // The four stage generators are white. Op-amp and resistor noise carries
        // a 1/f component, so analog hiss is warmer than a dither generator's.
        // One extra low-passed source per channel gets most of the way there for
        // the price of one filter. Flat white noise is a tell.
        //
        // Its own generator, not a filtered copy of the signal: filtering the
        // signal would be an EQ, not a noise floor.
        if (hs > 0.0f)
        {
            l += hissTiltL.process(tiltNoiseL.next()) * kTiltHiss;
            r += hissTiltR.process(tiltNoiseR.next()) * kTiltHiss;
        }

        outL[i] = l * outGain;
        outR[i] = r * outGain;
    }

    // What the cell actually delivered, not what the loop asked for: past about
    // -38 dB the FET bottoms out and the meter should stop moving with it.
    grPeakDb = -20.0f * std::log10(std::max(1e-6f, FETGainCell::gainFor(grPeak)));
}

//------------------------------------------------------------------------
} // namespace Yonie
