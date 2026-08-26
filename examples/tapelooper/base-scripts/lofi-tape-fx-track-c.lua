-- Smoke test for SetTrackDrive/SetTrackChorus/SetTrackEcho: a warped-tape-echo character on
-- track C, applied as soon as this loads - record something onto track C and play it back.
SetTrackDrive(2, 0.5)
SetTrackChorus(2, 0.4, 0.6)
SetTrackEcho(2, 4, 0.45) -- 1/4-note repeats
