# Looper

## Spectrogram regeneration on loop load

Bug: loading a saved loop does not show a spectrogram; the display only ever gets fed live
during an actual recording pass (`m_recordSpectrogram.processBlock()` while `isRecording()`),
so loaded audio never populates it.

### Phase 1: cache the image at save time (primary fix)
- When a loop finishes recording, `m_recordSpectrogram.getImageSet()` already holds the frozen
  full-resolution image. Persist it next to the existing `.wav`/`.json` sidecar as a new
  `<name>.spec` binary file.
- Format must be bounded and small, independent of loop length:
  - Fixed display resolution (resample/decimate to a constant column count, e.g. ~1024)
    instead of storing every native FFT slice (a 60s loop's raw image is tens of MB).
  - Quantize magnitude bins to 8-bit (dB-mapped) instead of float.
  - Small header: format version, bin count, column count, sample rate, fft length/window
    ratio, so future format changes fall back to regeneration instead of misparsing.
- On load: if the cache file exists and parses, decode straight into the display's image
  buffer. No worker thread involved in the common case.
- Open question: exact resolution/quantization tradeoff, tune once rendered.

### Phase 2: background regeneration fallback (loops without a cache)
- Dedicated worker thread, started after `checkLoopLoadCompletion()` installs
  `m_loopLoadLeft/Right`, only when no `.spec` cache was found.
- Downmix L/R and feed `SpectrogramBase::processBlock()` in paced chunks, not the whole
  buffer at once: the existing FFT queue is only `QUEUE_SIZE = 4` slots, so an unpaced call
  would silently drop almost everything. The regen thread must pace itself against the
  existing FFT worker (check queue occupancy / yield between chunks).
- Needs a real `reset()` on `SimpleSpectrogram`/`SpectrogramBase`: `setSlices()` only
  resizes-with-fill, which does not clear existing content when size is unchanged.
  Regeneration must explicitly zero the buffer and `m_currentSlice` first, or the new image
  shows garbage mixed with the previous session's data.
- Once regeneration finishes, opportunistically write the `.spec` cache so the next load of
  the same loop is instant (self-healing cache).

### Phase 3 (stretch, only matters for phase 2's path): rough-then-refined image
- Before the full-resolution background pass, do one fast low-res sweep (bigger hop, fewer
  bins, single pass) to paint something immediately, then let the full-res pass overwrite it
  incrementally. Only helps the no-cache fallback case; low priority.

### Order of work
1. Phase 1 (cache format + save/load wiring): biggest win, smallest risk, no threading changes.
2. Phase 2 (paced background regen + `reset()`): covers everything phase 1 doesn't.
3. Phase 3 only if phase 2 turns out visually slow for long loops.

Not urgent, revisit later.