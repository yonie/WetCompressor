# Changelog

All notable changes to WetCompressor are recorded here. The published notes for
each release are on the [Releases page](https://github.com/yonie/WetCompressor/releases);
this file is the portable copy that travels with the source.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
this project uses [semantic versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.1] - 2026-09-30

### Fixed
- Linux: no more crash when the editor opens in Carla, or in any host that hands its event loop over through the plug-in window.
- Linux: the FAST, NORMAL and SLOW buttons work in Ardour and other hosts when the system writes decimals with a comma (German, Dutch, French and many more).

### Changed
- Nothing on Windows and macOS; the sound and saved settings are unchanged.

## [1.1.0] - 2026-09-26

### Added
- An Audio Unit for Logic and GarageBand, next to the VST3.
- Mono tracks: all four layouts (mono or stereo in, mono or stereo out). A mono
  input feeds both sides; a mono output is the average of left and right.

### Changed
- The macOS builds are signed and notarised by Apple.
- The sound is unchanged, and saved projects reload with the same settings.

## [1.0.1] - 2026-09-04

### Added
- Power user mode: hold Shift while dragging, scrolling or using the arrow keys
  for three times the resolution - 1 dB instead of 3 dB on the input and output
  gain.

### Changed
- Self-noise floor is 10 dB further down.

## [1.0.0] - 2026-08-30

Initial release.

### Added
- FET compressor with a feedback detector and a fixed 4:1 closed-loop ratio.
- Two stepped knobs, 21 detents each (61 positions with Shift), and three
  timings.
- Stereo input and output metering plus a gain-reduction strip.
- Full VST3 parameter automation.
- Modelled as the circuit: gain cell with drain-to-gate linearisation and a
  38 dB floor, distortion and noise that rise with gain reduction, input and
  output transformers, class-A preamp and push-pull output stage.
- -40 dB channel bleed distributed across all stages, and 2.5% component
  tolerance per channel.
