-- Smoke test for SetTrackCompressor/SetTrackRingMod/SetTrackTremolo: an evened-out, modulated
-- character on track C, applied as soon as this loads - record something onto track C and
-- play it back.
SetTrackCompressor(2, -24, 4, 5, 80)
SetTrackRingMod(2, 220, 0.3)
SetTrackTremolo(2, 5, 0.6, 0.3)
