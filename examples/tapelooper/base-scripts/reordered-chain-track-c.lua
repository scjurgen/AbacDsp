-- Smoke test for SetTrackChain: puts distortion before the filter (grittier - the filter now
-- shapes already-clipped harmonics) instead of the default filter-before-distortion order.
-- Record something onto track C and play it back.
SetTrackDrive(2, 0.6)
SetTrackFilter(2, 800, 0.5)
SetTrackChain(2, {"distortion", "filter", "chorus", "echo", "compressor", "ringmod", "tremolo"})
