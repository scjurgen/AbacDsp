# Max Diffuser

A diffuser (kind of reverb) where you can control a behaviour to extreme reflections.
The diffuser chain can be tweaked with several parameter ot obtain various effects that vary
from early reflection style of reverb to tsunami style of very slow building up reverberation.


## Purpose

| Use case | How |
|---|---|
| Small room effect | Low number of diffusers, slight short reverb |
| Tsunami | 30+ elements with range sizes 1...100m, add strong modulation to cause extrem noise and lowpass everything |
| Chorus effect | Few elements, slight Mod Depth, low Diffusion |
| Alien effects | Pitch and/or Pitch 2 with their own Pitch Delay, moderate Pitch Mix, low Diffusion |
| Simple tap delay | Few elements, slight Diffusion, low Tap Span so single echoes stay distinct |
| Washing out effects | Strong Mod Depth and Mod Speed to blur the chain into a smeared wash |
| Stereo widener / doubler | Few elements, low Diffusion, moderate Size Spread for decorrelated width without a reverb tail |
| Metallic ringing | Early Size and Late Size close together with high Diffusion, for pitched, comb-like resonance |
| Density ramp instead of decay | Set Early Size above Late Size so echoes get denser toward the tail instead of thinning out |
| Thicker tail without more elements | Raise Tap Span to blend several late elements into the output instead of adding more Elements |
| Ambient pad | Keep the diffuser subtle (few elements, low Diffusion) and lean on Reverb Mix/Size/Decay for a smoother, longer wash |

## Signal Flow

A drive/EQ-shaped distortion sits on the dry (unpitched) path only, both
pitch taps get a hi/lo shelf, Extreme Stereo Tap reweights each channel's
own tap-mix window, and the FDN reverb tail gets its own hi/lo shelving.

```mermaid
flowchart TD
    IN["Input L/R"]

    IN --> PRED["Pre Delay"]
    subgraph SHAPE["Distortion block (7 knobs)"]
        EQIN["EQ in: low/mid/high"]
        EQIN --> DIST["Distortion atanh(drive)"]
        DIST --> EQOUT["EQ out: low/mid/high"]
        EQOUT --> LEVEL["* Level"]
    end
    PRED --> EQIN
    LEVEL --> DRY["dryToDiffuser L/R"]

    IN --> PTDL["Pitch Delay (per ch)"] --> PT1["Pitcher 1 (per ch)"] --> PSHELF1["Hi/Lo Shelf (2 knobs)"] --> P1["pitch1Data L/R"]
    IN --> PTDL2["Pitch 2 Delay (per ch)"] --> PT2["Pitcher 2 (per ch)"] --> PSHELF2["Hi/Lo Shelf (same 2 knobs)"] --> P2["pitch2Data L/R"]

    DRY --> MIX["mix=(dry+pitch1+pitch2)/3"]
    P1 --> MIX
    P2 --> MIX

    MIX --> CHAIN["Diffuser Delay Chain (independent L/R)\nExtreme Stereo Tap off: tap window averaged in full\nExtreme Stereo Tap on: L keeps even-indexed taps, R keeps odd-indexed taps"]

    CHAIN --> WETPRE["wet L/R (pre-width)"]
    WETPRE --> WIDE["Wide (-100..100), effective only when\nExtreme Stereo Tap is on"]
    WIDE --> WETLR["wet L/R"]

    WETLR --> FSUM["sum/FdnOrder -> mono"] --> FDN["FDN Reverb Tank"] --> RSHELF["Reverb Hi/Lo Shelf (2 knobs)"]

    IN --> SUMOUT["out = dry*in + wet*wetLR + fdnMix*reverbShaped"]
    WETLR --> SUMOUT
    RSHELF --> SUMOUT
    SUMOUT --> OUT["Output L/R"]
```

Notes on the EQ/distortion block: it is a single 7-knob unit (EQ in, drive,
EQ out, level) sitting between Pre Delay and dryToDiffuser, on the dry path
only - the two pitch paths are never distorted. The pre/post EQ wraps only
the distortion itself, a "frown/smile" shaping pattern: boost frequencies
with EQ in before driving them into `atanh`, then use EQ out to restore
whatever the boost removed.

The Extreme Stereo Tap switch keeps the two independent L/R diffuser chains
and changes how each channel's own tap-mix window is weighted: off, every
active tap is averaged equally; on, channel L's window keeps only
even-indexed taps and channel R's keeps only odd-indexed taps (a pair of
complementary per-tap gain masks). Wide only has an effect in that mode.

The tap-mix parity gain masks (even-only vs. odd-only tap selection within
the existing Tap Span window) are covered by
`test/Diffuser/DiffusorDelayChain_test.cpp`.

## Controls

Grouped to match the plugin's layout: Pre Processing, Pitch, Diffuser, Reverb.

### Pre Processing

| Control | Range | Description |
|---|---|---|
| Dry | -100 - 12 dB | Level of the unprocessed input in the output |
| Wet | -100 - 12 dB | Level of the diffuser chain's output in the output |
| Pre Delay | 0 - 1000 ms | Delay before the dry (unpitched) signal enters the diffuser chain |
| Drive | 0 - 100% | Pre-gain into the dry path's atanh distortion stage; 0 is near-unity, higher values saturate harder |
| EQ In Low | -18 - 18 dB | Low-shelf gain (150 Hz) applied to the dry path just before the distortion stage |
| EQ In Mid | -18 - 18 dB | Peak gain (1 kHz) applied to the dry path just before the distortion stage |
| EQ In High | -18 - 18 dB | High-shelf gain (4 kHz) applied to the dry path just before the distortion stage |
| EQ Out Low | -18 - 18 dB | Low-shelf gain (150 Hz) applied to the dry path just after the distortion stage |
| EQ Out Mid | -18 - 18 dB | Peak gain (1 kHz) applied to the dry path just after the distortion stage |
| EQ Out High | -18 - 18 dB | High-shelf gain (4 kHz) applied to the dry path just after the distortion stage |
| Level | -24 - 12 dB | Output trim of the distortion block, applied after EQ Out |

### Pitch

| Control | Range | Description |
|---|---|---|
| Pitch Mix | 0 - 100% | Blend of the two pitch-shifted taps into the signal feeding the diffuser |
| Pitch | -24 - 24 st | Pitch shift of the first, per-channel pitch tap |
| Pitch Delay | 0 - 1000 ms | Delay before the first pitch tap |
| Pitch 2 | -24 - 24 st | Pitch shift of the second, per-channel pitch tap |
| Pitch 2 Delay | 0 - 1000 ms | Delay before the second pitch tap |
| Pitch Mode | Drift / Sync / Vocoder | Pitch-shifting algorithm shared by both pitch taps |
| Pitcher Shelf Low | -18 - 18 dB | Low-shelf gain (200 Hz) applied to both pitch taps, before they mix into the diffuser chain |
| Pitcher Shelf High | -18 - 18 dB | High-shelf gain (5 kHz) applied to both pitch taps, before they mix into the diffuser chain |

### Diffuser

| Control | Range | Description |
|---|---|---|
| Elements | 0 - 50 | Number of allpass delay stages in series; more elements thicken the diffusion |
| Tap Span | 0 - 100% | How many of the active chain's last elements are averaged into the output tap; 0% taps only the final element, 100% averages the whole active chain |
| Diffusion | -100 - 100% | Feedback amount of each allpass stage; higher values smear transients into a denser wash |
| Bulge | -1 - 1 | Skews the Early/Late Size distribution across elements toward the early or late end |
| Early Size | 0.5 - 100 m | Physical size (converted to delay length) of the first element in the chain |
| Late Size | 0.5 - 100 m | Physical size of the last element in the chain; set below Early Size to make echoes denser toward the end instead of the start |
| Size Spread | 0 - 10 m | Per-element size offset, alternated between channels, for stereo decorrelation; 0 keeps the chain mono, larger values widen the stereo image |
| Mod Depth | 0 - 1 | Depth of the pitch-modulating LFO applied to every second delay element |
| Mod Speed | 0.01 - 5 Hz | Rate of that modulation LFO |
| Low Pass | 20 - 20000 Hz | Damping filter cutoff applied inside each element's feedback path |
| Extreme Stereo Tap | off / on | Switches each channel's tap-mix window from averaging every active tap to keeping only even-indexed taps (L) or odd-indexed taps (R) |
| Wide | -100 - 100% | Effective only with Extreme Stereo Tap on; 0 collapses L/R to mono, +-100 reaches the full/swapped tap-split image |

### Reverb

| Control | Range | Description |
|---|---|---|
| Reverb Mix | -100 - 12 dB | Level of the FDN reverb tail (fed from the diffuser output) in the output |
| Reverb Size | 1 - 330 m | Average delay-line size of the FDN reverb tank |
| Reverb Decay | 1 - 100000 ms | RT60-style decay time of the FDN reverb tail |
| Reverb Shelf Low | -18 - 18 dB | Low-shelf gain (150 Hz) applied to the FDN reverb tail |
| Reverb Shelf High | -18 - 18 dB | High-shelf gain (6 kHz) applied to the FDN reverb tail |

## Displays

| Display | Shows |
|---|---|
| Bands | Per-element low/mid/high band level meters across the active diffuser chain |
| Sizes | Per-element delay size, visualizing the Early/Late Size and Bulge distribution |

## Audio flow
```mermaid
flowchart TD
    IN["Stereo input"]
    IN --> TAPS["3 raw-input taps: pre, pitch1, pitch2-mono"]
    TAPS --> PROC["Apply delays and pitch processing"]
    PROC --> WET["Average 3 sources into stereo wet feed"]
    WET --> DIFF["Stereo diffuser chains"]
    DIFF --> FDN["Mono FDN send -> stereo FDN return"]
    IN --> MIX["Final mix: dry + diffuser + FDN"]
    DIFF --> MIX
    FDN --> MIX
    MIX --> OUT["Stereo output"]
```

```mermaid
flowchart TD
    IN["Stereo input in(L,R)"]

    IN --> TAPS["Create 3 parallel taps from raw input"]

    TAPS --> PRE["Pre-delay tap, stereo\n dryToDiffuser[L/R]"]
    TAPS --> P1["Pitch1 tap, stereo\n pitch1Data[L/R]"]
    TAPS --> P2["Pitch2 tap source, mono\n monoDry = 0.5 * (L + R)"]

    PRE --> PRE_D["m_preDelay[L/R]"]
    P1 --> P1_D["m_pitchDelay[L/R]"]
    P1_D --> P1_P["m_pitcher[L/R]"]
    P2 --> P2_D["m_pitch2Delay"]
    P2_D --> P2_P["m_pitcher2"]

    PRE_D --> WET["Per-channel wet build\nwet[c] = (pre[c] + pitch1[c] + monoPitch2) / 3"]
    P1_P --> WET
    P2_P --> WET

    WET --> DIFF["Stereo diffuser stage\nm_diffuser[0], m_diffuser[1]"]
    DIFF --> WET_LR["Diffused wetData[L/R]"]

    WET_LR --> FDN_SEND["Mono FDN send\nfdnIn = (wetL + wetR) / FdnOrder"]
    FDN_SEND --> FDN["m_fdn.processBlockSplit"]
    FDN --> FDN_LR["Stereo FDN return\nfdnOut[L/R]"]

    IN --> MIX["Final stereo mix"]
    WET_LR --> MIX
    FDN_LR --> MIX

    MIX --> OUT["out[L/R] = m_dry*in + m_wet*wetData + m_fdnMix*fdnOut"]
```
