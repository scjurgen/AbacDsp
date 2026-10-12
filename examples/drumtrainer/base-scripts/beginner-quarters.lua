-- First steps: closed hihat on every beat, kick on beat 1, snare on the last beat of the bar.
-- Two limbs at a time at most, so the focus stays on landing each hit on the click.
SetName("Beginner Quarters")
ClearPattern()
for beat = 0, BarBeats - 1 do
    AddHit(beat, HihatClosed, beat == 0 and 110 or 90)
end
AddHit(0, Kick)
AddHit(BarBeats - 1, Snare)
