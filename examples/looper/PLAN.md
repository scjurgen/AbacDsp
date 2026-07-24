# Looper Implementation Plan

A slicing looper example plugin, backed by new testable DSP includes in `src/includes/`.
Records audio, quantizes the loop to whole bars, slices it (grid + transient), and
plays the slices back locked to a BPM/swing grid with a metronome click.

## Design decisions (locked)

- **Slicing:** grid + transient built together (transient onsets snap to the grid).
- **Loop length:** quantized to whole bars at the current BPM when recording stops.
- **Shared code:** metronome is refactored onto the extracted `ClickGenerator` +
  `BeatSequencer`; the existing metronome test suite is the behavior-preserving safety net.
- **Realtime:** all buffers preallocated to a max loop length at construction. No
  allocation on the audio thread. Slice table published atomically to the playback path.
- **Swing:** UI uses 0..100% (50% = straight). A `swingPercentToRatio()` helper maps to
  the sequencer's internal long/short ratio, shared with the metronome.

## Signal flow

```
Input -> [LoopRecorder] --record/overdub--> loop buffer
                                              |
                             (on stop)  [Slicer] -> slice table
                                              |
[BeatSequencer]+[HostTransport] -> grid --> [SlicePlayer] -> out + [ClickGenerator]
                                              |
                                    visualization state -> UI (slices + playhead)
```

## Phases (each a closed TDD cycle: header + *_test.cpp, build via dev-scripts/, green, review)

- [x] **0a** `Generators/ClickGenerator.h` + test (8 cases); `MetronomeImpl` rewired onto it,
      compiles clean with -Wall -Wextra -Wpedantic, full library suite green.
- [x] **0b** `Generators/BeatSequencer.h` + test (12 cases): host-syncable grid, per-sample
      GridEvent (beatStart/subdivision/barWrapped), swing sub-positions, ppq sync. Metronome
      rewired onto it; verified byte-for-byte identical output vs the pre-refactor version across
      straight/shuffle/odd-meter+dropbars/host-sync (golden comparison, 0 mismatches).
- [x] **1** `Sampler/LoopRecorder.h` + test (18 cases): Empty/Recording/Playing/Overdubbing/
      Stopped state machine, bar-quantized loop length on stop (tail zeroed, stale-tail leak
      guarded), overdub with decay, auto-stop when full, play/pause/stop, no RT allocation.
- [x] **2** `Analysis/Slicer.h` + test (14 cases): uniform grid slicing (remainder distributed),
      grid boundaries, slices-from-boundaries (implied leading 0, dedupe/sort), transient onset
      detection (magnitude envelope + relative threshold + min-gap), grid snap with max distance,
      zero-crossing edge snap, stereo->mono downmix.
- [x] **3** `Sampler/SlicePlayer.h` + test (10 cases): 16-voice pool plays loop slices launched by
      index, per-sample linear fade-in/out (click-free edges), playLength caps a slice to the grid
      step, overlapping triggers sum, oldest-voice stealing caps polyphony, short-slice fade clamp.
      Beat-grid triggering is wired in Phase 4 (looper drives triggerSlice from BeatSequencer).
- [~] **4** `examples/looper/` code-complete; full JUCE build + host smoke-test pending (user).
      - Generator extended: `cc` now works on `switch` controls (sustain/damper-pedal style,
        0..127 scaled across the 0..1 bool range -> flips at 63/64; runtime unchanged). Verified.
      - `blueprints/looper.json`: 4 momentary transport pulses (Record/Play/Overdub/Clear, all CC,
        auto-reset, labels reflect real state) + Host Sync; Grid/Transient mode + division drops;
        BPM/Swing/Click dials (CC); SliceWaveDisplay gauge; patches + host_transport.
      - `SliceWaveDisplay.h` added to generator template library (inc list + templates dir).
      - Hand-written `impl/LooperImpl.h` wires LoopRecorder + Slicer + SlicePlayer + BeatSequencer
        + ClickGenerator. Compiles clean (-Wall -Wextra) and runs end-to-end via dev-explore
        (record -> bar-quantize -> 8 grid slices -> playback -> clear). Editor/Processor wiring
        verified by grep.
      - Known limitations to revisit: slicing runs inline on the audio thread at record-stop
        (one-time alloc; move to a worker); swing affects the click, not slice timing yet;
        host-sync drives tempo/click, not loop phase alignment.
- [ ] **5** Future, each standalone:
      - [ ] tempo/beat extraction (`Analysis/TempoEstimator.h`)
      - [ ] reverse play (beat lands on the slice end)
      - [ ] shuffle slices
      - [ ] pitch-shift slices (reuse `Spectral/PhaseVocoderPitcher.h`, RT-safe pool)

## Parameters (all CC-mappable)

record, start, pause, stop, overdub, clear (momentary), BPM (50..250),
swing (0..100%, 50% straight), click volume (dB, -inf..0). Host transport supported.
