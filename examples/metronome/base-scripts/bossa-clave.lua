-- Bossa nova 3-2 clave on wood, shaker on every 8th, kick on beats 1 and 3 (needs a bar of
-- at least 4 beats). Positions are 16ths: 0, 0.75, 1.5, 2.5, 3.25 beats.
SetName("Bossa Nova Clave")
ClearPattern()
local clave = { 0, 0.75, 1.5, 2.5, 3.25 }
for _, position in ipairs(clave) do
    if position < BarBeats then
        AddInstrument(position, Wood, -4)
    end
end
for beat = 0, BarBeats - 1 do
    AddInstrument(beat, Shaker, -10)
    AddInstrument(beat + 0.5, Shaker, -14)
    if beat % 2 == 0 then
        AddInstrument(beat, Kick, -3)
    end
end

ClearAnalysis()
for i = 0, BarBeats * 4 - 1 do
    AddAnalysisPosition(i / 4)
end
