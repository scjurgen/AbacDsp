# LooperImpl refactoring plan

## Goal
Split `LooperImpl` into a small orchestration facade plus focused subsystems.

## Main problems
- Transport, timing, rendering, persistence, freeze analysis, and UI/query helpers are mixed in one class.
- Audio-thread, message-thread, and worker-thread state are interleaved.
- `processBlock()` contains too many responsibilities.

## Target structure

### 1. `LooperImpl`
Keep as facade only.

Responsibilities:
- Public API used by editor/plugin.
- Ownership of core DSP objects.
- High-level wiring between subsystems.
- Final `processBlock()` orchestration.

### 2. `LooperTransportController`
Responsibilities:
- Handle transport pulses.
- Record/play/overdub/clear state transitions.
- Sequencer play/clear toggles.

Move:
- `handleTransportPulses()`
- `toggleRecord()`
- `togglePlay()`
- `toggleOverdub()`
- `toggleSequencerPlayback()`
- `clearAll()`
- `clearSequencer()`

### 3. `LooperTimingController`
Responsibilities:
- BPM application.
- Host sync.
- Time signature handling.
- Meter timeline management.
- Count-in and bar/beat position state.

Move:
- `syncToHostTransport()`
- `applyTimeSignatureAwareBpm()`
- `installTimeSignature()`
- `applyTimeSignatureRequest()`
- `applyMeterAtBarBoundary()`
- `finalizeMeterTimeline()`
- `resetTimekeeper()`
- `beginCountIn()`
- Timing-related parts of `applyParameters()`

### 4. `BarLockedRecordingController`
Responsibilities:
- Quantized record start/stop.
- Pre-roll capture ring.
- Pending stop handling.
- Auto-stop handling.
- Record-start mode details.

Move:
- `startRecording()`
- `beginBarLockedRecord()`
- `requestStop()`
- `commitPendingStop()`
- `finishRecording()`
- `updateCaptureRing()`
- Related state for pre-roll, pending stop, start offset, auto-stop

### 5. `FreezeService`
Responsibilities:
- Freeze request lifecycle.
- Background slicing analysis.
- Thumbnail generation.
- Audio-thread completion handoff.

Move:
- `requestFreeze()`
- `runFreezeAnalysis()`
- `computeSliceThumbnail()`
- `checkFreezeCompletion()`
- Freeze worker state/thread/cv/generation counters

### 6. `LoopStorageService`
Responsibilities:
- Save/load loop files.
- Name sanitizing and file paths.
- Background file I/O.
- Load conflict reporting/resolution.

Move:
- `setLoopsDirectory()`
- `listLoopNames()`
- `deleteLoopNamed()`
- `renameLoopNamed()`
- `requestSaveLoopAs()`
- `requestLoadLoop()`
- `resolveLoopLoadBpm()`
- `runSaveLoopAs()`
- `runLoadLoop()`
- `checkLoopSaveCompletion()`
- `checkLoopLoadCompletion()`
- Path and sanitize helpers

### 7. `SequencerPatternBuilder`
Responsibilities:
- Build/update sequencer pattern from frozen or loaded material.
- Keep slice-library to pattern mapping in one place.

Move:
- `rebuildPatternForCurrentLoop()`
- `populatePatternFromTrack()`
- Optional: track extraction helpers used only by save/load

### 8. `LooperViewModel`
Responsibilities:
- UI/query-only derived values.
- Labels, waveform helpers, display accessors.

Move:
- `getStateLabel()`
- `getBarBeatLabel()`
- `getOuterRingBars()`
- Waveform and slice display getters
- Spectrogram getters
- Other read-only derived helpers

## State split
Introduce explicit state groups.

### `LooperParams`
- User-controlled atomics.
- Pulse flags.
- Requested values from message thread.

### `LooperRuntimeState`
- Audio-thread mutable state.
- Applied BPM/time signature.
- Armed/count-in/pending-stop/playback mode state.

### `LooperAsyncState`
- Freeze/save/load worker-owned state.
- Generation counters.
- Completion flags and worker scratch buffers.

## `processBlock()` target shape
Reduce to clear stages:
- Apply parameter requests.
- Sync/update timing.
- Update capture ring.
- Handle transport actions.
- Render click/sequencer.
- Process recorder.
- Commit pending async or stop actions.
- Mix outputs.
- Update visualization state.

## Refactor order
1. Extract `LoopStorageService`.
2. Extract `FreezeService`.
3. Extract `BarLockedRecordingController`.
4. Extract `LooperTimingController`.
5. Extract `LooperTransportController`.
6. Extract `SequencerPatternBuilder`.
7. Move read-only UI helpers into `LooperViewModel`.
8. Simplify `LooperImpl::processBlock()` last.

## Constraints
- Keep `LooperImpl` public API stable.
- Preserve existing namespaces, templates, and external types.
- Do not change DSP behavior during structural extraction.
- Keep thread ownership explicit.
- Prefer composition over inheritance.

## First milestone
A good first pass is enough if:
- async save/load code is gone from `LooperImpl`,
- freeze code is gone from `LooperImpl`,
- bar-locked recording state is isolated,
- `processBlock()` reads as orchestration instead of implementation.
