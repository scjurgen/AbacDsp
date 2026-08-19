-- test-ringmodulator.lua: one RingModulator tap for quick A/B reference. Load via
-- Settings > Scripts, replacing whatever patch script is currently running - one of a
-- matching set of test-<type>.lua files, one per tap type, all at the same 440 Hz/delay 0
-- baseline so they're directly comparable. No decay/Q - only SetFrequency matters here.
SetMaxTaps(1)
SetTap(0, 0, 8, 1.0, 0.0)
SetFrequency(0, 440)

function OnTiming(bpm, divisionIndex) end -- no-op: keep this reference tap fixed
