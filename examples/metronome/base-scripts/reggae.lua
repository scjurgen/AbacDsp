-- Reggae-style one-drop feel: kick on every beat, snare on beat 3, 8th hihat accented on the
-- heavy beats (1 and 3). Timing is measured on beats 2 and 4 only.
ClearPattern()
for beat = 0, math.min(4, BarBeats) - 1 do
    AddInstrument(beat, Kick, 0)
end
if BarBeats > 2 then
    AddInstrument(2, Snare, 0)
end

for beat = 0, BarBeats - 1 do
    local heavy = beat % 2 == 0
    AddInstrument(beat, Hihat, heavy and -4 or -9)
    AddInstrument(beat + 0.5, Hihat, -14)
end

ClearAnalysis()
for _, position in ipairs({ 1, 3 }) do
    if position < BarBeats then
        AddAnalysisPosition(position)
    end
end
