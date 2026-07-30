# Looper

## Spectrogram regeneration on loop load (done)

Bug: loading a saved loop did not show a spectrogram; the display was only ever fed live
during an actual recording pass. Fixed: `LooperImpl` now runs a dedicated
`m_spectrogramRegenThread`, triggered from `checkLoopLoadCompletion()` right after
`m_recorder.loadLoop(...)`. It downmixes and decimates the loaded loop the same way the live
path does, feeds `SimpleSpectrogram` in `forwardLength()`-paced chunks (checking
`queueHasRoom()` before each push so nothing silently drops), and calls the new
`SpectrogramBase::resetWindow()` / `SimpleSpectrogram::reset()` first so no stale image data
leaks across loads.

`getSpectrogramHeadFrames()` reports 0 (blank ring) while a regen is still in flight, since
`CircularLoopDisplay` assumes `activeSlice` is time-synced with the reported head position.
A `m_spectrogramFeedLock` (non-blocking CAS on the audio thread, spin-acquire on the regen
thread) guards against the live feed and the regen feed both writing into
`SimpleSpectrogram`'s window buffer if a new recording starts while a previous load's regen
is still catching up.

## CircularLoopDisplay enhancements (future)

- Antialiased ring mask: the spectrogram annulus edge is currently hard-edged, should be
  softened so it blends better with the surrounding UI.
- Volume curve currently draws outward from the base radius; should instead direct inward,
  and be drawn as a filled polygon rather than a stroked line, to avoid spiky peaks.

