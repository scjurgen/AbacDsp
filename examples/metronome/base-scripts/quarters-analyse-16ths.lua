-- Plays only a quarter-note pulse (woodblock, accented downbeat) but measures what you play
-- against a 16th grid, so subdivisions you add on top of the pulse are analysed too.
SetName("Quarters, 16th Analysis")
ClearPattern()
for beat = 0, BarBeats - 1 do
    AddInstrument(beat, Wood, beat == 0 and 0 or -6)
end

ClearAnalysis()
for i = 0, BarBeats * 4 - 1 do
    AddAnalysisPosition(i / 4)
end
