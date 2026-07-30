# Looper

## Spectrogram regeneration on loop load

Bug: loading a saved loop does not show a spectrogram; the display only ever gets fed live
during an actual recording pass (`m_recordSpectrogram.processBlock()` while `isRecording()`),
so loaded audio never populates it.


### background regeneration
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

## CircularLoopDisplay enhancements (future)

- Antialiased ring mask: the spectrogram annulus edge is currently hard-edged, should be
  softened so it blends better with the surrounding UI.
- Volume curve currently draws outward from the base radius; should instead direct inward,
  and be drawn as a filled polygon rather than a stroked line, to avoid spiky peaks.

