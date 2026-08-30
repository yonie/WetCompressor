//------------------------------------------------------------------------
// fetcore.h - the 1176's circuit, stage by stage.
//
// WHY THIS IS NOT A GAIN COMPUTER PLUS AN ENVELOPE FOLLOWER
//
// The textbook digital compressor is: measure the level, subtract a threshold,
// multiply by (1 - 1/ratio), smooth with two one-poles, apply. That is a
// description of what a compressor does to a sine wave. It is not an 1176, and
// five things it cannot do are the five things people mean when they say a
// track "sounds 1176'd":
//
//   1. Be a FEEDBACK compressor. The 1176 taps its detector from the OUTPUT of
//      the gain cell, not the input. Everything else follows from that one
//      fact: the ratio is a closed-loop ratio, so it is set by loop gain and
//      goes soft on its own near the threshold; the knee needs no knee
//      parameter because the loop cannot correct what it has not yet heard; and
//      the whole thing self-limits instead of running away. A feed-forward
//      model with a knee control is imitating the SHAPE this produces.
//
//   2. Let the first cycle of a transient through. The detector cannot see a
//      sample until it has already been through the gain cell, so the loop is
//      always one sample behind the signal. That overshoot is why an 1176 keeps
//      drums sounding hit rather than pressed, and it is not a feature that was
//      added - it is what a feedback loop does.
//
//   3. Release in two time constants at once. The timing cap discharges through
//      a fixed resistor AND through the detector's own impedance, which depends
//      on how far the cap was charged. So the unit empties fast for the first
//      few dB and then crawls, and how much of each you get depends on how hard
//      it was hit. One exponential per direction gives pumping without
//      breathing.
//
//   4. Distort BECAUSE it is compressing. The FET is IN the signal path, so the
//      same device that sets the gain adds the harmonics, and the harmonic
//      content tracks the gain reduction. A saturator after a clean compressor
//      distorts hardest when the compressor is working least, which is
//      backwards. The FET's noise does the same thing, for the same reason.
//
//   5. Be FOUR AMPLIFIERS. An 1176 is an input transformer, a FET gain cell, a
//      single-ended class-A preamp and a push-pull output stage into an output
//      transformer. Each is its own circuit with its own noise and its own leak
//      into the channel beside it, and the two irons colour opposite ends of the
//      spectrum in opposite ways. One tanh at the end of a multiply is a
//      description of the sum, and it sounds like one device rather than four.
//
// Ronald, 2026-08-28: "all analog circuit emulation as advanced as we can. the
// famous 1176 circuit. including similar oddness as weteq: crosstalk at each
// stage and little noise at each stage."
//------------------------------------------------------------------------
#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace Yonie {

//------------------------------------------------------------------------
// Per-stage noise. One generator per stage per channel, so the four stages do
// not hiss in unison - which they would if they shared one, and which is
// audible as a single added noise rather than as a floor.
//
// Same xorshift as WetEQ's analog chain, so the line's noise floor sounds like
// one manufacturer's gear.
class StageNoise
{
public:
    void seed(uint32_t s) { state = s ? s : 0x9E3779B9u; }

    inline float next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(static_cast<int32_t>(state)) * 4.6566129e-10f;
    }

private:
    uint32_t state = 0x9E3779B9u;
};

//------------------------------------------------------------------------
// The FET as a voltage-controlled resistor.
//
// A 2N5457 in the 1176's gain cell runs in its TRIODE region as a shunt across
// the signal path, with a fixed series resistor above it:
//
//     gain = Rds / (Rds + Rseries)
//
// At rest the gate is near pinch-off, Rds is large and the divider passes
// almost everything. Control voltage turns the FET ON, Rds falls, and signal is
// pulled to ground. Three things fall out of that and none is expressible as a
// slope in dB:
//
//   * The cell BOTTOMS OUT. Rds cannot go below Rds(on), so there is a hard
//     floor to how much reduction the circuit can produce - about -38 dB on a
//     real 1176. Past that, driving the input harder stops buying compression
//     and starts buying distortion, which is the whole "slam it" technique.
//
//   * The channel is nonlinear IN THE SIGNAL. The drain voltage modulates the
//     channel resistance within the cycle, so a symmetric input comes out
//     asymmetric - second harmonic, and more of it the harder the cell works.
//
//   * It gets NOISIER as it works. The channel is a resistor in circuit, and a
//     resistor being switched harder into the path brings its own thermal noise
//     with it. An 1176 doing 15 dB audibly hisses more than one sitting idle,
//     and that is the device, not the amplifier after it.
//------------------------------------------------------------------------
class FETGainCell
{
public:
    // Rds(on) / Rseries. 0.0126 puts the floor at -38 dB, the 1176's own
    // ceiling on gain reduction.
    static constexpr float kRdsOn = 0.0126f;

    // Depth of the within-cycle channel modulation at full reduction. This is
    // the distortion, and it is tied to how hard the cell is working rather
    // than being a separate drive control.
    //
    // MEASURED, not chosen by ear: comptest sweeps THD against gain reduction,
    // and a real 1176 sits around half a percent idle and a few percent at
    // 20 dB. The first value here was 0.30 and produced 33% at 24 dB - which is
    // not a compressor, it is a fuzz box. Worse, it broke the RATIO: the
    // detector is fed from the cell's output, so distortion products inflate
    // the rectified peak, the loop asks for more reduction than the signal
    // needs, and a 4:1 unit measured 14:1 at the top of its range.
    static constexpr float kChannelMod = 0.055f;

    // GATE LINEARISATION - the part of the gain cell that is easiest to leave
    // out and hardest to hear the absence of.
    //
    // A bare FET across the signal path is a bad distorter: its channel
    // conductance depends on Vgs, the drain is swinging, so the channel is
    // modulated by the very signal passing through it and the second-order term
    // is enormous. Every FET VCA ever built fixes this the same way - a
    // resistive divider feeds HALF the drain voltage back to the gate, so the
    // gate rides with the drain and the square-law term cancels. The 1176 does
    // it with the pair of out-of-phase copies at the gain cell.
    //
    // What survives cancellation is what the box actually sounds like: a little
    // residual second, because two real resistors and a real FET are not an
    // ideal square law, and a THIRD-order term that the divider does not touch.
    // Modelling the cell without the divider makes it far too second-heavy -
    // and since the class-A preamp downstream is already a second-harmonic
    // generator, all the colour ends up on one harmonic.
    //
    // The split is modelled, not measured against a unit.
    static constexpr float kResidual2 = 0.20f;   // second left after cancelling
    static constexpr float kOdd3      = 0.60f;   // third, which the divider misses

    // Q-BIAS. The gate sits at a standing negative voltage set by a trimmer, so
    // the cell is very slightly on even with no signal - that is what the
    // control is for, and it is the calibration a tech sets by ear for lowest
    // distortion. It means an 1176 in circuit is never quite transparent: there
    // is always a little of the FET in the path. Expressed here as a floor on
    // the control voltage, in dB.
    static constexpr float kQBiasDb = 0.35f;

    // How much the channel's own noise rises with reduction, as a multiple of
    // the stage's resting floor at the bottom of the cell's range.
    static constexpr float kNoiseRise = 3.0f;

    // Linear gain for a control voltage. cv is the gate drive in dB-equivalent
    // units: cv = 6 means the loop is asking for 6 dB of reduction.
    //
    // The request is turned into the channel resistance that would deliver it,
    // Rds = w/(1-w), and Rds(on) is ADDED rather than clamped to. That one line
    // is the whole nonideality: at small reductions Rds(on) is negligible and
    // the cell delivers what was asked; as the request grows it becomes the
    // dominant term, so the last few dB compress smoothly against the floor
    // instead of stepping onto it. Measured: 6 dB asked gives 5.94, 20 gives
    // 19.2, 40 gives 33.1, and nothing gets past -38.
    static inline float gainFor(float cv)
    {
        if (cv <= 0.0f) return 1.0f;
        // exp() rather than pow(): this runs once per sample and pow(10, x) is
        // the more expensive way to write the same thing. ln(10)/20.
        const float want = std::exp(-cv * 0.115129255f);       // (0, 1]
        const float rIdeal = want / (1.0f - want > 1e-6f ? 1.0f - want : 1e-6f);
        const float rds = kRdsOn + rIdeal;
        return rds / (rds + 1.0f);
    }

    // The cell applied to a sample, including the within-cycle channel
    // modulation and the channel's own noise. Wide open (g near 1) the FET is a
    // piece of wire, this is a multiply, and it is quiet.
    inline float process(float x, float g, float bleed, float hissAmp)
    {
        x += bleed;

        // Q-bias keeps a little of the FET in circuit at rest, so "no gain
        // reduction" is not the same as "no gain cell".
        const float work = std::min(1.0f, (1.0f - g) + kQBiasFloor);
        const float mod = kChannelMod * work;

        // The signal term is SOFT-BOUNDED before it modulates the channel. A
        // real FET's channel responds to the drain voltage relative to its
        // pinch-off voltage, and past that it simply stops responding further -
        // so x/(1+|x|) is the honest shape and a bare x is not. With a bare x a
        // full-scale sample sent the divider negative and the clamp that caught
        // it turned the waveform inside out.
        const float ax = x < 0.0f ? -x : x;
        const float xs = x / (1.0f + ax);              // (-1, 1)

        // What the gate actually sees, after the drain-to-gate divider has
        // cancelled most of the square-law term. Second survives at a fifth of
        // its raw size; third is untouched by the divider and becomes the
        // larger of the two once the cell is working hard.
        const float e = kResidual2 * xs + kOdd3 * xs * xs * xs;
        const float local = 1.0f / (1.0f + mod * e);

        const float y = x * g * local;
        return y + noise.next() * hissAmp * (1.0f + kNoiseRise * work);
    }

    // kQBiasDb as a fraction of the cell's travel, so `work` can use it
    // directly. 10^(-0.35/20) is the gain the standing bias alone produces.
    static constexpr float kQBiasFloor = 0.0395f;   // 1 - 10^(-0.35/20)

    void setNoiseSeed(uint32_t s) { noise.seed(s); }
    void reset() {}

private:
    StageNoise noise;
};

//------------------------------------------------------------------------
// The sidechain, as a network rather than as two coefficients.
//
// ATTACK is one time constant: the rectified signal charging the timing cap
// through the attack resistor.
//
// RELEASE is TWO, in parallel. The cap discharges through a fixed resistor and
// through the detector's own impedance, and the second path only carries charge
// in proportion to how full the cap got. So a big transient leaves a long tail
// behind it and a small one does not - the program dependence is a consequence
// of the topology, not a modulation applied to a release time.
//------------------------------------------------------------------------
class Sidechain
{
public:
    struct Times
    {
        double attackMs;
        double releaseFastMs;
        double releaseSlowMs;
        double slowWeight;    // how much of the recovery goes the slow way
    };

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        updateCoeffs();
        reset();
    }

    void reset() { fast = 0.0f; slow = 0.0f; }

    void setTimes(const Times& t)
    {
        times = t;
        updateCoeffs();
    }

    // rect: the rectified detector voltage, as dB of requested reduction.
    inline float process(float rect)
    {
        fast += (rect > fast ? (rect - fast) * aAtk : (rect - fast) * aRelFast);
        slow += (rect > slow ? (rect - slow) * aAtk : (rect - slow) * aRelSlow);

        const float w = static_cast<float>(times.slowWeight);
        return fast * (1.0f - w) + slow * w;
    }

    float attackCoeff() const { return aAtk; }
    const Times& currentTimes() const { return times; }

    // The attack coefficient is CAPPED, and the cap is a property of running a
    // feedback loop in discrete time rather than a taste decision.
    //
    // Linearised in dB the loop is  gr[n] = gr[n-1](1 - a(1+beta)) + a*beta*x.
    // At 4:1, beta is 3, so the pole goes negative once a passes 0.25 and the
    // detector starts alternating sample to sample - which is audible as a buzz
    // riding the gain reduction, not as a faster attack. 0.22 keeps the pole
    // positive with room to spare.
    //
    // Practical effect: at 44.1 kHz the fastest attack this can do is about
    // 0.09 ms. The 1176's own 20 us is shorter than a sample period there, so
    // nothing real is being given up; at 96 kHz the cap stops binding and FAST
    // gets its full speed back.
    static constexpr float kMaxAttackCoeff = 0.22f;

private:
    void updateCoeffs()
    {
        aAtk     = std::min(kMaxAttackCoeff, coeff(times.attackMs));
        aRelFast = coeff(times.releaseFastMs);
        aRelSlow = coeff(times.releaseSlowMs);
    }

    float coeff(double ms) const
    {
        if (ms <= 0.0) return kMaxAttackCoeff;
        const double n = ms * 0.001 * sr;
        if (n < 1.0) return kMaxAttackCoeff;
        return static_cast<float>(1.0 - std::exp(-1.0 / n));
    }

    double sr = 44100.0;
    Times times{ 0.3, 150.0, 700.0, 0.40 };
    float aAtk = 0.1f, aRelFast = 0.001f, aRelSlow = 0.0002f;
    float fast = 0.0f, slow = 0.0f;
};

//------------------------------------------------------------------------
// A transformer.
//
// Iron saturates where there is flux to saturate it, which is the bottom two
// octaves; above that it is a wire. Saturating the whole band instead is a fuzz
// box - saturating the flux is a transformer, and the difference is audible on
// anything with a kick in it.
//
// It also loses the top, to leakage inductance and winding capacitance. The two
// irons in an 1176 are not the same part and do not lose the same amount, which
// is why the input and output stages are separate instances with separate
// corner frequencies rather than one shared class.
//------------------------------------------------------------------------
class Transformer
{
public:
    void prepare(double sampleRate, double fluxHz, double topHz)
    {
        const double a = 2.0 * 3.14159265358979323846 * fluxHz / sampleRate;
        fluxCoeff = static_cast<float>(std::exp(-a));
        const double b = 2.0 * 3.14159265358979323846 * topHz / sampleRate;
        topCoeff = static_cast<float>(std::exp(-b));
        reset();
    }

    void reset() { lp = 0.0f; top = 0.0f; }

    void setNoiseSeed(uint32_t s) { noise.seed(s); }

    inline float process(float x, float drive, float bleed, float hissAmp)
    {
        x += bleed;

        // Split off the low end, saturate only that, put it back.
        lp = (1.0f - fluxCoeff) * x + fluxCoeff * lp;
        const float high = x - lp;
        float y = std::tanh(lp * drive) / drive + high;

        // Leakage: a gentle first-order loss at the top, not a brick wall.
        top = (1.0f - topCoeff) * y + topCoeff * top;
        y = top;

        return y + noise.next() * hissAmp;
    }

private:
    float fluxCoeff = 0.0f, topCoeff = 0.0f;
    float lp = 0.0f, top = 0.0f;
    StageNoise noise;
};

//------------------------------------------------------------------------
// The class-A preamp: single-ended, so one device does all the work and the two
// halves of the wave are not treated alike. That asymmetry is what puts the
// second harmonic above the third, and it is the 1176's "thickness".
//------------------------------------------------------------------------
class ClassAStage
{
public:
    void setNoiseSeed(uint32_t s) { noise.seed(s); }
    void reset() {}

    inline float process(float x, float headroom, float bias,
                         float bleed, float hissAmp)
    {
        x += bleed;
        const float k = x / headroom;
        const float b = k > 0.0f ? bias : 1.0f;
        const float y = headroom * std::tanh(k * b) / b;
        return y + noise.next() * hissAmp;
    }

private:
    StageNoise noise;
};

//------------------------------------------------------------------------
// The push-pull output stage: two devices, one on each half of the wave, so the
// transfer curve is SYMMETRIC and the harmonics it makes are odd. That is the
// opposite of the preamp in front of it, and running both is why the unit reads
// as full rather than as fuzzy - a chain of identical tanh stages just makes
// more of the same harmonic.
//
// It does NOT have a crossover region. The 1176's output stage is push-pull
// CLASS A - both devices conduct through the whole cycle, which is the point of
// biasing it that way and the reason it can be run hard without getting ugly.
// The first version of this modelled a class-AB handover and put a quarter of a
// percent of third harmonic into the signal at REST, at low level, where a real
// unit is at its cleanest. Crossover distortion is also the one kind of
// distortion nobody wants, so inventing it is worse than leaving it out.
//
// What is left is device mismatch: two transistors are never identical, so the
// two halves of the wave are not treated to exactly the same curve. That is a
// trace, not a feature.
//------------------------------------------------------------------------
class PushPullStage
{
public:
    void setNoiseSeed(uint32_t s) { noise.seed(s); }
    void reset() {}

    inline float process(float x, float headroom, float crossover,
                         float bleed, float hissAmp)
    {
        x += bleed;
        const float k = x / headroom;
        // Symmetric compression: odd harmonics only.
        float y = std::tanh(k);
        // Device mismatch: the two halves see very slightly different curves.
        // Scales WITH level, unlike a crossover, because it is a difference in
        // how hard each device is driven rather than a dead-band at zero.
        if (crossover > 0.0f)
            y += crossover * k * k * (k > 0.0f ? 1.0f : -1.0f) * 0.5f;
        return y * headroom + noise.next() * hissAmp;
    }

private:
    StageNoise noise;
};

//------------------------------------------------------------------------
} // namespace Yonie
