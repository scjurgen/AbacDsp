-- Straight rock groove: kick on the even beats, snare on the odd beats, closed hihat on every
-- 8th with the off-beats softer. Analysis measures your playing against a 16th grid.
SetName("Rock 8th Groove")
ClearPattern()
for beat = 0, BarBeats - 1 do
    AddInstrument(beat, Hihat, -6)
    AddInstrument(beat + 0.5, Hihat, -12)
    if beat % 2 == 0 then
        AddInstrument(beat, Kick, 0)
    else
        AddInstrument(beat, Snare, -1)
    end
end

ClearAnalysis()
for i = 0, BarBeats * 4 - 1 do
    AddAnalysisPosition(i / 4)
end
