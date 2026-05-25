# Blueprint Format Reference

A blueprint is a single JSON file in `blueprints/<name>.json` that fully describes a JUCE
standalone plugin. Run the generator from `JuceStandaloneGenerator/`:

```bash
python3 generate-juce-standalone.py <name>
# output → ../examples/<name>/src/
```

---

## Top-level fields

| Field | Required | Description |
|---|---|---|
| `name` | yes | Module name — used as directory name and C++ identifier prefix |
| `displayname` | no | Human-readable name shown in the UI title bar (defaults to `name`) |
| `plugintype` | yes | JUCE plugin type string (e.g. `"Gain"`, `"ReverbPlugin"`, `"InstrumentPlugin"`) |
| `color` | no | Theme colour — named preset (`"Chorus"`, `"Analyser"`) or hex `"#rrggbb"` |
| `shortdescription` | no | One-line description |
| `description` | yes | Array of strings joined with `\n` for the about text |
| `status` | no | Freeform tag, e.g. `"beta"` |
| `major` / `minor` / `micro` | no | Semantic version numbers (auto-incremented by generator if omitted) |
| `protected_files` | no | Array of relative paths the generator must not overwrite (e.g. already-customised files) |
| `extra_ui_includes` | no | Array of `#include` paths injected into the Editor header |
| `extra_timer_callbacks` | no | Array of C++ statement strings appended to the 60 Hz UI timer callback |
| `extra_processor_methods` | no | Array of C++ method definition strings injected into the Processor class |

### Optional port meta-sections

These are informational / used by some templates; usually omitted for basic plugins.

```json
"midi-ports-in":  [{"name": "MidiIn",  "symbol": "midiIn"}],
"midi-ports-out": [{"name": "MidiOut", "symbol": "midiOut"}],
"audio-ports-in": [{"name": "In L", "symbol": "in[0]"}, {"name": "In R", "symbol": "in[1]"}],
"audio-ports-out":[{"name": "Out L","symbol": "out[0]"},{"name": "Out R","symbol": "out[1]"}],
"transport":      [{"name": "transport", "symbol": "transport"}]
```

---

## `ports-control` entries

Every entry has at minimum:

| Field | Description |
|---|---|
| `short` | Short label shown on the widget (≤ 6 chars) |
| `type` | Control type — see below |
| `display` | Full label text |
| `symbol` | C++ identifier used for the parameter and generated setter (`set<Symbol>`) |
| `default` | Default value |

### type: `dial`

Rotary knob mapped to an `AudioParameterFloat`.

```json
{"short": "GAIN", "type": "dial", "display": "Gain", "symbol": "gain",
 "default": 0, "range": [-60, 60, 0.1, 1, false], "precision": 1, "unit": "dB"}
```

| Field | Description |
|---|---|
| `range` | `[min, max, interval, skewFactor, useSymmetricSkew]` — skew < 1 expands the low end |
| `precision` | Decimal places in the value label |
| `unit` | Unit suffix appended to the value string |
| `count` | Expands to `count` separate controls; use `{}` in `short`, `display`, `symbol` as a 1-based index placeholder |

When `unit` is `dB` / `db` / `DB`, `GenericImpl` auto-converts the value with `pow(10, v/20)`.

### type: `drop`

Combo-box mapped to an `AudioParameterChoice`. The setter receives a `size_t` index.

```json
{"short": "PRE", "type": "drop", "display": "Preset", "symbol": "preset",
 "default": 0, "listitems": ["Alpha", "Beta", "Gamma"]}
```

`listitems` accepts two forms:

| Form | Example | Result |
|---|---|---|
| Array | `["A", "B", "C"]` | Fixed list |
| Template string + `count` | `"listitems": "Patch {}", "count": 8` | "Patch 1" … "Patch 8" |
| `#{}` template + `count` | `"listitems": "#{}", "count": 5` | "#1" … "#5" |

Add `"patch": true` to mark a drop as a **patch selector**: changing it triggers a
save-before-switching dialog and updates `m_patchIndex`.

### type: `switch`

Toggle button mapped to an `AudioParameterBool`. The setter receives a `bool`.

```json
{"short": "ONF", "type": "switch", "display": "Power", "symbol": "onOff", "default": 0}
```

### type: `gauge`

Read-only display widget — no JUCE parameter is created.

```json
{"short": "LVL", "type": "gauge", "gaugetype": "levels",      "display": "Level",       "symbol": "level"}
{"short": "CPU", "type": "gauge", "gaugetype": "cpuload",     "display": "CPU",         "symbol": "cpu"}
{"short": "FFT", "type": "gauge", "gaugetype": "spectrogram", "display": "Spectrogram", "symbol": "spectrogram"}
{"short": "SIG", "type": "gauge", "gaugetype": "signal",      "display": "Beat",        "symbol": "signal"}
```

| `gaugetype` | Widget class | Data source |
|---|---|---|
| `levels` | `Gauge` (VU meter) | `processorRef.getInputDbLoad()` / `getOutputDbLoad()` |
| `cpuload` | `CpuGauge` | `processorRef.getCpuLoad()` |
| `spectrogram` | `SpectrogramDisplay` | `processorRef.getSpectrogram()` |
| `signal` | `WaveformGauge` | `processorRef.getWaveDataToShow()` |

Add `"customtype": "MyWidgetClass"` to substitute a hand-written widget class for the default.

### type: `label`

Static text divider — renders as a `juce::Label`, no parameter.

```json
{"short": "DIV", "type": "label", "display": "--- Section ---", "symbol": "div1"}
```

---

## Conditional visibility

Any control can be shown/hidden dynamically:

```json
{"short": "SWG", "type": "dial", "symbol": "swingRatio", ...,
 "visible_when": "processorRef.presetHasSwing(presetDrop.getSelectedItemIndex())",
 "depends_on": "preset"}
```

| Field | Description |
|---|---|
| `visible_when` | C++ boolean expression evaluated in the Editor; may call processor methods or read other widget state |
| `depends_on` | Symbol of the control whose `onChange` triggers a visibility refresh |

The generator emits an `updateSwingRatioVisibility()` helper and wires it to `presetDrop.onChange`.

---

## `layout`

```json
"layout": {
  "WINDOW_WIDTH": "960",
  "WINDOW_HEIGHT": "660",
  "type": "≡",
  "composition": "R=(1:1:4) A1=(SUB*120, PRE*200, DRP, SWG, ONF, BPM) A2=(SVOL, MVOL, IVOL) A3=(SIG)"
}
```

### `type` — split direction

| Value | Meaning |
|---|---|
| `"|||"` | Three equal vertical columns |
| `"\|\|"` | Two equal vertical columns |
| `"\|=N"` | N vertical columns (widths set by `C=` ratios) |
| `"≡"` | Horizontal rows |

### `composition` syntax

Space-separated tokens:

| Token | Meaning |
|---|---|
| `C=(r1:r2:…)` | Column width ratios |
| `R=(r1:r2:…)` | Row height ratios |
| `A1=(…)` `A2=(…)` | Control lists for each area, in order |

Inside an area list, each entry is a `SHORT` code optionally followed by a size modifier:

| Modifier | Meaning |
|---|---|
| `SHORT*N` | Override height to N pixels |
| `SHORT%N` | Set height as a fraction (1 = full area height) |
| _(none)_ | Equal share of remaining height |

Example: `A1=(LVL*500, CPU*200, BPM)` — LVL gets 500 px, CPU gets 200 px, BPM fills the rest.

---

## `CPP` block

```json
"CPP": {
  "CLASS_NAME": "MetronomeImpl<NumSamplesPerBlock>",
  "INCLUDE_PEDAL": "impl/MetronomeImpl.h",
  "PROCMODE": "Stereo-Stereo"
}
```

| Field | Description |
|---|---|
| `CLASS_NAME` | Fully-qualified implementation class (template parameter `NumSamplesPerBlock` is always available) |
| `INCLUDE_PEDAL` | Path to the hand-written implementation header, relative to `src/` |
| `PROCMODE` | Audio routing — see table below |

### `PROCMODE` values

| Value | Impl method called | Notes |
|---|---|---|
| `"Stereo-Stereo"` | `processBlock(in, out)` | Default; two-channel in and out |
| `"Mono-Stereo"` | `processBlockMonoStereo(in, outL, outR)` | |
| `"Mono-Mono"` | `processBlockMonoMono(in, out)` | Output copied to both channels |
| `"Midi-Stereo"` | `processBlockMidiStereo(outL, outR)` | No audio input |

---

## Generated vs. hand-written files

| File | Generated | Edit? |
|---|---|---|
| `src/<Name>Processor.h` | yes | never |
| `src/<Name>Processor.cpp` | yes | never |
| `src/<Name>Editor.h` | yes | never |
| `src/<Name>Constants.h` | yes | never |
| `src/impl/PatchParameters.h` | yes | never |
| `src/impl/GenericImpl.h` | yes | never (reference stub only) |
| `src/impl/FileIo.h` | yes | never |
| `src/impl/<Name>Impl.h` | **no** | your DSP implementation |
| `CMakeLists.txt` | first run only | safe to extend |
| `src/unittests/CMakeLists.txt` | first run only | safe to extend |

Files listed in `"protected_files": []` are skipped even on re-runs.
