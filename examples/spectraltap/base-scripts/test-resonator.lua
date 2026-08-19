-- test-resonator.lua: one Resonator tap for quick A/B reference. Load via Settings >
-- Scripts, replacing whatever patch script is currently running - one of a matching set
-- of test-<type>.lua files, one per tap type, all at the same 440 Hz/delay 0 baseline so
-- they're directly comparable. Compare against test-bandpass.lua: both use the same
-- decay-to-Q mapping and should sound comparably loud/resonant, not radically different.
SetMaxTaps(1)
SetTap(0, 0, 5, 1.0, 0.0)
SetResonance(0, 440, 1.2)

function OnTiming(bpm, divisionIndex) end -- no-op: keep this reference tap fixed
