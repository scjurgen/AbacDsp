-- Five even hits spread over the whole bar (a 5:4 polyrhythm) on wood, against a kick on
-- every beat. Analysis measures only the five polyrhythm positions.
ClearPattern()
for beat = 0, BarBeats - 1 do
    AddInstrument(beat, Kick, -4)
end
for i = 0, 4 do
    AddInstrument(i * BarBeats / 5, Wood, -2)
end

ClearAnalysis()
for i = 0, 4 do
    AddAnalysisPosition(i * BarBeats / 5)
end
