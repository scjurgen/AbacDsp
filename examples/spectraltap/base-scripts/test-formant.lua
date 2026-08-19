-- test-formant.lua: one Formant tap for quick A/B reference. Load via Settings > Scripts,
-- replacing whatever patch script is currently running - one of a matching set of
-- test-<type>.lua files, one per tap type, all at the same 440 Hz/delay 0 baseline so
-- they're directly comparable. F1/F2 factors and gains match the plugin's own stub script.
SetMaxTaps(1)
SetTap(0, 0, 6, 1.0, 0.0)
SetFormant(0, 440, 2.0, 0.6, 3.5, 0.35)

function OnTiming(bpm, divisionIndex) end -- no-op: keep this reference tap fixed
