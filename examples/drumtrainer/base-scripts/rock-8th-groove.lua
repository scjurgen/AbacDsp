-- Straight rock groove: closed hihat on every 8th (off-beats softer), kick on the even beats,
-- snare on the odd beats (2 and 4 in 4/4).
SetName("Rock 8th Groove")
ClearPattern()
for beat = 0, BarBeats - 1 do
    AddHit(beat, HihatClosed, 100)
    AddHit(beat + 0.5, HihatClosed, 70)
    if beat % 2 == 0 then
        AddHit(beat, Kick)
    else
        AddHit(beat, Snare)
    end
end
