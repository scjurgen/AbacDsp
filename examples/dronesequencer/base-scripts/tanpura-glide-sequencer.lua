-- tanpura-glide-sequencer: a Tanpura-style stepped pattern player for DroneSequencer, where
-- the 1st and 2nd pattern notes (this library's "Harmonic1"/"Harmonic2", the same idea as
-- Tanpura's own H1/H2 roles) can each independently glide in from the other's pitch, the
-- way Tanpura's own slide does - but expressed through DroneSequencer's per-note
-- `slide`/`slideTime` fields instead of a fixed C++ mechanism, so it's entirely configured
-- from here, no blueprint/UI changes needed.
--
-- A patch script configures it by setting:
--
--   GlidePattern
--       Array of { note = <midi note>, velocity = <0..1>, pause = <extra silent steps
--       after this note> } entries, walked in order and looped. The 1st and 2nd entries
--       are the glide-eligible pair - every other entry always plucks straight.
--   GlideTimeMs (optional, default 150)
--       Milliseconds for a glide to resolve once it starts.
--
-- This library also declares a "Glide Probability" knob (0..1): each time the 1st or 2nd
-- pattern entry comes up, it independently rolls this probability to decide whether *that*
-- occurrence glides in from the other harmonic's pitch this time - Tanpura's own per-pluck
-- Bernoulli trial, just rolled for both roles instead of only Harmonic1.

GlidePattern = GlidePattern or { { note = 60, velocity = 0.8, pause = 0 } }
GlideTimeMs = GlideTimeMs or 150
GlideProbability = GlideProbability or 0
GlideStep = GlideStep or 1
GlidePauseRemaining = GlidePauseRemaining or 0
GlideChannelStep = GlideChannelStep or 0

UICreateParameterSet({
    { id = "glideProbability", name = "Glide Probability", type = "knob",
      range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0,
      description = "Chance the 1st or 2nd pattern note glides in from the other's pitch" },
})

function OnGlideProbabilityChanged(value)
    GlideProbability = value
end

function OnStart()
    GlideStep = 1
    GlidePauseRemaining = 0
    GlideChannelStep = 0
end

-- Semitone offset and time this entry should glide in from, or 0, 0 if it doesn't glide
-- this time - only the 1st and 2nd pattern entries are ever eligible.
local function rollGlide(index)
    if #GlidePattern < 2 or (index ~= 1 and index ~= 2) then
        return 0, 0
    end
    if math.random() >= GlideProbability then
        return 0, 0
    end
    local otherIndex = (index == 1) and 2 or 1
    return GlidePattern[otherIndex].note - GlidePattern[index].note, GlideTimeMs
end

function NextNotes()
    if GlidePauseRemaining > 0 then
        GlidePauseRemaining = GlidePauseRemaining - 1
        return {}
    end

    local index = GlideStep
    local entry = GlidePattern[index]
    GlideStep = GlideStep + 1
    if GlideStep > #GlidePattern then
        GlideStep = 1
    end
    GlideChannelStep = GlideChannelStep + 1
    if GlideChannelStep >= 16 then
        GlideChannelStep = 0
    end
    GlidePauseRemaining = entry.pause or 0

    local slideAmount, slideTime = rollGlide(index)
    return {
        { note = entry.note, velocity = entry.velocity, channel = GlideChannelStep, length = 0, delay = 0,
          slide = slideAmount, slideTime = slideTime },
    }
end
