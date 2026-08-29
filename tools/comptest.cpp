//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//
// comptest - measures CompEngine directly, with no plugin and no host.
//
// Compiles against the same source the plugin uses, so a timing constant that
// does not do what its name says shows up here in a second rather than after a
// full VST3 build and a hand test in a DAW.
//
// What it measures:
//   * the static transfer curve - in dBFS against out dBFS - which is where the
//     threshold, the 4:1 slope and the FET's -38 dB floor all have to appear
//     without any of them being written into the signal path as a number;
//   * gain reduction against time for each of the three buttons, from a step,
//     which is the only honest way to show attack and a two-constant release;
//   * harmonic distortion against gain reduction, which on a FET compressor has
//     to RISE with the reduction, because the device doing both is the same;
//   * the noise floor and the channel crosstalk.
//
// Build (from the plugin repo root):
//   cl /EHsc /O2 /std:c++17 /I WetCompressor/source tools/comptest.cpp ^
//      WetCompressor/source/compengine.cpp
//
// Pass --json <path> to write the numbers out for the specification sheet.
//------------------------------------------------------------------------

#include "compengine.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Yonie;

namespace {

constexpr double kPI = 3.14159265358979323846;
constexpr double kRate = 44100.0;
constexpr int    kBlock = 512;

const char* kModeName[3] = { "FAST", "NORMAL", "SLOW" };

//------------------------------------------------------------------------
double dB(double lin) { return 20.0 * std::log10(lin > 1e-12 ? lin : 1e-12); }
double lin(double db) { return std::pow(10.0, db / 20.0); }

//------------------------------------------------------------------------
// Goertzel magnitude at one frequency, over a window that starts after the
// settling period.
double magnitudeAt(const std::vector<float>& x, double freq, int start)
{
    const int n = static_cast<int>(x.size()) - start;
    if (n <= 0) return 0.0;
    const double coeff = 2.0 * std::cos(2.0 * kPI * freq / kRate);
    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double s0 = x[start + i] + coeff * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return 2.0 * std::sqrt(power > 0.0 ? power : 0.0) / n;
}

//------------------------------------------------------------------------
// Run the engine over a signal built by gen(), returning the left output.
template <typename Gen>
std::vector<float> run(CompEngine& eng, int samples, Gen gen)
{
    std::vector<float> outL(samples, 0.0f), outR(samples, 0.0f);
    std::vector<float> inL(kBlock), inR(kBlock);

    for (int base = 0; base < samples; base += kBlock)
    {
        const int n = std::min(kBlock, samples - base);
        for (int i = 0; i < n; ++i)
        {
            const float v = gen(base + i);
            inL[i] = v;
            inR[i] = v;
        }
        eng.processStereo(inL.data(), inR.data(), &outL[base], &outR[base], n);
    }
    return outL;
}

//------------------------------------------------------------------------
struct Engine
{
    CompEngine eng;

    // A clean unit for measurement: the analog stages are what colour the
    // reading, so the curve is measured with them out and quoted with them in
    // where that is the point.
    Engine(int mode, bool analog)
    {
        CompEngine::Settings s;
        s.input = 0.5;      // 0 dB
        s.output = 0.5;     // 0 dB
        s.mode = mode;
        eng.prepare(kRate, kBlock);
        eng.setSettings(s);
        eng.setSaturationEnabled(analog);
        eng.setHissEnabled(analog);
        eng.setCrosstalkEnabled(analog);
        eng.reset();
    }
};

//------------------------------------------------------------------------
// The static transfer curve: a settled 1 kHz tone in, its own magnitude out.
std::vector<std::pair<double, double>> transferCurve(int mode, bool analog)
{
    std::vector<std::pair<double, double>> pts;
    for (double inDb = -60.0; inDb <= 12.001; inDb += 1.0)
    {
        Engine e(mode, analog);
        const double amp = lin(inDb);
        const int settle = static_cast<int>(kRate * 1.2);
        const int measure = static_cast<int>(kRate * 0.3);
        auto y = run(e.eng, settle + measure, [&](int i) {
            return static_cast<float>(amp * std::sin(2.0 * kPI * 1000.0 * i / kRate));
        });
        pts.emplace_back(inDb, dB(magnitudeAt(y, 1000.0, settle)));
    }
    return pts;
}

//------------------------------------------------------------------------
// Gain reduction against time, from a step: 200 ms of silence, 300 ms at
// -6 dBFS, then 1.5 s of silence again. Reported every millisecond.
//
// The tail has to be long enough for the SLOW setting to actually get back:
// at 400 ms fast and 1.5 s slow it has not fallen through 37% inside half a
// second, and a window that short reports "never released" for a unit that
// releases perfectly well.
std::vector<double> stepResponse(int mode)
{
    Engine e(mode, false);
    const int msTotal = 2000;
    std::vector<double> gr;
    gr.reserve(msTotal);

    const double amp = lin(-6.0);
    const int perMs = static_cast<int>(kRate / 1000.0);
    std::vector<float> inL(perMs), inR(perMs), outL(perMs), outR(perMs);

    for (int ms = 0; ms < msTotal; ++ms)
    {
        const bool on = ms >= 200 && ms < 500;   // 300 ms burst
        for (int i = 0; i < perMs; ++i)
        {
            const int n = ms * perMs + i;
            const float v = on
                ? static_cast<float>(amp * std::sin(2.0 * kPI * 220.0 * n / kRate))
                : 0.0f;
            inL[i] = v; inR[i] = v;
        }
        e.eng.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), perMs);
        gr.push_back(e.eng.gainReductionDb());
    }
    return gr;
}

//------------------------------------------------------------------------
// Attack, at sample resolution. All three settings are sub-millisecond, so the
// per-millisecond step trace cannot tell them apart; this feeds a hard step and
// asks how many samples the gain reduction takes to cross 63% of where it ends.
//
// The test signal is a SQUARE wave, not a sine. A rectified sine only reaches
// its peak a quarter of a cycle after it starts, and at 220 Hz that is 1.1 ms -
// longer than any of the three attack settings, so every one of them measures
// as "however long the tone took to get loud" and the three read alike. A
// square is at full level from its first sample, so what is left is the
// sidechain. This is a measurement rig, not a musical signal.
double attackMs(int mode)
{
    Engine e(mode, false);
    const double amp = lin(-6.0);
    const int n = 4096;
    std::vector<float> inL(n), inR(n), outL(n), outR(n);
    for (int i = 0; i < n; ++i)
    {
        const double ph = std::fmod(1000.0 * i / kRate, 1.0);
        const float v = static_cast<float>(ph < 0.5 ? amp : -amp);
        inL[i] = v; inR[i] = v;
    }
    // One sample at a time, so the block-rate GR readout does not quantise the
    // answer to the block length. Recorded first, then read: the target is
    // where this signal ends up, which is not the same number as where the
    // sine burst ends up.
    std::vector<double> gr(n);
    for (int i = 0; i < n; ++i)
    {
        e.eng.processStereo(&inL[i], &inR[i], &outL[i], &outR[i], 1);
        gr[i] = e.eng.gainReductionDb();
    }
    const double target = gr[n - 1];
    for (int i = 0; i < n; ++i)
        if (gr[i] >= 0.63 * target)
            return 1000.0 * i / kRate;
    return -1.0;
}

//------------------------------------------------------------------------
// THD against gain reduction. On a FET compressor the two have to move
// together: the device setting the gain is the device making the harmonics.
std::vector<std::pair<double, double>> thdVsReduction()
{
    std::vector<std::pair<double, double>> pts;
    for (double inDb = -24.0; inDb <= 6.001; inDb += 2.0)
    {
        Engine e(CompRange::kNormal, true);
        const double amp = lin(inDb);
        const int settle = static_cast<int>(kRate * 1.2);
        const int measure = static_cast<int>(kRate * 0.4);
        auto y = run(e.eng, settle + measure, [&](int i) {
            return static_cast<float>(amp * std::sin(2.0 * kPI * 220.0 * i / kRate));
        });
        const double f = magnitudeAt(y, 220.0, settle);
        double harm = 0.0;
        for (int h = 2; h <= 8; ++h)
        {
            const double m = magnitudeAt(y, 220.0 * h, settle);
            harm += m * m;
        }
        const double thd = f > 1e-9 ? std::sqrt(harm) / f : 0.0;
        pts.emplace_back(e.eng.gainReductionDb(), 100.0 * thd);
    }
    return pts;
}

//------------------------------------------------------------------------
double noiseFloorDb()
{
    Engine e(CompRange::kNormal, true);
    const int n = static_cast<int>(kRate * 2.0);
    auto y = run(e.eng, n, [](int) { return 0.0f; });
    double sum = 0.0;
    const int start = static_cast<int>(kRate * 0.5);
    for (int i = start; i < n; ++i) sum += double(y[i]) * y[i];
    return dB(std::sqrt(sum / (n - start)));
}

//------------------------------------------------------------------------
double crosstalkDb()
{
    CompEngine eng;
    CompEngine::Settings s;
    eng.prepare(kRate, kBlock);
    eng.setSettings(s);
    eng.setHissEnabled(false);          // isolate the bleed from the floor
    eng.reset();

    const int n = static_cast<int>(kRate * 1.0);
    std::vector<float> outL(n), outR(n), inL(kBlock), inR(kBlock);
    const double amp = lin(-20.0);
    for (int base = 0; base < n; base += kBlock)
    {
        const int c = std::min(kBlock, n - base);
        for (int i = 0; i < c; ++i)
        {
            inL[i] = static_cast<float>(amp * std::sin(2.0 * kPI * 1000.0 * (base + i) / kRate));
            inR[i] = 0.0f;              // silent channel: whatever arrives, leaked
        }
        eng.processStereo(inL.data(), inR.data(), &outL[base], &outR[base], c);
    }
    const int start = static_cast<int>(kRate * 0.4);
    const double driven = magnitudeAt(outL, 1000.0, start);
    const double leaked = magnitudeAt(outR, 1000.0, start);
    return dB(leaked / (driven > 1e-12 ? driven : 1e-12));
}

//------------------------------------------------------------------------
void writeJson(const char* path)
{
    FILE* f = std::fopen(path, "w");
    if (!f) { std::printf("could not write %s\n", path); return; }

    std::fprintf(f, "{\n  \"rate\": %.0f,\n  \"threshold_db\": %.1f,\n  \"ratio\": %.1f,\n",
                 kRate, CompEngine::kThresholdDb, CompEngine::kRatio);

    std::fprintf(f, "  \"transfer\": {\n");
    for (int m = 0; m < 3; ++m)
    {
        auto pts = transferCurve(m, false);
        std::fprintf(f, "    \"%s\": [", kModeName[m]);
        for (size_t i = 0; i < pts.size(); ++i)
            std::fprintf(f, "%s[%.1f,%.3f]", i ? "," : "", pts[i].first, pts[i].second);
        std::fprintf(f, "]%s\n", m < 2 ? "," : "");
    }
    std::fprintf(f, "  },\n");

    std::fprintf(f, "  \"step\": {\n");
    for (int m = 0; m < 3; ++m)
    {
        auto gr = stepResponse(m);
        std::fprintf(f, "    \"%s\": [", kModeName[m]);
        for (size_t i = 0; i < gr.size(); ++i)
            std::fprintf(f, "%s%.3f", i ? "," : "", gr[i]);
        std::fprintf(f, "]%s\n", m < 2 ? "," : "");
    }
    std::fprintf(f, "  },\n");

    std::fprintf(f, "  \"times\": {\n");
    for (int m = 0; m < 3; ++m)
    {
        const Sidechain::Times t = CompEngine::timesFor(m);
        std::fprintf(f, "    \"%s\": {\"attack_ms\": %.2f, \"release_fast_ms\": %.0f, "
                        "\"release_slow_ms\": %.0f, \"slow_weight\": %.2f}%s\n",
                     kModeName[m], t.attackMs, t.releaseFastMs, t.releaseSlowMs,
                     t.slowWeight, m < 2 ? "," : "");
    }
    std::fprintf(f, "  },\n");

    auto thd = thdVsReduction();
    std::fprintf(f, "  \"thd\": [");
    for (size_t i = 0; i < thd.size(); ++i)
        std::fprintf(f, "%s[%.2f,%.4f]", i ? "," : "", thd[i].first, thd[i].second);
    std::fprintf(f, "],\n");

    std::fprintf(f, "  \"noise_db\": %.1f,\n", noiseFloorDb());
    std::fprintf(f, "  \"crosstalk_db\": %.1f,\n", crosstalkDb());
    std::fprintf(f, "  \"max_reduction_db\": %.1f\n",
                 -20.0 * std::log10(FETGainCell::gainFor(200.0f)));
    std::fprintf(f, "}\n");
    std::fclose(f);
    std::printf("wrote %s\n", path);
}

} // namespace

//------------------------------------------------------------------------
int main(int argc, char** argv)
{
    const char* jsonPath = nullptr;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--json") == 0 && i + 1 < argc)
            jsonPath = argv[++i];

    std::printf("WetCompressor - CompEngine measurement\n");
    std::printf("=====================================\n");
    std::printf("rate %.0f Hz   threshold %.1f dBFS   closed-loop ratio %.1f:1\n\n",
                kRate, CompEngine::kThresholdDb, CompEngine::kRatio);

    //--- the curve --------------------------------------------------------
    std::printf("Static transfer, 1 kHz, analog stages out (NORMAL)\n");
    std::printf("   in dBFS    out dBFS    reduction    ratio in this decade\n");
    {
        auto pts = transferCurve(CompRange::kNormal, false);
        double prevIn = 0.0, prevOut = 0.0;
        bool have = false;
        for (size_t i = 0; i < pts.size(); i += 6)
        {
            const double in = pts[i].first, out = pts[i].second;
            char ratio[32] = "     -";
            if (have && out - prevOut > 1e-6)
                std::snprintf(ratio, sizeof(ratio), "%6.2f:1", (in - prevIn) / (out - prevOut));
            std::printf("  %7.1f    %8.2f    %8.2f    %s\n", in, out, in - out, ratio);
            prevIn = in; prevOut = out; have = true;
        }
    }

    //--- what the cell can do --------------------------------------------
    std::printf("\nGain cell ceiling: %.1f dB "
                "(the FET bottoms out; more INPUT past this buys distortion, not reduction)\n",
                -20.0 * std::log10(FETGainCell::gainFor(200.0f)));

    //--- the three buttons ------------------------------------------------
    std::printf("\nSidechain, per button\n");
    std::printf("   setting   attack     release fast   release slow   slow share\n");
    for (int m = 0; m < 3; ++m)
    {
        const Sidechain::Times t = CompEngine::timesFor(m);
        std::printf("   %-8s  %5.2f ms   %8.0f ms   %8.0f ms   %8.0f %%\n",
                    kModeName[m], t.attackMs, t.releaseFastMs, t.releaseSlowMs,
                    t.slowWeight * 100.0);
    }

    std::printf("\nStep response, -6 dBFS burst (time to reach / fall back through 63%%)\n");
    for (int m = 0; m < 3; ++m)
    {
        auto gr = stepResponse(m);
        double peak = 0.0;
        for (size_t i = 200; i < 500; ++i) peak = std::max(peak, gr[i]);
        int rel = -1;
        for (size_t i = 500; i < gr.size(); ++i)
            if (gr[i] <= 0.37 * peak) { rel = static_cast<int>(i) - 500; break; }

        // Attack is measured at SAMPLE resolution. All three settings are well
        // under a millisecond, so a per-millisecond trace reports 0 or 1 for
        // every one of them and three genuinely different settings read alike.
        const double atkMs = attackMs(m);

        if (rel < 0)
            std::printf("   %-8s  peak %5.2f dB   attack %6.3f ms   release >%4d ms\n",
                        kModeName[m], peak, atkMs, static_cast<int>(gr.size()) - 500);
        else
            std::printf("   %-8s  peak %5.2f dB   attack %6.3f ms   release %5d ms\n",
                        kModeName[m], peak, atkMs, rel);
    }

    //--- the analog stages ------------------------------------------------
    std::printf("\nDistortion against gain reduction (220 Hz, analog stages in)\n");
    std::printf("   reduction dB    THD %%\n");
    for (auto& p : thdVsReduction())
        std::printf("   %10.2f    %6.3f\n", p.first, p.second);

    std::printf("\nNoise floor      %6.1f dBFS  (four stages per channel, uncorrelated)\n",
                noiseFloorDb());
    std::printf("Channel crosstalk %6.1f dB\n", crosstalkDb());

    if (jsonPath)
    {
        std::printf("\n");
        writeJson(jsonPath);
    }
    return 0;
}
