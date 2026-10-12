-- Rock groove with the hihat opened on the "and" of the last beat and closed again on beat 1:
-- trains the foot to lift exactly on the off-beat and come down on the downbeat.
SetName("Open Hihat on the And")
ClearPattern()
for beat = 0, BarBeats - 1 do
    AddHit(beat, HihatClosed, 100)
    if beat == BarBeats - 1 then
        AddHit(beat + 0.5, HihatOpen, 100)
    else
        AddHit(beat + 0.5, HihatClosed, 70)
    end
    AddHit(beat, beat % 2 == 0 and Kick or Snare)
end
