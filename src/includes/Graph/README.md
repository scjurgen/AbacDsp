# Graph

A small audio-graph toolbox: a graph is described as data (usually Lua), checked, compiled once
into a fixed schedule, and then run block by block without allocation.

```
Lua text -> LuaGraphLoader -> GraphDescription -> GraphValidator -> GraphCompiler -> CompiledGraph
```

- `GraphDescription`, `GraphValidator`, `GraphCompiler`, `CompiledGraph`, `NodeRegistry`: the core,
  with no dependencies. Node types are registered by name with `registerStandardNodes`,
  `registerFilterNodes`, `registerControlNodes` and `registerTapeDelayNode<BlockSize>`.
- `Lua/LuaGraphLoader.h`: the Lua front end, built only with `WITH_LUA_GRAPH` (sol2 and Lua).
- `GraphSwapper`: replaces the running graph with a new one at a block boundary, with an
  equal-power crossfade. The audio thread never allocates or frees a graph.
- `GraphInspector`: DOT and JSON export, feedback cycles, macros, buffer layout, diagnostics text.
- `OfflineRender`: impulse, sine, sweep and noise stimuli, and a block-wise driver for a graph.
- `Presets/`: the chorus families below.

`TapeDelay` is compiled for one fixed block size and must be called with exactly that many samples.
The compiler ignores `macros`; `MacroLowering` rewrites them into nodes that read a `MacroBank`, so a
host only writes an atomic per macro (see Macros below).

## Macros

A macro is one control, declared in the script. Its value travels 0 to 1; `unit`, `min` and `max` are
what a control shows for that travel and `default` is in those units (all optional, 0 to 1 by default).
Each target names `node.param` and sweeps it from its own `min` to `max` as the control travels (the
parameter's range when both are left out); `curve = "exp"` makes the sweep geometric and needs a
positive `min` and `max`.

```lua
macros = {
  { id = "depth", label = "Depth", unit = "%", min = 0, max = 100, default = 65,
    targets = { { to = "tape.flutterDepth", min = 0, max = 1 },
                { to = "tape.transportRatio", min = 0.5, max = 2, curve = "exp" } } },
}
```

`MacroLowering::lower` turns each usable macro into a `MacroInput` node reading one `MacroBank` slot,
plus a `ScaleOffset` or `ExpMap` per target feeding a control edge. The graph then applies the values
itself on the audio thread; the host writes `MacroBank::set(slot, value)` from any thread and never
touches a node. A macro or target that cannot be lowered is skipped with a warning. A control edge
sets its parameter only when the value changes, because several nodes restart a smoothing ramp on
every set and would never settle on a steady value.

Every preset declares its macros, and a test checks that at their defaults they reproduce the
script's own parameters and that moving each one changes the output.

## Presets

Each preset is a Lua file in `Presets/lua/`. `Presets/ChorusPresets.h` is generated from them and
must not be edited by hand.

To add or change a preset: edit or add a `.lua` file (`NN_lower_snake_case.lua`), run
`dev-scripts/dev-generate-presets.sh`, then `dev-scripts/dev-test-lua.sh`. A test compares the header
with the files, so a forgotten regeneration fails. `dev-scripts/dev-generate-presets.sh --check`
does the same comparison without writing.

| Preset | Family | Measured effect |
|---|---|---|
| `tape_vibrato` | tape vibrato, one shared stereo tape | 21 cents peak at about 5 Hz, 47 samples of delay swing |
| `classic_stereo_chorus` | dual mono, independent seeds and rates | 104 samples (2.2 ms) swing |
| `shared_transport_heads` | two heads, same modulation | 117 samples (2.4 ms) swing per head |
| `ensemble_tri_chorus` | three independent voices, 1/sqrt(3) each | 114 to 130 samples per voice |
| `crossover_preserve_stereo` | LR4 split, stereo low band stays dry | null against the input to 3e-8 with an identity wet path |
| `crossover_mono_low` | LR4 split, low band folded to mono | mono fold-down preserved to 1.2e-7 |
| `crossover_wet_only_low_cut` | no split, wet bus high-passed | dry path untouched |
| `bbd_inspired` | short delay, 2-pole bandwidth limit, damped saturated feedback | 120 samples swing, 15 kHz 15.9 dB below 1 kHz, echoes at 20, 7 and 2.5 percent |
| `dimension` | two near-static taps folded to opposite sides | 0.8 cents pitch movement, mono input decorrelated to 0.68 with dry, 0.00 wet only |
| `experimental_resonant_feedback` | resonant band-pass in a saturated feedback loop | a loud burst rings 0.33 rms at 1 s, 0.001 at 5 s; output never leaves +-1 |

The effect sizes are asserted in `test/Graph/Lua/ChorusPresets_test.cpp`, not just the structure:
an effect can pass every structural check and still be inaudible. Render cost at block size 16 is
4 to 14 ms per second of audio (about 0.4 to 1.4 percent of one core), the ensemble being the most
expensive.

What sets the swing: `Wow` depth is cubed and limited to +-1 ms, while `Flutter` depth sets a speed
error of about 31 cents at 1.0 whatever the rate, so the delay swing grows with flutter depth and
falls with its rate.

## What is missing

What a family asks for and the node set cannot do, with what the preset does instead.

- Classic chorus: a 90 to 180 degree modulation offset between channels needs an external
  modulation input on `TapeDelay`. The channels use different seeds and rates instead.
- Shared-transport heads: one medium with several heads and per-head drift would be a new node.
  Two `TapeDelay` with the same seed move in lockstep instead, with no per-head drift.
- BBD: no expander to pair with `Compander`, no clock noise. Neither is modelled.
- Experimental feedback: no `SafetyLimiter` and no feedback meter. A `Saturator` (hard limit at +-1)
  bounds the loop and the output. The validator warns twice on purpose: the cycle has no damping
  filter and holds a resonant filter.
- The validator limits the gain of a feedback edge, but a `Gain` node inside a loop is not checked.
  The Lua loader reads only `from` and `to` on an audio edge, so `gain`, `polarity` and `label` there
  are ignored; the presets use `Gain` and `Matrix` nodes for level and polarity.
- Not present as nodes: `Pan`, `Noise`, `Multimode4`, an N-input `Mixer` (chain `Mixer` nodes).
- A graph swap resets the state of every node; migrating state needs a state interface on `Node`.
- Presets take their parameters from `TapeDelay` and friends as they are today: the runtime ignores
  a parameter or config key it does not know, and a test checks that no preset carries one.
