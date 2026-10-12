-- Disco / house: kick on every beat, open hihat on every off-beat, closed hihat with the kick,
-- snare on the odd beats.
SetName("Four on the Floor")
ClearPattern()
for beat = 0, BarBeats - 1 do
    AddHit(beat, Kick)
    AddHit(beat, HihatClosed, 80)
    AddHit(beat + 0.5, HihatOpen, 100)
    if beat % 2 == 1 then
        AddHit(beat, Snare)
    end
end
