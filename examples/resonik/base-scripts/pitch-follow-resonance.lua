-- pitch-follow-resonance: a fuller showcase of Resonik's Lua control surface, not a
-- minimal example. Harmonizes the resonator bank to the incoming signal's detected
-- pitch, quantized to a selectable key (Root/Scale dials below), and sweeps a moving
-- window of individually-tuned chains across the whole bank each hop, so a large number
-- of resonators stay actively - and distinctly - engaged over time instead of always
-- retuning the same handful.

UICreateParameterSet({
    { id = "root", name = "Root", type = "drop",
      items = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, default = 0,
      description = "Key root note the detected pitch is harmonized against" },
    { id = "scale", name = "Scale", type = "drop",
      items = { "Major", "NaturalMinor", "Dorian", "MajorPentatonic", "MinorPentatonic", "Blues", "WholeTone", "Chromatic" },
      default = 0,
      description = "Scale the detected pitch is quantized into" },
})

local kScaleNames = { "Major", "NaturalMinor", "Dorian", "MajorPentatonic", "MinorPentatonic", "Blues", "WholeTone",
                      "Chromatic" }

RootNote = 60 -- C4; kept as a global so OnPitchDetected below can read it
ScaleName = kScaleNames[1]

function OnRootChanged(index)
    RootNote = 60 + index
end

function OnScaleChanged(index)
    ScaleName = kScaleNames[index + 1]
end

-- Chains are retuned kWindowSize at a time, one window per pitch-detection hop; the
-- window advances each call so the sweep visits every chain in 0..(kWindowSize *
-- kNumWindows - 1) in turn rather than only ever touching the same few.
local kWindowSize = 4
local kNumWindows = 8
WindowIndex = 0

function OnPitchDetected(hz, confidence)
    if hz <= 0 or confidence < 0.5 then
        return
    end

    local note = Music.HarmonizeToScale(Music.HzToNote(hz), RootNote, ScaleName)
    local fundamental = Music.NoteToHz(note)

    -- The aggregate spread keeps every chain broadly on-topic; the per-body sweep below
    -- then layers finer, individually-shaped detail on top of a rotating slice of it.
    SetFreqRange(fundamental, fundamental * 8)
    SetDecayRange(0.3, 4.0)
    SetGainRange(-18, 0)
    SetDelayRange(0, 300)
    SetQ(6)

    local firstIndex = WindowIndex * kWindowSize
    for i = 0, kWindowSize - 1 do
        local bodyIndex = firstIndex + i
        -- Walks further up the key per body, re-quantized into the scale, then stacks
        -- an extra octave every 7 bodies for spread across the whole sweep.
        local bodyNote = Music.HarmonizeToScale(note + i * 2, RootNote, ScaleName)
        local bodyFreq = Music.NoteToHz(bodyNote) * (1 + math.floor(bodyIndex / 7))

        SetResonanceBody(bodyIndex, {
            freq    = bodyFreq,
            decay   = 0.5 + i * 0.8,
            gainDb  = -6 - i * 3,
            q       = 4 + i * 3,
            delayMs = i * 40,
        })
    end

    WindowIndex = (WindowIndex + 1) % kNumWindows
end
