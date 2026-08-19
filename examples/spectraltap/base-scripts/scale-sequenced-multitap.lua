-- scale-sequenced-multitap: the plugin's own out-of-the-box default (SpectraltapScriptEngine's
-- kStubScript), kept here as a plain file too so it's visible in the script editor's library
-- dropdown and loadable via Settings > Scripts. The two are kept in sync by hand - if you edit
-- one, mirror the change in the other (see SpectraltapScriptEngine.h).
--
-- Sets up 4 taps at the 1st/3rd/5th/7th degree of Scale above Root, one each of BandPass/
-- Resonator/Formant/Notch, delayed at increasing multiples of the Division dropdown's beat
-- value against the effective BPM (manual dial, or the host's tempo while Host Sync is on),
-- alternating hard left/right pan - and retunes whenever BPM, Host Sync, Division, Root, or
-- Scale changes. Root/Scale are exposed as Lua Controls (see UICreateParameterSet below).

UICreateParameterSet({
    { id = "root", name = "Root", type = "drop",
      items = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, default = 0,
      description = "Root note the tap frequencies are built from" },
    { id = "scale", name = "Scale", type = "drop",
      items = { "Major", "NaturalMinor", "Dorian", "MajorPentatonic", "MinorPentatonic", "Blues", "WholeTone", "Chromatic" },
      default = 0,
      description = "Scale the tap frequencies are drawn from" },
})

local kScaleNames = { "Major", "NaturalMinor", "Dorian", "MajorPentatonic", "MinorPentatonic", "Blues", "WholeTone",
                      "Chromatic" }

Root = 60          -- MIDI note (C4)
Scale = kScaleNames[1]
LastBpm = 120
LastDivisionIndex = 4

TapType = { Bypass = 0, LowPass = 1, HighPass = 2, BandPass = 3, Notch = 4, Resonator = 5, Formant = 6, CombResonator = 7 }

-- Division dropdown index -> beats per quarter note; mirrors the plugin's own Division
-- list (1/1 .. 1/16T), 0-based to match the index OnTiming() passes.
local kDivisionBeats = {
    [0] = 4, [1] = 2, [2] = 3, [3] = 4 / 3,
    [4] = 1, [5] = 1.5, [6] = 2 / 3,
    [7] = 0.5, [8] = 0.75, [9] = 1 / 3,
    [10] = 0.25, [11] = 0.375, [12] = 1 / 6,
}

local degreeIndex = { 1, 3, 5, 7 } -- 1st/3rd/5th/7th scale steps
local types = { TapType.BandPass, TapType.Resonator, TapType.Formant, TapType.Notch }
local pans = { -0.6, 0.6, -0.6, 0.6 }

function RetuneTaps(bpm, divisionIndex)
    SetMaxTaps(4)
    local beats = kDivisionBeats[divisionIndex] or 1
    local scale = Music.Scales[Scale]
    for i = 1, 4 do
        local delayMs = Rhythm.BeatsToMs(beats * i, bpm)
        local hz = Music.NoteToHz(Root + scale[degreeIndex[i]])
        SetTap(i - 1, delayMs, types[i], 0.8, pans[i])
        if types[i] == TapType.Formant then
            SetFormant(i - 1, hz, 2.0, 0.6, 3.5, 0.35)
        else
            SetResonance(i - 1, hz, 1.2)
        end
    end
end

function OnRootChanged(index)
    Root = 60 + index
    RetuneTaps(LastBpm, LastDivisionIndex)
end

function OnScaleChanged(index)
    Scale = kScaleNames[index + 1]
    RetuneTaps(LastBpm, LastDivisionIndex)
end

-- Fires once at startup and again whenever the BPM dial, Host Sync, or the Division
-- dropdown changes - bpm is the effective tempo (manual dial, or the host's own tempo
-- while Host Sync is on), independent of the raw Transport.Tempo() every script gets.
function OnTiming(bpm, divisionIndex)
    LastBpm = bpm
    LastDivisionIndex = divisionIndex
    RetuneTaps(bpm, divisionIndex)
end
