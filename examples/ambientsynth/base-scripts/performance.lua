-- performance: the default ambientsynth patch. Harmony lives entirely here, as a plain
-- Lua table of 6 ordered regions (Drone, Major, Minor, Modal warmth, Open suspended,
-- Chromatic), each holding a few home-relative "sets" - PlayRegionHarmony{region=, set=}
-- looks one up and hands its semitones to the engine's own PlayHarmony() voice-leading.
-- Ported from the old built-in palette (bass-forward "/lo" variants dropped), plus a new
-- Drone set. Region/Harmony Set/Home are also three Lua Controls below for quick manual
-- testing - Home is a Lua Control rather than a blueprint dial specifically so it can
-- re-play the current region/set at its new transposition; a plain dial has no such hook.
--
-- Real playing is from a MIDI keyboard: white keys C4..A4 pick a region (any octave, by
-- pitch class - see kPitchClassToRegion), using whatever the Harmony Set knob is currently
-- on; any note below C4 is the bass pedal, reverting to defaultPedalNote when released -
-- unless the Hold switch is on, in which case the last pedal note holds indefinitely.

local HarmonicRegions = {
    { name = "Drone", sets = {
        { -12,0, 0.1, -0.1},
    }},
    { name = "Major", sets = {
        { -12, 11, 0, 4, 7 },
        { -12, 0, 14, 4, 7, 9 },
        { -12, 0, 14, 4, 7, 11 },
    }},
    { name = "Minor", sets = {
        { -12, 0, 14, 3, 7 },
        { -12, -2, 0, 14, 3, 7 },
        { -12, -2, 0, 14, 3, 5, 7 },
    }},
    { name = "Modal warmth", sets = {
        { -12, 8, 0, 3, 11 },
        { -12, -2, 0, 3, 7 },
        { -12, 5, -4, 0, 3, 7 },
    }},
    { name = "Open suspended", sets = {
        { -12, -2, 0, 14, 5, 9 },
        { -12, -7, -3, 0, 7 },
        { -12, -5, 0, 14 },
    }},
    { name = "Chromatic", sets = {
        { -12, -4, 1, 5 },
        { -12, -3, 1, 4 },
        { -12, -2, 1, 5, 6 },
    }},
}

-- Out-of-range set clamps to that region's actual count (e.g. Drone's single set), not
-- an error - lets every region share the same "Harmony Set" knob range.
function PlayRegionHarmony(args)
    local region = args.region or 1
    if region < 1 then region = 1 end
    if region > #HarmonicRegions then region = #HarmonicRegions end
    local sets = HarmonicRegions[region].sets
    local set = args.set or 1
    if set < 1 then set = 1 end
    if set > #sets then set = #sets end
    PlayHarmony({ semitones = sets[set] })
    print("Region harmony set: ", region, set)
end

local currentRegion = 1
local currentSet = 1
local currentHold = false
local currentHome = 4 -- E

-- Diatonic white keys from C, octave-independent - matches HarmonicRegions' own order.
-- Black keys (pitch classes 1, 3, 6, 8, 10, 11) are ignored for region selection.
local kPitchClassToRegion = {
    [0] = 1, -- C  -> Drone
    [2] = 2, -- D  -> Major
    [4] = 3, -- E  -> Minor
    [5] = 4, -- F  -> Modal warmth
    [7] = 5, -- G  -> Open suspended
    [9] = 6, -- A  -> Chromatic
}

local defaultPedalNote = 40 -- E2, two octaves below home
local currentPedalNote = nil

UICreateParameterSet({
    { id = "region", name = "Region", type = "drop",
      items = { "Drone", "Major", "Minor", "Modal warmth", "Open suspended", "Chromatic" },
      default = 0 },
    { id = "harmonySet", name = "Harmony Set", type = "drop",
      items = { "1", "2", "3" },
      default = 0 },
    { id = "hold", name = "Hold", type = "switch",
      default = 0 },
    { id = "home", name = "Home", type = "drop",
      items = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" },
      default = 4 },
})

function OnRegionChanged(value)
    currentRegion = value + 1
    PlayRegionHarmony({ region = currentRegion, set = currentSet })
end

function OnHarmonySetChanged(value)
    currentSet = value + 1
    PlayRegionHarmony({ region = currentRegion, set = currentSet })
end

-- Home has no dial of its own (see the header comment) - changing it here is the only way
-- to retranspose PlayHarmony, and it takes effect by immediately re-playing the current
-- region/set; a currently-sounding chord otherwise stays at its old pitches until then.
function OnHomeChanged(value)
    currentHome = math.floor(value)
    SetHarmonyHome(currentHome)
    PlayRegionHarmony({ region = currentRegion, set = currentSet })
end

-- On: the last pedal note (from OnNoteOn below) holds indefinitely, ignoring every
-- OnNoteOff until Hold goes off again. Off: the next OnNoteOff reverts it as usual.
function OnHoldChanged(value)
    currentHold = value >= 0.5
end

-- Real MIDI note-on/off, from a keyboard or the host - distinct from the Lua-callable
-- NoteOn()/NoteOff() a script can call directly to trigger a channel.
function OnNoteOn(channel, note, velocity)
    if note < 60 then
        currentPedalNote = note
        SetPedalNote(note)
        print("new pedal:", note)
        return
    end
    local region = kPitchClassToRegion[note % 12]
    if region then
        currentRegion = region
        PlayRegionHarmony({ region = currentRegion, set = currentSet })
        print("new region by note:", region)
        return
    end
    print("note not assigned")
end

function OnNoteOff(channel, note)
    if note == currentPedalNote and not currentHold then
        currentPedalNote = nil
        SetPedalNote(defaultPedalNote)
    end
end

function OnStart()
    for ch = 1, 16 do
        SetOscillator(ch, 0, { waveform = 0, level = 0.7, height = 0, cents = 0 })
        SetOscillator(ch, 1, { waveform = 1, level = 0.5, height = 0, cents = 4 })

        -- Every channel gets all six per-voice LFOs, phase-spread across one cycle so
        -- 16 simultaneously-sounding notes shimmer independently, not in lockstep.
        local phase = (ch - 1) * 22.5
        SetCutoffLfo(ch, 4 + ch * 0.2, 4, phase)
        SetMaterialLfo(ch, 3 + ch * 0.15, 0.15, phase)
        SetResonanceLfo(ch, 5, 0.1, phase)
        SetPitchLfo(ch, 6, 4, phase)
        SetBreathLfo(ch, 4, 1, phase)
        SetDriftLfo(ch, 5, 8, phase)
    end
    SetGain(16, 2) -- pedal channel a touch louder than the chord channels

    SetMaterial(0.4)
    SetMaterialRange(0.35)
    SetCutoff(0.5)
    SetCutoffOuRange(6)
    SetResonance(0.15)
    SetResonanceRange(0.15)
    SetBreathOuRange(2)
    SetPitchOuRange(15)
    SetBloom(0.4)
    SetFilterType(FilterType.LP4)
    SetDistortion(0) -- off; 1.. selects a WaveShaperTables.h preset

    SetChorus({ rateHz = 0.4, depth = 0.6, mix = 0.5 })
    SetReverb({ sizeMeters = 20, decayMs = 4000, dryDb = 0, mixDb = -12 })

    -- Re-asserted here, not just read: resendUiParameters() may have already fired
    -- OnHomeChanged once at Home's host-side raw default (C) before OnStart runs.
    currentHome = 4 -- E
    SetHarmonyHome(currentHome)
    SetHarmonyGlideTime(2)
    SetPedalNote(defaultPedalNote)

    PlayRegionHarmony({ region = currentRegion, set = currentSet })
end
