-- soundscape-journey: a fully autonomous soundscape arc, triggered by a single note.
-- OnNoteOn takes that note as home/pedal and plays four sections in sequence - calm
-- widely-spaced drones that slowly build, a Lydian-mode harmonic movement (chords up to a
-- 9th), a short tense chromatic section, then quieter drones that fade out and stop. Every
-- dwell time, voice choice, and chord order is randomized within tasteful bounds, so no two
-- plays are identical. A fresh OnNoteOn cancels whatever is still running and restarts from
-- the new note.

-- Widely-spaced interval candidates (semitones from home), one per channel; a run picks
-- 5-7 of these eight pairs at random, capping the drone voice count at 7.
local kDroneIntervals = { -12, 0, 7, 12, 16, 19, 24, 28 }
local kDroneVoicingPool = {}
for i = 1, 8 do kDroneVoicingPool[i] = { channel = i, interval = kDroneIntervals[i] } end

-- Six chord voicings, every pitch class verified to lie in the Lydian scale
-- {0,2,4,6,7,9,11}, stacked in thirds up to a 9th - "easy chords" through that degree.
local kLydianChords = {
    { -12, 0, 4, 7, 11, 14 },  -- Imaj9
    { -12, 2, 6, 9, 16 },      -- II add9, the raised-4th color native to Lydian
    { -12, 4, 7, 11, 14 },     -- iii, open
    { -12, 6, 9, 12, 16 },     -- #IV stack, rooted on Lydian's own characteristic tone
    { -12, 7, 11, 14, 18 },    -- V add9(#11)
    { -12, 9, 12, 16, 19 },    -- vi, gentle
}

-- Short chromatic/tritone clusters against home for the tension section - deliberately
-- outside the Lydian set above.
local kTensionChords = {
    { -12, 0, 1, 6, 7 },
    { -12, 1, 6, 7, 13 },
    { -12, 0, 6, 7, 8 },
    { -12, 1, 2, 6, 13 },
}

local homeNote = 60
local activeTimers = {}

local function schedule(ms, fn)
    local id = Timer.After(ms, fn)
    if id ~= 0 then table.insert(activeTimers, id) end
    return id
end

local function cancelAllTimers()
    for _, id in ipairs(activeTimers) do Timer.Cancel(id) end
    activeTimers = {}
end

-- +/-10% jitter around a base duration in seconds, so replays never line up exactly.
local function jitter(seconds)
    return seconds * (0.9 + math.random() * 0.2)
end

local function shuffled(sourceArray)
    local copy = {}
    for i, v in ipairs(sourceArray) do copy[i] = v end
    for i = #copy, 2, -1 do
        local j = math.random(i)
        copy[i], copy[j] = copy[j], copy[i]
    end
    return copy
end

local function pickDroneVoicing()
    local pool = shuffled(kDroneVoicingPool)
    local voicing = {}
    for i = 1, math.random(5, 7) do voicing[i] = pool[i] end
    return voicing
end

local function applyDroneLfos(voicing, cutoffDepth, materialDepth, breathDepth, pitchDepth, rateScale)
    for _, v in ipairs(voicing) do
        local phase = math.random(0, 360)
        SetCutoffLfo(v.channel, (0.6 + math.random() * 1.2) * rateScale, cutoffDepth, phase)
        SetMaterialLfo(v.channel, (0.5 + math.random() * 1.0) * rateScale, materialDepth, phase)
        SetBreathLfo(v.channel, (0.6 + math.random() * 1.0) * rateScale, breathDepth, phase)
        SetPitchLfo(v.channel, (0.4 + math.random() * 0.8) * rateScale, pitchDepth, phase)
    end
end

local function triggerDroneVoices(voicing)
    for _, v in ipairs(voicing) do
        NoteOn(v.channel, homeNote + v.interval, math.random(45, 65))
    end
end

-- NoteOff ignores its note argument (a channel is a voice, never stolen) - this just
-- silences every non-pedal channel before a section that triggers voices directly.
local function releaseAllVoices()
    for ch = 1, 15 do NoteOff(ch, 0) end
end

-- Plain globals, not `local function`, so each section can call the next one before it is
-- defined - Lua resolves a global by name at call time, not at the point its enclosing
-- function is declared (same pattern as ambientpad's drone-chaos-cycle.lua).
function enterDroneIntro()
    local voicing = pickDroneVoicing()

    SetMaterial(0.35)
    SetMaterialRange(0.15)
    SetCutoff(0.3)
    SetResonance(0.08)
    SetFilterType(FilterType.LP4)
    SetBloom(0.65)
    SetCutoffOuRange(3)
    SetResonanceRange(0.05)
    SetBreathOuRange(1)
    SetPitchOuRange(4)

    applyDroneLfos(voicing, 3, 0.08, 0.6, 3, 1.0)
    triggerDroneVoices(voicing)

    schedule(jitter(20) * 1000, function()
        applyDroneLfos(voicing, 7, 0.18, 1.2, 6, 1.35)
        SetCutoffOuRange(8)
        SetResonanceRange(0.15)
        SetBreathOuRange(2.5)
        SetPitchOuRange(10)
        SetMaterialRange(0.3)
    end)
    schedule(jitter(42) * 1000, enterHarmonicSection)
end

function enterHarmonicSection()
    SetMaterial(0.5)
    SetMaterialRange(0.35)
    SetCutoff(0.55)
    SetResonance(0.15)
    SetFilterType(FilterType.LP2)
    SetBloom(0.45)
    SetCutoffOuRange(10)
    SetResonanceRange(0.2)
    SetBreathOuRange(3)
    SetPitchOuRange(12)
    SetHarmonyGlideTime(jitter(4))

    playLydianChord(shuffled({ 1, 2, 3, 4, 5, 6 }), 1, math.random(5, 6))
end

function playLydianChord(order, i, total)
    if i > total then
        enterTensionSection()
        return
    end
    PlayHarmony({ semitones = kLydianChords[order[i]] })
    schedule(jitter(math.random(8, 14)) * 1000, function() playLydianChord(order, i + 1, total) end)
end

function enterTensionSection()
    SetMaterial(0.7)
    SetMaterialRange(0.5)
    SetCutoff(0.75)
    SetResonance(0.55)
    SetFilterType(FilterType.BP2)
    SetBloom(0.15)
    SetCutoffOuRange(20)
    SetResonanceRange(0.35)
    SetBreathOuRange(5)
    SetPitchOuRange(25)
    SetHarmonyGlideTime(0.4)

    playTensionChord(1, math.random(3, 4))
end

function playTensionChord(i, total)
    if i > total then
        enterFinalRelaxation()
        return
    end
    PlayHarmony({ semitones = kTensionChords[math.random(#kTensionChords)] })
    schedule((2.5 + math.random() * 1.5) * 1000, function() playTensionChord(i + 1, total) end)
end

function enterFinalRelaxation()
    releaseAllVoices()
    schedule(300, function()
        local voicing = pickDroneVoicing()

        SetMaterial(0.3)
        SetMaterialRange(0.08)
        SetCutoff(0.25)
        SetResonance(0.06)
        SetFilterType(FilterType.LP4)
        SetBloom(0.8)
        SetCutoffOuRange(2)
        SetResonanceRange(0.03)
        SetBreathOuRange(0.6)
        SetPitchOuRange(2)

        applyDroneLfos(voicing, 1.5, 0.04, 0.3, 1.5, 0.8)
        triggerDroneVoices(voicing)

        schedule(jitter(38) * 1000, fadeOutAndStop)
    end)
end

function fadeOutAndStop()
    releaseAllVoices()
    SetPedalNote(0)
end

function startJourney(note)
    cancelAllTimers()
    releaseAllVoices()
    homeNote = note
    SetHarmonyHome(note % 12)
    SetPedalNote(note)
    enterDroneIntro()
end

function OnNoteOn(channel, note, velocity)
    startJourney(note)
end

function OnStart()
    for ch = 1, 16 do
        SetOscillator(ch, 0, { waveform = 0, level = 0.7, height = 0, cents = 0 })
        SetOscillator(ch, 1, { waveform = 1, level = 0.5, height = 0, cents = 4 })
    end
    SetGain(16, 2) -- pedal channel a touch louder than the rest

    SetDistortion(0)
    SetPhaser({ rateHz = 0, depth = 0, feedback = 0, mix = 0 })
    SetChorus({ rateHz = 0.25, depth = 0.5, mix = 0.4 })
    SetReverb({ sizeMeters = 30, decayMs = 6000, dryDb = 0, mixDb = -10 })

    -- Silent until the first OnNoteOn - nothing to trigger here.
end
