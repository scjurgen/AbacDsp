-- 7/8 in 2+2+3, independent of the Preset: seven pulses per bar, hihat on every pulse, with
-- kick, snare and kick on the three group starts. Timing is measured on every pulse.
SetName("7/8 (2+2+3)")
SetBeatsPerBar(7)

ClearPattern()
for pulse = 0, BarBeats - 1 do
    AddInstrument(pulse, Hihat, -10)
end
AddInstrument(0, Kick, 0)
AddInstrument(2, Snare, -2)
AddInstrument(4, Kick, -2)

ClearAnalysis()
for pulse = 0, BarBeats - 1 do
    AddAnalysisPosition(pulse)
end
