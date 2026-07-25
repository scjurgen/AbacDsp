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
- **Sample sequencer (Phase 10):** slicing is manual/on-demand, not automatic at record-stop
  (that shape was already tried and reverted in Phase 7). Frozen tracks in the slice library
  survive independently of the base looper's Clear/re-record. Per-voice insert effects
  (compression, distortion), not a shared bus, but the real DSP for those is deferred: the user
  has existing library code to drop in later, so this phase only wires a per-voice placeholder
  (pass-through) effect slot. This design pass covers the DSP engine and a programmatic pattern
  API only; a piano-roll/step-grid UI is a later phase. Lives entirely in `src/includes/Sampler/`
  (not example-local, not a new top-level folder) so it is reusable by other, non-looper
  projects, same as `SliceBank.h`/`SlicePlayer.h` today.

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
- [x] **8a** Beat-quantized loop length + configurable boundary fade. `LoopRecorder::setSamplesPerBar`
      -> `setSamplesPerBeat` / `quantizeToBar` -> `quantizeToBeat` (loop length becomes a whole-beat
      multiple, not a whole-bar multiple). New `setFadeFrames(size_t)`; `finalizeLoop()` applies a
      linear fade-in over the first N frames and fade-out over the last N (click-free wrap seam,
      clamped for short loops). `LooperImpl` gets a `setFadeMs()` setter (atomic, default 5 ms,
      applied like the other parameters) plumbed to `m_recorder.setFadeFrames()`; not yet wired to a
      blueprint dial. Tests: 3 new `LoopRecorderTest` cases (ramp shape, zero-fade no-op, short-loop
      clamp). Verified: full `ctest` suite green, `dev-explore.sh --example looper`, JUCE
      `Looper_Standalone` builds warning-clean.
- [x] **8b** Pre/post-roll capture folded across the loop seam, beat-locked. Replaces the earlier
      "detect early/late, branch between splice and silence-pad" idea: capture is unconditional and
      continuous (real audio always, never synthesized silence -- if the musician was quiet there,
      the captured audio for that stretch is quiet too), and the fold is the same operation on both
      sides of the seam.

      **Notation:**
      - `fs` = sample rate, `spb` = samples per beat (`BeatSequencer::samplesPerBeat()`, the clock
        never resets), `L` = finalized `loopLengthFrames` (whole-beat multiple, from 8a).
      - `x[n]` = the continuously-captured raw input, indexed by absolute beat-clock sample position
        (captured independent of recorder state, not just while `Recording`).
      - `s` = the beat-clock sample position of the tick nearest record-start (loop frame 0). Because
        `L` is a whole-beat multiple counted from `s`, the tick nearest record-stop is exactly
        `e = s + L`.
      - `R = round(spb / 8)`: the fixed pre-/post-roll capture window, one eighth note, captured
        unconditionally on every start and every stop.
      - `F = min(fadeFrames, R)`: the 8a boundary-fade window (ms -> frames via `setFadeMs`),
        clamped so the taper never exceeds the roll window.

      **Base take** (unchanged): `y[k] = x[s + k]` for `k = 0 .. L-1`.

      **Pre-roll fold onto the tail** (`i = 0` is the far edge, 1/8 beat before `s`; `i = R-1` is
      adjacent to the beat):
      ```
      for i in 0 .. R-1:
          g = min((i + 1) / F, 1)
          y[L - R + i] += x[s - R + i] * g
      ```
      **Post-roll fold onto the front** (`i = 0` is adjacent to the beat, right after `e`; `i = R-1`
      is the far edge, 1/8 beat after `e`):
      ```
      for i in 0 .. R-1:
          g = min((R - i) / F, 1)
          y[i] += x[e + i] * g
      ```
      Net effect: the fade sits at the *outer* edge of each roll window (`F` samples, ramping in for
      the pre-roll / out for the post-roll); the rest of the window plays at 0 dB right up to the
      beat, so a pickup or trailing note hits full strength at the impact point instead of being
      faded near it. Both folds are a sum, not a replace, but in the base-take case the destination
      is otherwise silence so in practice it's just placement.

      **Supersedes 8a's `applyBoundaryFades()` at the wrap seam**: that plain amplitude taper down to
      ~0 at both physical edges is exactly wrong once real pre-/post-roll content belongs there at
      full volume. `applyBoundaryFades()` stays only as the fallback when there's no beat reference
      to fold around (`samplesPerBeat() == 0`, i.e. unquantized free recording).

      **Implementation:** `LooperImpl` needs a continuously-written capture spanning at least
      `[s - R, s)` before a start and `[e, e + R)` after a stop -- a small always-on buffer, sized
      for `R` (not the earlier "half the fade window" sizing, which was only ever right for the taper
      length, not the roll window itself). `LoopRecorder` gains the actual fold-and-sum step, run once
      at `stopRecord()` after `applyBoundaryFades()`'s replacement logic. Scope: base take only
      (`beginRecord`/`stopRecord`); `beginOverdub`/`endOverdub` don't redefine loop boundaries so
      aren't touched.

      **Done, uncommitted.** `BeatSequencer::samplesToNearestBeat()` (+4 tests). `LoopRecorder::
      stopRecordBeatLocked(loopLength, preRoll, lateGap, rollFrames)` (+3 tests): shifts captured
      content up by `lateGap` (backfilling from the second half of `preRoll` when the take started a
      touch late), stashes the post-roll before the shift can clobber it, then applies the pre-/
      post-roll folds above. `LooperImpl` gains an always-on capture ring (1s, generous), a free-
      running `m_absPos` (never reset), a pre-roll snapshot taken at record-start, and a
      pending-start/pending-stop wait pair (`beginBeatAwareRecord`/`commitPendingStart`,
      `requestStop`/`commitPendingStop`) so real audio is what gets folded, never synthesized
      silence. Threshold-armed start only (button-press-immediate record stays unquantized, as
      scoped); stop applies to any beat-locked take's stop regardless of how it started. Outside the
      1/8-beat tolerance on either end, falls back to the plain 8a finalize. Bug caught during
      smoke-testing and fixed: `commitPendingStart` must clamp `commitAbs` to `max(absPos, tickAbs)`
      -- block granularity means the commit can land on the block *containing* the tick, not exactly
      on it, and using a `commitAbs` before the tick underflows the unsigned `lateGap` subtraction.
      Verified via a `dev-explore.sh --example looper` scenario (pickup blip before an armed
      threshold crossing, trailing blip after a stop): resulting loop's peak waveform showed the
      pickup on the tail and the trailing note on the front, silence in between, matching the design
      exactly. Full `ctest` suite green (20/20), JUCE `Looper_Standalone` warning-clean.

      **Post-review fix:** the beat clock (`BeatSequencer`/`m_seq`) is the single source of truth for
      timing, full stop -- nothing in `LooperImpl` may ever reset or reposition it (host sync excepted:
      `syncToPpq` keeps it aligned to the host transport, which is the source of truth in that mode).
      Removed every `m_seq.reset()` call: the out-of-tolerance record-start fallback, the plain
      immediate record-button start, play-start, and the post-finalize re-align originally added at
      beat-locked stop. Previously, missing the eighth-note tolerance (easy to do -- it's a tight
      window) silently fell back to a path that reset the clock, which read as "the beat position
      shifts when threshold is reached." Now every transport action locks onto wherever the clock
      already is instead of moving it. Re-verified: full `ctest` green, `dev-explore.sh --example
      looper` scenario unchanged (it never depended on the resets).

      **Post-review redesign:** the pending-start wait was itself wrong -- "we start recording when
      the threshold is reached" means capture must never be delayed, but the early-crossing case
      (tick still ahead) waited up to an eighth note before calling `beginRecord()`. Recording now
      always starts immediately at the trigger; the correction for an early crossing happens by
      *relocating* the redundant pre-tick frames onto the tail afterward, not by delaying capture.
      This take's own frame 0 (`trig`) can now land either side of the tick `s`; `finalizeBeatLocked`
      takes a signed `startOffset = s - trig` instead of a non-negative `lateGap`:
      - `startOffset > 0` (early: tick still ahead when capture started): loop frame `k` = capture
        frame `k+startOffset` (shift down, forward iteration). The redundant frames
        `[0, startOffset)` this take already captured are stashed (`m_frontScratch`, a new
        preallocated buffer alongside `m_postRollScratch`) before the shift overwrites them, then
        folded onto the tail alongside whatever the ring-buffer `preRoll` snapshot covers for the
        rest of the fixed roll window.
      - `startOffset < 0` (late, unchanged from before): backfill from `preRoll`, shift up.

      `preRoll` itself is no longer a fixed `2*rollFrames` snapshot taken after a wait -- it's
      `[s-rollFrames, trig)` (length `rollFrames-startOffset`, 0 to `2*rollFrames`), snapshotted
      *immediately* at record-start (the ring buffer's history is bounded and won't still hold it
      later). `LooperImpl` drops `m_pendingStart`/`commitPendingStart`/`commitBeatLockedStartAt`
      entirely; `beginBeatAwareRecord()` is now a single straight-line function. The stop side is
      unchanged (post-roll genuinely requires waiting for real audio that hasn't happened yet, so
      `m_pendingStop` stays). New `LoopRecorderTest` case for the early-relocate path (+1, 4 total for
      `stopRecordBeatLocked`). Re-verified: full `ctest` green, JUCE build warning-clean,
      `dev-explore.sh` scenario shows recording starting immediately on the trigger block instead of
      after the ~1/8-beat wait, same correct fold result (headPeak/tailPeak both 0.8, midPeak 0.0).

      **Integration test, wired into `ctest`:** `examples/looper/src/unittests/Looper_tests.cpp`
      (a pre-existing, never-wired-up file -- see below) now has a 3x3 `BeatLockMatrixTest` covering
      start timing x stop timing (each before/on/after the tick), driving `LooperImpl::processBlock`
      directly with a precisely-positioned "doublet" (-1.0, +1.0) as the threshold-crossing signal and
      scanning the raw finalized loop for it. Asserts two things per case: `loopLength` is always the
      same exact beat-multiple regardless of stop-request timing (the invariant that stop timing,
      within tolerance, must never affect what's captured), and the doublet lands at the frame its
      start category predicts (`loopLength-N` early, `0` on-time, `N` late). All 9 pass. New
      `LooperImpl` accessors for this: `rawLoopSample()`, `rawLoopLengthFrames()` (thin pass-throughs
      to `LoopRecorder`, not used by the UI).

      **Output-stream timing test added** (`OutputTimingTest`, still isolated -- `LooperImpl` only,
      no JUCE/host layer): real 48 kHz, 60 BPM (1 beat = 1 second exactly), records 1/2/3/4 beats with
      the signal before/on/after the start tick, then captures the actual *output stream* (not the
      stored buffer) over 4 full loop repeats with silent input, scanning for the doublet on every
      repeat. Asserts the first occurrence lands at the predicted frame (per the same before/on/after
      formula as `BeatLockMatrixTest`) AND that every subsequent repeat is exactly one loop length
      later than the last -- zero drift across repeats. All 12 cases (4 beat-counts x 3 start timings)
      pass. Together with `BeatLockMatrixTest`, 21 cases total confirm the algorithm is correct in
      isolation, including the produced audio stream over multiple cycles, not just internal state.

      Build wiring fixed along the way: `examples/looper/src/unittests/CMakeLists.txt` already existed
      (hardcoded into the generator's `protected_files` set -- meant to be hand-maintained, never
      auto-regenerated) but its parent `examples/looper/CMakeLists.txt` never got the matching
      `add_subdirectory(src/unittests)` block, and the unittests CMakeLists still had stale
      standalone-mode include paths (`3rdparty/AbacDsp/src/includes`) instead of this repo's actual
      localexample layout (`../../src/includes`). Fixed both; the new `PluginTests` target builds via
      `BUILD_FULL_PROJECT=ON` (`cmake-build-release`) and is discovered by `ctest` via
      `gtest_discover_tests` (note: after changing test names, delete the stale
      `PluginTests[1]_tests.cmake`/`_include.cmake` and reconfigure -- `gtest_discover_tests`'s cache
      can otherwise keep a stale display name even though the actual `--gtest_filter` is correct).
      Not reachable from `dev-test.sh` (that path is `BUILD_FULL_PROJECT=OFF`); run via
      `ctest --test-dir cmake-build-release -R BeatLockMatrixTest`.

      Target renamed `PluginTests` -> `LooperPluginTests`: the generator's `unittests/CMakeLists.txt`
      template hardcodes the target name literally as `PluginTests` for *every* example (confirmed:
      identical in all of them), which is a landmine the moment two examples' unittests both get wired
      up. Not the compile failure itself (both `cmake-build-release` and the user's own
      `cmake-build-debug` built and ran it fine once reconfigured), but clearly the source of "can't
      find/compile the test" confusion, so fixed regardless.
- [ ] **9 (dropped for now)** Future, each standalone (remaining original TODO bullets, renumbered).
      Deferred, not scoped for the current push; revisit after 10.
      - [ ] free recording (no BPM), extract the actual BPM when recording stops
        (`Analysis/TempoEstimator.h`)
      - [ ] reverse play (beat lands on the slice end)
      - [ ] shuffle slices
      - [ ] pitch-shift slices (reuse `Spectral/PhaseVocoderPitcher.h`, saves a new sample into the
        `SliceBank`, RT-safe pool)
- [ ] **10** On-demand sample sequencer (TODO "Looper with Sample Sequencer" step 3), an overlay
      engine on top of the plain looper. All new headers live in `src/includes/Sampler/` (reusable
      outside the looper example, same as `SliceBank.h`/`SlicePlayer.h` today).

      **Locked decisions** (see also the design-decisions block above): slicing a loop into the
      library is a manual "freeze" action, not automatic at record-stop (Phase 7 already tried and
      reverted always-slicing); frozen tracks persist independently of the base looper's
      Clear/re-record; compression/distortion are per-voice inserts, not a shared bus; pitch change
      is resampling only (no time-stretch), reusing `Numbers/Interpolation.h`'s hermite variants for
      the fractional read, not `Spectral/PhaseVocoderPitcher.h`. This pass is DSP engine + a
      programmatic pattern API only; a piano-roll/step-grid UI is a later phase.

      **Data model:**
      ```
      [LoopRecorder] --(freeze action)--> Slicer::adaptiveTransientSlices --extract--> SliceLibrary
                                                                                            |
                                  SequencePattern --events--> SequencerEngine <--- reads slices
                                         ^                          |
                           (shuffle/humanize/reverse/random         v
                            transform the event list)       voice pool: pitch/gain/reverse/fade
                                                                      |
                                  BeatSequencer (shared clock,        v
                                   same one the looper uses)  per-voice FX (comp/distortion) --> out
      ```

      - [x] **10a** `Sampler/SliceLibrary.h` (replaces `SliceBank.h`/`SliceBank_test.cpp` outright via
        `git rm`; nothing else referenced them, confirmed by grep). `extractTrack(loop, slices) ->
        trackIndex` appends rather than replacing, so slices from several frozen loops coexist,
        addressed as `(track, indexInTrack)` via `sliceInfo()`/`sample()`, plus a flat `slices()`
        list across all tracks. `SliceInfo` adds `peak`/`rms` computed at extraction time (metadata
        for "normalize" and default gain, not a runtime cost). `clear()` drops every track (the only
        way to reset; per the locked lifecycle decision, extracting into it is otherwise independent
        of the looper's own Clear). Overflow now stops extraction per-track (only that track's
        remainder is dropped; earlier tracks are untouched) rather than for the whole bank. 13 tests
        in `test/Sampler/SliceLibrary_test.cpp` (mirrors the old `SliceBank` coverage + 3 new
        multi-track cases: separate tracks appended, earlier track survives a later track's overflow,
        peak/rms correctness). Verified: `SamplerTests` (65/65) and full suite (20/20 targets) green,
        no warnings.
      - [x] **10b** `Sampler/SequencePattern.h`: plain event-list data structure, no engine logic.
        Constructed with `(lengthBars, beatsPerBar, stepsPerBeat)`; `totalSteps()` is their product.
        `SequenceEvent{stepPosition, track, sliceIndex, gain, pitchRatio, reverse, randomizeSlice,
        timingOffsetFrames, humanizeAmountFrames}` (renamed from the earlier `gridPosition` sketch to
        `stepPosition`, matching the constructor's step-based grid rather than a raw frame position).
        Shuffle = edit on `timingOffsetFrames`; humanize = small RNG jitter re-rolled per pattern
        repeat at trigger time; `randomizeSlice` = a flag meaning the engine re-picks `sliceIndex`
        from `track` each repeat instead of using the fixed one stored here. `addEvent` rejects (no-op,
        returns false) a `stepPosition >= totalSteps()`; `removeEvent`/`clear` mutate in place;
        `eventIndicesAtStep()` returns every event index at a step in insertion order, supporting
        layered (simultaneous) events at the same step. Pure data + edit operations, independently
        testable without audio. 13 tests in `test/Sampler/SequencePattern_test.cpp`. Verified:
        `SamplerTests` and full suite (20/20 targets) green, no warnings.
      - [ ] **10c** `Sampler/SequencerEngine.h`: polyphonic voice pool (same shape as `SlicePlayer`'s:
        fixed array, oldest-voice steal), driven by the same per-sample `GridEvent` stream
        `LooperImpl` already computes from `m_seq` (not a separate clock, so it can never drift from
        the loop or the click). Each voice reads its slice via fractional position (hermite
        interpolation) with the read increment set by `pitchRatio`, direction negative when
        `reverse`; reuses `SlicePlayer`'s edge-fade approach for click-free starts/stops/steals.
      - [ ] **10d/10e (deferred, placeholders only)** Real `Dynamics/Compressor.h` and a distortion/
        waveshaper header are NOT built in this phase; the user has existing library code for these
        to drop in later. Stand in with a trivial per-voice `PassthroughEffect` (identity, no-op)
        satisfying whatever effect concept/interface `SequencerEngine` defines, so the per-voice
        insert slot exists and is exercised by tests now, without committing to a real DSP
        implementation or folder layout ahead of time.
      - [ ] **10f** Wire the per-voice insert slot (placeholder effects for now, swappable for real
        ones later) into `SequencerEngine`; apply `SequenceEvent.gain` and `SliceLibrary`'s stored
        normalize metadata at trigger time.
      - [ ] **10g** Integrate into `LooperImpl`: the manual freeze action, hooking the engine's output
        into `processBlock` alongside the existing loop playback, and minimal accessors for a later
        UI (pattern/active-voice state) without building that UI now.

## Parameters (all CC-mappable)

record, start, pause, stop, overdub, clear (momentary), BPM (50..250),
swing (0..100%, 50% straight), click volume (dB, -inf..0). Host transport supported.
