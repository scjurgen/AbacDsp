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
- **Spectrogram:** live streaming during record (`SimpleSpectrogram`, worker-thread FFT),
  painted on the outer ring; the last image persists after stop. (Phase 5)
- **Slicing method:** transient mode uses spectral-flux onsets from a worker-side STFT of the
  finalized loop (snapped to the grid); grid mode unchanged. (Phase 6)
- **Playback source:** slices are extracted into an owned `SliceBank` (per-slice buffers) and
  played from there, not read from the record buffer; extraction runs on a worker thread with a
  raw-loop fallback until ready. Precondition for reverse/pitch-shift. (Phase 6)
- **Pivot (Phase 7):** always-slicing-the-whole-loop was the wrong shape. The looper is now a
  plain, traditional single-buffer record/play/overdub loop (no automatic slicing at all); a
  sample sequencer that requests individual slices on demand, copied lazily from the loop
  buffer and positioned relative to the beat grid, will be designed and added on top later
  (Phase 10, TBD by the user). `Slicer.h`/`SlicePlayer.h`/`SliceBank.h` are kept in
  `src/includes/` for that future phase but are currently unused by `LooperImpl`.

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
- [x] **4** `examples/looper/` built (Looper_Standalone warning-clean) and committed; host smoke-tested.
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
      - Post-4: merged bar + loop viz into one concentric `inc/CircularLoopDisplay.h` (inner disc =
        current bar, dry input only, short fast hand = bar phase; outer ring = whole loop, waveform
        band + slice/transient spokes + bar spokes, long slow hand = playhead drawn only across the
        ring). Ring span = loop length in bars (`getOuterRingBars`), grows one bar ahead while
        recording. `SliceWaveDisplay` kept as the linear strip. Committed d057725.

### TODO bullets 1-3 (spectrogram / off-thread slicing / play extracted)

- [x] **5** Loop spectrogram (live chart-recorder). TODO bullet 1. Smoke-tested OK (contrast/range
      tuning may follow).
      - Add `AbacDsp::SimpleSpectrogram` to `LooperImpl` (RT-safe: audio thread pushes windows, its
        own worker does the FFT), fed the dry input mono while recording; reset on record-start/clear;
        the last image persists after stop. Expose `getSpectrogramData()` (mirror MetronomeImpl).
      - Add a polar-iris spectrogram layer to `CircularLoopDisplay`'s outer ring (technique from
        `CircularSpectrogramDisplay`), mapped to the loop's angular span so it grows with the ring the
        same way the waveform band does. Drawn beneath the waveform band / spokes.
      - No change to slicing or playback. Standalone, visual smoke-test.
      - Decision: live-while-recording (not a static post-record pass).
- [~] **6** Off-thread spectral-flux slicing, marker-based (TODO bullets 2+3). Code-complete, tests
      green, standalone builds clean; host smoke-test pending. Fixes a real hazard: slicing used to
      allocate on the audio thread at record-stop.
      - [x] Worker thread in `LooperImpl` (std::jthread, declared last so it joins first). On finalize
        (and on overdub-end) the audio thread publishes a request gen; the worker slices off-thread into
        a double-buffered marker table; the audio thread consumes via a done-gen handshake and swaps
        front/back at a loop boundary. Punch-in record->overdub defers slicing until overdub ends.
      - [x] Spectral-flux onsets in `Slicer` (+ 3 tests): STFT + half-wave-rectified flux peak-pick,
        window-centred position, snapped to grid + zero crossings. Used for transient mode; grid mode
        unchanged. Time-domain detectOnsets kept.
      - [x] Playback is MARKER-BASED: slices play in place from the immutable loop buffer via the
        marker table (no extracted copies). Decision (after smoke-test): copies aren't needed once
        overdub stops mutating the base; a real copy is deferred to the destructive pitch-shift bullet.
      - [x] `Sampler/SliceBank.h` (+ test, 9 cases) kept in the repo for that future pitch-shift, but
        OUT of the live path (was the earlier extract-copies design, superseded).
      - [x] Transition: raw loop plays until markers are published, then swaps at the next loop wrap
        (`useRawLoop = overdubbing || !slicesReady`); voices reset on overdub entry. Record/overdub/clear
        pulses dropped while a slice request is in flight (~ms); play/stop stay live.
      - Known: slice count capped to SlicePlayer::kMaxSlices (256) to keep the swap allocation-free.
      - Smoke-test feedback: slicing a bit imprecise (tune spectral flux later); overdub to become a
        separate item, not a mix-in; show slice start/end as a shaded overlay on the spectrogram.
- [x] **7 (revised)** Pivot cleanup: the independent-overdub-layers design above (the original
      Phase 7) is superseded. "Slice everything up front" was the wrong shape; reverted to a plain,
      traditional single-buffer looper and removed all automatic whole-loop slicing, ready for an
      on-demand sample sequencer to be designed later (Phase 10).
      - [x] Removed from `LooperImpl.h`: the slice-request worker (`std::jthread`, double-buffered
        `SliceSet`/marker-swap machinery), the independent overdub `Layer` pool, and `SlicePlayer`
        position-driven triggering. Base loop now plays straight from `LoopRecorder`'s own output.
      - [x] Overdub reverted to `LoopRecorder`'s built-in single-buffer decay-overdub
        (`beginOverdub`/`endOverdub`, `Overdubbing` state) -- this was already implemented, just
        bypassed by the old Phase 7 layer design.
      - [x] `getSliceBoundaries()` now always returns `{}`. `CircularLoopDisplay`'s slice/transient
        spokes (`drawSliceSpokes`) removed since they had no data source left; the live spectrogram
        and waveform band stay. `setSliceBoundaries()` kept as a no-op on both display widgets (the
        generated `Processor.h` wiring calls it; changing that needs a blueprint/generator edit, not
        done here to keep the UI untouched per the TODO).
      - [x] `Slicer.h`, `SlicePlayer.h`, `SliceBank.h` (+ tests) untouched, just unreferenced from
        `LooperImpl` -- kept for the future on-demand sequencer.
      - Verified: `dev-explore.sh --example looper` end-to-end, JUCE `Looper_Standalone` builds
        warning-clean.
- [ ] **8a** Beat-quantized loop length + configurable boundary fade. `LoopRecorder::setSamplesPerBar`
      -> `setSamplesPerBeat` / `quantizeToBar` -> `quantizeToBeat` (loop length becomes a whole-beat
      multiple, not a whole-bar multiple). New `setFadeFrames(size_t)`; `finalizeLoop()` applies a
      linear fade-in over the first N frames and fade-out over the last N (click-free wrap seam,
      clamped for short loops). `LooperImpl` gets a fade-ms setter plumbed through.
- [ ] **8b** Pre/post-roll ring buffer + beat-relative snap on record start/stop. A continuous
      always-on ring buffer (`fadeFrames / 2`) in `LooperImpl`. Threshold-armed record start/stop snap
      to the nearest running beat boundary within an eighth-note tolerance: early hits splice the
      pre-roll onto the loop tail (pickup wraps forward); late hits pad the start with silence up to
      the boundary. New `BeatSequencer::samplesToNearestBeat()`-style query; new `LoopRecorder`
      `beginRecord(silentPrefixFrames)` / `stopRecord(tailSplice)` overloads. Both boundaries use the
      8a fade for click-free splices/pads, not just the loop-wrap seam.
- [ ] **9** Future, each standalone (remaining original TODO bullets, renumbered):
      - [ ] free recording (no BPM), extract the actual BPM when recording stops
        (`Analysis/TempoEstimator.h`)
      - [ ] reverse play (beat lands on the slice end)
      - [ ] shuffle slices
      - [ ] pitch-shift slices (reuse `Spectral/PhaseVocoderPitcher.h`, saves a new sample into the
        `SliceBank`, RT-safe pool)
- [ ] **10** On-demand sample sequencer (TODO "Looper with Sample Sequencer" step 3). Fed individual
      slices copied lazily from the loop buffer, positioned relative to the beat grid; design TBD by
      the user once 8a/8b land.

## Parameters (all CC-mappable)

record, start, pause, stop, overdub, clear (momentary), BPM (50..250),
swing (0..100%, 50% straight), click volume (dB, -inf..0). Host transport supported.
