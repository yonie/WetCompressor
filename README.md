# WetCompressor

![VST3](https://img.shields.io/badge/VST3-Windows%20·%20macOS%20·%20Linux-blue)
![Licence](https://img.shields.io/badge/licence-MIT-green)
![Price](https://img.shields.io/badge/price-free-brightgreen)

A FET compressor modelled as the circuit: a feedback detector, a gain cell that
bottoms out, and four amplifiers that each colour what passes through them.

![WetCompressor panel](docs/panel.png)

## What it is

Two knobs and three buttons, because that is what the circuit has. There is no
threshold control and no ratio control — the threshold is a fixed property of
the circuit and INPUT is how far you drive the signal past it. More INPUT is
more compression, and OUTPUT puts back what that cost.

The detector is taken from the **output** of the gain cell rather than its
input, and almost everything the unit does follows from that one fact. The 4:1
is a closed-loop ratio, so it is set by loop gain and softens on its own near
the threshold — there is no knee parameter because the loop cannot correct what
it has not yet heard. The loop is also always one sample behind the signal,
which is why the leading edge of a transient gets through.

Both knobs are stepped, twenty-one positions, 3 dB apart.

## Sound

**It runs out of room, and that is the point.** The gain cell is a FET across
the signal path, and its channel resistance cannot fall below its own minimum —
so there is a hard floor at 38 dB of gain reduction. Past that, driving the
input harder stops buying compression and starts buying distortion.

**It distorts because it is compressing.** The device that sets the gain is the
device that makes the harmonics, so the two move together: 0.02% at rest, 2.9%
at 19 dB of reduction. A compressor with a saturator bolted after it does the
opposite — it colours hardest when it is working least.

**Four amplifiers, not one.** Input transformer, gain cell, single-ended class-A
preamp, push-pull class-A output into the output iron. The preamp is asymmetric
and makes second harmonic; the output pair is symmetric and makes third. Each
stage adds its own noise and leaks a little into the channel beside it, and the
two channels are built to ±2.5% component tolerance, so they are never quite the
same channel twice.

**Release runs two time constants at once**, in parallel, with the slow one
weighted by how deep the reduction went. A loud transient leaves a long tail
behind it; a quiet one does not. That program dependence comes out of the
network rather than being modulated onto a release time.

Nothing is resampled, quantised or band-limited.

![Specification](docs/curves.png)

## Controls

| | Range | Per detent |
|---|---|---|
| **INPUT** | ±30 dB, 21 positions | 3 dB |
| **OUTPUT** | ±30 dB, 21 positions | 3 dB |
| **FAST / NORMAL / SLOW** | attack and release together | three positions |

| | Attack | Release |
|---|---|---|
| **FAST** | 1 sample | 89 ms |
| **NORMAL** | 0.07 ms | 278 ms |
| **SLOW** | 0.18 ms | 819 ms |

All three attack settings are under a millisecond, as on the original — its
whole attack range is 20 to 800 µs. SLOW here is still faster than most
compressors' fastest.

| | |
|---|---|
| Ratio | 4:1 closed loop, softening to 3.7:1 as the cell runs out |
| Threshold | −20 dBFS, fixed |
| Maximum reduction | 38 dB |
| Detector | peak, after the gain cell, linked across the pair |
| Noise floor | −83.6 dBFS RMS |
| Crosstalk | −40 dB, distributed across all stages |
| Latency | 0 samples, any host rate |

Mouse wheel steps one detent per notch; shift drags finer; ctrl-click returns a
knob to 0 dB. Right-click the panel for **UI Zoom** — 75%, 100% or 125%.

## Install

Download the latest release from the [Releases page](https://github.com/yonie/WetCompressor/releases).
One download covers every platform.

| | |
|---|---|
| **Windows** | copy `WetCompressor.vst3` to `C:\Program Files\Common Files\VST3\` |
| **macOS** | copy `WetCompressor.vst3` to `~/Library/Audio/Plug-Ins/VST3/` |
| **Linux** | copy `WetCompressor.vst3` to `~/.vst3/` |

Rescan for plugins in your DAW. Intel and Apple Silicon are both in the macOS
build.

### macOS needs one more step

The build is unsigned, so macOS quarantines it and reports that the developer
cannot be verified. That does not mean the plugin is unsafe. Clear the flag
before you rescan:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/WetCompressor.vst3
```

Notarising a build means enrolling in the Apple Developer Program at $99 a year,
which a free MIT plugin does not pay for. The full source is in this repo if you
would rather build it yourself.

## Build it yourself

### Prerequisites

**Windows**
- Visual Studio 2022 Build Tools or Community Edition
- CMake 3.15 or higher, Git

**Linux (Ubuntu/Debian)**
```bash
sudo apt-get install cmake gcc g++ libstdc++6 libx11-xcb-dev libxcb-util-dev \
    libxcb-cursor-dev libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev \
    libfontconfig1-dev libcairo2-dev libgtkmm-3.0-dev libsqlite3-dev \
    libxcb-keysyms1-dev git
```

**macOS**
- Xcode Command Line Tools: `xcode-select --install`
- CMake 3.15+ (`brew install cmake`)

### Build steps

1. **Clone, then clone the VST3 SDK inside it:**
   ```bash
   git clone https://github.com/yonie/WetCompressor.git
   cd WetCompressor
   git clone --recursive https://github.com/steinbergmedia/vst3sdk.git
   ```

2. **Build:**
   - Windows: `build.bat`
   - Linux/macOS: `chmod +x build.sh && ./build.sh`

3. **Install:**
   - Windows: `install.bat`
   - Linux/macOS: `chmod +x install.sh && ./install.sh`

### Validation

The build runs the official Steinberg VST3 validator: 47 tests, all passing.

`tools/comptest.cpp` measures the DSP directly, with no plugin and no host:

- the static transfer curve, where the threshold, the 4:1 and the 38 dB floor
  all have to appear without any of them being written into the signal path as
  a number
- gain reduction against time for each of the three buttons, at sample
  resolution
- distortion against gain reduction, split into second and third harmonic
- the noise floor and the channel crosstalk

```
cl /EHsc /O2 /std:c++17 /I WetCompressor/source tools/comptest.cpp ^
   WetCompressor/source/compengine.cpp
```

## Licence

MIT Licence — Copyright © 2026 Ronald Klarenbeek (Yonie). See [LICENSE](LICENSE)
for the full text.

The VST3 SDK is licensed separately, under a BSD-style licence. See the SDK's own
licence files.

## Author

**Ronald Klarenbeek**
- Website: [https://wetvst.com](https://wetvst.com)
- Email: contact@wetvst.com
- GitHub: [https://github.com/yonie](https://github.com/yonie)

---

Part of **[WET](https://wetvst.com)** — with [WetDelay](https://github.com/yonie/WetDelay), [WetReverb](https://github.com/yonie/WetReverb) and [WetEQ](https://github.com/yonie/WetEQ)
