-- odd-harmonics: builds an odd-harmonic series in plain Lua and rings the resonator bank
-- on every note - the same shape as the engine's built-in default script, kept here as a
-- loadable/importable starting point. Edit NumHarmonics/Decay/GainFalloff, or replace the
-- ratio formula in the loop below, to try a different timbre (even, stretched, inharmonic,
-- whatever the math expresses - there is no fixed formula list, only what this loop writes).

NumHarmonics = 12
Decay = 1.2
GainFalloff = 0.7

function OnNoteOn(channel, note, velocity)
    local fundamental = Music.NoteToHz(note)
    local vel = Vel.Cubic(velocity / 127)
    local harmonics = {}
    for i = 1, NumHarmonics do
        local ratio = 2 * i - 1 -- odd harmonics: 1, 3, 5, 7, ...
        harmonics[i] = {
            freq = fundamental * ratio,
            gain = vel * (GainFalloff ^ (i - 1)),
            decay = Decay,
            delayMs = 0,
        }
    end
    SetHarmonics(channel, note, velocity, { harmonics = harmonics, attackMs = 1, softExcitation = 0 })
end
