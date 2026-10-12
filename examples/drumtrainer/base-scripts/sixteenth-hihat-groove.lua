-- 16th-note hihat with the beat accented, kick on the even beats plus the last 16th before
-- every odd beat, snare on the odd beats.
SetName("16th Hihat Groove")
ClearPattern()
local accents = {110, 60, 80, 60}
for beat = 0, BarBeats - 1 do
    for sixteenth = 0, 3 do
        AddHit(beat + sixteenth / 4, HihatClosed, accents[sixteenth + 1])
    end
    if beat % 2 == 0 then
        AddHit(beat, Kick)
        if beat + 1 < BarBeats then
            AddHit(beat + 0.75, Kick, 80)
        end
    else
        AddHit(beat, Snare)
    end
end
