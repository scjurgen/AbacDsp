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
| `plugintype` | yes | JUCE plugin type string (e.g. `"Gain"`, `"ReverbPlugin"`, `"InstrumentPlugin"`) |
| `description` | yes | Array of strings joined with `\n` for the about text |
| `patches` | no | `true` enables the named-patch browser (save/load/rename dialogs, `Settings > Patches` menu) |
| `host_transport` | no | `true` enables host-transport sync support (`HOSTTRANSPORT` template section) |
| `protected_files` | no | Array of relative paths the generator must not overwrite (e.g. already-customised files) |
| `extra_ui_includes` | no | Array of `#include` paths injected into the Editor header |
| `extra_timer_callbacks` | no | Array of C++ statement strings appended to the 60 Hz UI timer callback |
| `extra_processor_methods` | no | Array of C++ method definition strings injected into the Processor class |
| `processor_forwards` | no | Array of declarative pass-through Processor accessors — see [`processor_forwards`](#processor_forwards) below |

Note: `displayname`, `shortdescription`, `status`, `major`/`minor`/`micro`, `color`, and the
`midi-ports-in`/`midi-ports-out`/`audio-ports-in`/`audio-ports-out`/`transport` meta-sections were
removed from the schema (checked against the generator: none of them were read anywhere). Version
is currently always `"0.0.0"`; theming is handled entirely by the runtime theme system
(`AppSettings::loadTheme()`), not a per-blueprint colour field.

---

## `processor_forwards`

Declarative alternative to `extra_processor_methods` for simple pass-through accessors: the
generated method returns `pluginRunner-><call>()` (or a fallback when there is no runner yet),
without hand-writing the null-check boilerplate.

```json
"processor_forwards": [
  {"name": "isRecording", "type": "bool"},
  {"name": "getBarBeats", "type": "int", "default": 4},
  {"name": "getCurrentClickBpm", "call": "currentClickBpm", "type": "float", "default": "120.f"},
  {"name": "getProcessingBinLevels", "type": "std::array<float, 51>", "noexcept": true}
]
```

| Field | Required | Description |
|---|---|---|
| `name` | yes | Generated method name |
| `type` | yes | Return type |
| `call` | no | Underlying `pluginRunner` method name, if different from `name` (defaults to `name`) |
| `default` | no | Fallback value when `pluginRunner` is null (defaults to `false` for `bool`, `0`/`0.f`/`0u` for numeric types, `<type>{}` otherwise) |
| `noexcept` | no | Force `noexcept` on or off; defaults to `true` for `bool`/`int`/`float`/`double`/`size_t`/`uint32_t`/`int64_t`, `false` otherwise |

For `bool` with no explicit `default`, the body is `pluginRunner && pluginRunner-><call>()` (the
common "false when not running" idiom) instead of a ternary.

Anything that doesn't fit this shape — a method taking parameters, or one returning a reference
with a static-local fallback — stays a hand-written entry in `extra_processor_methods`.

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

`count` also works on `switch` items, with the same `{}` placeholder expansion.

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

Add `"signed": false` to cast the setter's parameter to `size_t` instead of the default `int`.

### type: `switch`

Toggle button mapped to an `AudioParameterBool`. The setter receives a `bool`.

```json
{"short": "ONF", "type": "switch", "display": "Power", "symbol": "onOff", "default": 0}
```

| Field | Description |
|---|---|
| `momentary` | Widget is a `MomentaryToggleButton` instead of `juce::ToggleButton`: holds its visual on-state for a minimum time regardless of trigger source (click, preset load, host automation), instead of flashing for a single timer tick. Use for pulse/trigger-style controls (record, clear, freeze, ...). The generator auto-emits a `tickFlash()` call in the timer callback. |
| `state_label` | `{"query": "<ProcessorMethod>", "on": "<text>", "off": "<text>"}` — auto-emits `<symbol>Switch.setButtonText(processorRef.<query>() ? "<on>" : "<off>");` each timer tick. Only fits a plain binary label swap; anything more complex (a three-way state, an interpolated count) still goes in `extra_timer_callbacks`. |

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
| `iris` | *(none)* | `customtype` required — used only to tag theme-callback dispatch (`setGradientPreset`) |
| `processingbins` | *(none)* | `customtype` required — used only to tag theme-callback dispatch (`updateColors`) |

Add `"customtype": "MyWidgetClass"` to substitute a hand-written widget class for the default.
Required for `iris` and `processingbins`, which have no built-in default widget.

Add `"bindings"` to wire additional per-frame setter calls without hand-writing them in
`extra_timer_callbacks`:

```json
{"short": "SLICE", "type": "gauge", "gaugetype": "signal", "display": "Loop", "symbol": "slice",
  "customtype": "SliceWaveDisplay",
  "bindings": [
    {"call": "setSampleRate", "from": "getSampleRate", "cast": "float"},
    {"call": "setLoopWaveform", "from": "getLoopWaveform"}
  ]}
```

Each entry emits `<symbol>Gauge.<call>(processorRef.<from>());`, optionally wrapped in
`static_cast<cast>(...)`. Only fits a zero-argument Processor getter feeding directly into one
setter call; anything needing a locally-computed value (e.g. derived from two getters) stays in
`extra_timer_callbacks`.

### type: `label`

Static text divider — renders as a `juce::Label`, no parameter.

```json
{"short": "DIV", "type": "label", "display": "--- Section ---", "symbol": "div1"}
```

---

## MIDI CC mapping

Add `"cc"` to a `dial` or `switch` item to make it MIDI-learnable and CC-automatable:

```json
{"short": "GAIN", "type": "dial", "display": "Gain", "symbol": "gain", "default": 0,
 "range": [-60, 60, 0.1, 1, false], "precision": 1, "unit": "dB",
 "cc": {"controller": 20, "valueLow": -20, "valueHigh": 10}}
```

| Field | Required | Description |
|---|---|---|
| `controller` | yes | MIDI CC number (0-127) the control listens on |
| `valueLow` | no | Parameter value at CC 0 (defaults to the dial's `rangeStart`, or `0` for a switch) |
| `valueHigh` | no | Parameter value at CC 127 (defaults to the dial's `rangeEnd`, or `1` for a switch) |

The dial gets a right-click menu (MIDI Learn, Set CC Range, Clear CC Assignment) via
`CustomRotaryDial::setCcMappable`. Switches map like sustain/damper pedals: the CC's 0..127 value
is scaled across the parameter's 0..1 range, so a bool flips at the 63/64 split automatically — no
extra widget wiring is generated for them. `NUM_CC_TARGETS` and the `CcTarget` enum (used by
`extra_ui_includes`/hand-written code to reference a specific mapped control) are generated from
every `dial`/`switch` item that has a `"cc"` field.

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
| `src/impl/CcMapping.h` | yes | never |
| `src/impl/CcSettings.h` | yes | never |
| `src/UiElements.h` | yes | never |
| `src/impl/<Name>Impl.h` | **no** | your DSP implementation |
| `CMakeLists.txt` | first run only | safe to extend |
| `src/unittests/CMakeLists.txt` | first run only | safe to extend |

Files listed in `"protected_files": []` are skipped even on re-runs.
