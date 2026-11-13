# Anjis Saturator

A multiband saturation and distortion plugin (VST3 / Standalone, plus AU on Mac) with a modulation system.

## Getting the plugin (no coding needed)

1. Create a new GitHub repository and upload everything in this folder, including the hidden `.github` folder.
2. Open the repo's **Actions** tab. The "Build Anjis Saturator" workflow starts by itself (or click **Run workflow**).
3. When it finishes (about 10–15 min), open the run and download **AnjisSaturator-Windows** (or **AnjisSaturator-macOS**).

### Install on Windows (FL Studio, Ableton, etc.)
- Unzip and copy the whole **`Anjis Saturator.vst3`** folder to `C:\Program Files\Common Files\VST3\`
- FL Studio: *Options → Manage plugins → Find more plugins*, then star it so it appears in your effects list.

### Install on Mac
- VST3: copy `Anjis Saturator.vst3` to `~/Library/Audio/Plug-Ins/VST3/`
- AU: copy `Anjis Saturator.component` to `~/Library/Audio/Plug-Ins/Components/`
- The build is unsigned. If macOS blocks it, run this in Terminal:
  `xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Anjis Saturator.vst3"`

## Features

**Bands and crossovers**
- 1 to 6 bands. Double-click the display to split, drag the lines to move crossovers, and right-click for slope (6/12/24/48 dB/oct).
- **Zero Latency** mode (analog-style crossovers that sum flat) or **Linear Phase** mode (phase-perfect, adds latency).
- Per band: Solo, Mute, Bypass, and Stereo / Mid / Side / Left / Right processing, plus Width.

**32 saturation styles** in 8 groups: Tube, Tape, Transformer, Amp, Clip, Fold, Digital and Dynamic.

**Per-band controls:** Drive, Feedback (with its own frequency), Dynamics, Tone, Mix, Level and Width.

**Modulation**
- 4 LFOs, including a 16-step mode where you draw the steps. They can sync to tempo and retrigger on notes.
- 2 envelopes, triggered by MIDI notes or by audio hits.
- 2 envelope followers, with a transient mode.
- 2 XY pads and 4 macro knobs.
- MIDI velocity, note, mod wheel, pitch bend and aftertouch.
- 50 routing slots. To add one, drag a modulation tab or chip onto any knob. Alt-drag a knob to change the amount, or use the Routing tab.
- Modulation is shown live: a ring around each modulated knob and a ghost line for modulated crossovers.

**Workflow**
- Oversampling (Eco, 2x, 4x, 8x, 16x, 32x).
- Auto Gain, which keeps loudness steady while you change Drive.
- Global Input, Mix and Output.
- Factory presets, your own presets and per-band presets. Copy or paste a band's settings.
- Undo/redo, A/B compare and MIDI Learn (right-click any knob).
- Resizable window (60–200%).
- A help bar at the bottom explains whatever is under the mouse.
- Animation: a spectrum analyzer, bands that pulse with the music, a live transfer curve with a ball that rides it, a scope, and LFO and envelope playheads.

User presets are saved in `%APPDATA%\Anji\Anjis Saturator\Presets` (Windows) or `~/Library/Anji/Anjis Saturator/Presets` (Mac).

## Building locally (developers)
```
cmake -B build
cmake --build build --config Release --target AnjisSaturator_VST3
```
JUCE 8 is downloaded automatically. Offline DSP tests: configure with `-DANJIS_BUILD_TESTS=ON` and run `AnjisTest`.
