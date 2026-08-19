-- test-combresonator.lua: one CombResonator tap for quick A/B reference. Load via
-- Settings > Scripts, replacing whatever patch script is currently running - one of a
-- matching set of test-<type>.lua files, one per tap type, all at the same 440 Hz/delay 0
-- baseline so they're directly comparable.
SetMaxTaps(1)
SetTap(0, 0, 7, 1.0, 0.0)
SetResonance(0, 440, 1.2)
-- Try SetResonance(0, 440, 1.2, true) instead for negative feedback - the resonant peaks
-- move to odd harmonics of half the tuned frequency (220 Hz here) instead of 440 Hz itself.

function OnTiming(bpm, divisionIndex) end -- no-op: keep this reference tap fixed
