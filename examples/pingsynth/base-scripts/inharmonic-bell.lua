-- inharmonic-bell: a fuller showcase of arbitrary (non-harmonic) partial ratios - bell
-- and struck-metal timbres come from ratios that are not whole multiples of each other,
-- which a fixed harmonic-series dropdown could never express but a Lua loop can. Also
-- demonstrates OnMpeModeChanged: the voice's attack/excitation character shifts with the
-- Mode dial, since MPE mode implies a monophonic string per channel (a slower, breathier
-- attack reads better there) while polyphonic mode favors a crisper strike.

local kBellRatios = { 0.56, 0.92, 1.19, 1.71, 2.00, 2.74, 3.00, 3.76, 4.07 }
local kDecayPerRatio = 1.8

local mpeMode = true

function OnMpeModeChanged(value)
    mpeMode = value
end

function OnNoteOn(channel, note, velocity)
    local fundamental = Music.NoteToHz(note)
    local vel = Vel.Exponential(velocity / 127)
    local harmonics = {}
    for i, ratio in ipairs(kBellRatios) do
        harmonics[i] = {
            freq = fundamental * ratio,
            gain = vel / ratio,
            decay = kDecayPerRatio / ratio,
            delayMs = (i - 1) * 3,
        }
    end
    local attackMs = mpeMode and 25 or 2
    local softExcitation = mpeMode and 0.15 or 0.02
    SetHarmonics(channel, note, velocity, { harmonics = harmonics, attackMs = attackMs, softExcitation = softExcitation })
end
