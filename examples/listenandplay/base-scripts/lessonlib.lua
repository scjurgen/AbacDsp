-- lessonlib: helpers for ListenAndPlay lesson scripts. Use with: import "lessonlib"
--
-- Lesson{...} builds a whole phrase from options; the pieces below it can also be used on their own.
--
-- Lesson options (all optional unless marked):
--   name (required)    lesson name
--   focus              FocusPitch, FocusRhythm, FocusDynamics or FocusCombined (default FocusCombined)
--   tempo              {low, high} bpm range, a whole bpm is drawn from it
--   click              false to start with the click off
--   scale              name from Music.Scales (default "Major"); the tonic is drawn at random
--   degrees            scale degrees (1 = tonic) the melody may use; default all
--   span               window of the melody in semitones (default 12)
--   count              number of notes for an even rhythm, noteBeats long each (default 1 beat)
--   rhythm             list of rhythm cell names (see Cells) instead of an even rhythm
--   bars               length in bars when a rhythm is given (default 1)
--   minNotes           fewest notes a rhythm may produce (default 2)
--   pickup             anacrusis in beats (default 0)
--   maxLeap            largest melodic step in scale steps (default 2)
--   maxRepeat          how often a pitch may repeat in a row (default 1, no repeats)
--   fixedPitches       k: use only the k lowest stable pitches, drawn at random per note
--   pairs              {intervals = {semitones...}, direction = "up"|"down"|"both", count = n}
--   (every lesson names its key note with SetTonic, which the Reference button plays)
--   dynamics           flat, accent, crescendo, decrescendo, levels, backbeat, swell or echo (default flat)
--   beatsPerBar        bar length in beats (default 4)
--   motif              {beats, repeats}: a rhythm and melody of that many beats, played repeats times, the melody
--                      each time a few scale steps further, ending on a stable note; needs rhythm, no pickup
--   timing             {good, zero} onset error in ms for full / no credit (default {60, 200})
--   dynamicsTolerance  {good, zero} relative level error in dB (default {3, 9})
--   pass               percent from which the lesson counts as passed (default 70)
--   octaveFree         false to require the exact octave (default true)

local kStableOffsets = { [0] = true, [3] = true, [4] = true, [7] = true }
local kEpsilon = 1e-6

Dyn = { pp = 35, p = 55, mf = 80, f = 105, ff = 122, flat = 90 }

-- Rhythm cells: span in beats, align = beat grid the cell has to start on, notes = {offset, length}.
-- Gaps between the notes of a cell are rests.
Cells = {
    quarter = { span = 1, align = 1, notes = { { 0, 1 } } },
    half = { span = 2, align = 2, notes = { { 0, 2 } } },
    whole = { span = 4, align = 4, notes = { { 0, 4 } } },
    eighths = { span = 1, align = 1, notes = { { 0, 0.5 }, { 0.5, 0.5 } } },
    sixteenths = { span = 1, align = 1, notes = { { 0, 0.25 }, { 0.25, 0.25 }, { 0.5, 0.25 }, { 0.75, 0.25 } } },
    dottedEighth = { span = 1, align = 1, notes = { { 0, 0.75 }, { 0.75, 0.25 } } },
    triplet = { span = 1, align = 1, notes = { { 0, 1 / 3 }, { 1 / 3, 1 / 3 }, { 2 / 3, 1 / 3 } } },
    restQuarter = { span = 1, align = 1, notes = {} },
    offbeat = { span = 1, align = 1, notes = { { 0.5, 0.5 } } },
    dottedQuarter = { span = 2, align = 2, notes = { { 0, 1.5 }, { 1.5, 0.5 } } },
    syncopation = { span = 2, align = 2, notes = { { 0, 0.5 }, { 0.5, 1 }, { 1.5, 0.5 } } },
    eighth = { span = 0.5, align = 0.5, notes = { { 0, 0.5 } } },
}

local function isMultiple(value, grid)
    local ratio = value / grid
    return math.abs(ratio - math.floor(ratio + 0.5)) < kEpsilon
end

local function pickFrom(list)
    return list[math.random(#list)]
end

local function weightedPick(candidates, weights)
    local total = 0
    for i = 1, #weights do
        total = total + weights[i]
    end
    local r = math.random() * total
    for i = 1, #weights do
        r = r - weights[i]
        if r <= 0 then
            return candidates[i]
        end
    end
    return candidates[#candidates]
end

local function toSet(list)
    local set = {}
    for _, v in ipairs(list or {}) do
        set[v] = true
    end
    return set
end

-- Scale pitches within [low, high]: entries {pitch, degree, stable}. stable marks tonic, third and fifth.
function ScaleLadder(tonicPitchClass, intervals, low, high, degrees)
    local allowed = degrees and toSet(degrees) or nil
    local degreeOfOffset = {}
    for degree, offset in ipairs(intervals) do
        degreeOfOffset[offset] = degree
    end
    local ladder = { tonicClass = tonicPitchClass, low = low, high = high }
    for pitch = low, high do
        local offset = (pitch - tonicPitchClass) % 12
        local degree = degreeOfOffset[offset]
        if degree and (not allowed or allowed[degree]) then
            ladder[#ladder + 1] = { pitch = pitch, degree = degree, stable = kStableOffsets[offset] == true }
        end
    end
    return ladder
end

-- The key note of a ladder: the tonic nearest the middle of its window, inside the playable range.
function TonicOf(ladder)
    local centre = (ladder.low + ladder.high) / 2
    local best
    for pitch = RangeLow, RangeHigh do
        if pitch % 12 == ladder.tonicClass and (not best or math.abs(pitch - centre) < math.abs(best - centre)) then
            best = pitch
        end
    end
    return best or RangeLow
end

local function hasStable(ladder)
    for _, entry in ipairs(ladder) do
        if entry.stable then
            return true
        end
    end
    return false
end

-- Draws a tonic and a window of the scale inside the playable range; returns the ladder.
function PickLadder(intervals, span, degrees, minPool)
    local room = (RangeHigh - RangeLow) - span
    assert(room >= 0, "PickLadder: the playable range is narrower than the window")
    for _ = 1, 100 do
        local low = RangeLow + math.random(0, room)
        local ladder = ScaleLadder(math.random(0, 11), intervals, low, low + span, degrees)
        if #ladder >= (minPool or 3) and hasStable(ladder) then
            return ladder
        end
    end
    error("PickLadder: no window with enough notes for these options")
end

local function stepWeight(delta, index, count, arch)
    local base = ({ [0] = 1, [1] = 6, [2] = 3, [3] = 2 })[math.abs(delta)] or 1
    if arch and count > 1 and delta ~= 0 then
        local rising = (index - 1) / (count - 1) < 0.5
        if (delta > 0) == rising then
            base = base * 1.6
        end
    end
    return base
end

local function nextIndex(ladder, walk, runLength, options)
    local previous = walk[#walk]
    local lastDelta = #walk > 1 and previous - walk[#walk - 1] or 0
    local candidates, weights = {}, {}
    for delta = -options.maxLeap, options.maxLeap do
        local target = previous + delta
        local repeatOk = delta ~= 0 or runLength < options.maxRepeat
        local recoverOk = math.abs(lastDelta) < 3 or delta * lastDelta < 0
        if target >= 1 and target <= #ladder and repeatOk and recoverOk then
            candidates[#candidates + 1] = target
            weights[#weights + 1] = stepWeight(delta, #walk + 1, options.count, options.arch)
        end
    end
    if #candidates == 0 then
        return nil
    end
    return weightedPick(candidates, weights)
end

-- The ladder indexes of a melody of count notes: starts on a stable note, mostly by step, arched, and ends on a
-- stable one unless options.endStable is false. options: maxLeap (default 2), maxRepeat (default 1), arch (default true).
-- Returns nil when the ladder admits no such phrase (e.g. stable notes an even number of steps apart).
function MelodyWalk(ladder, count, options)
    options = options or {}
    local walkOptions = {
        count = count,
        maxLeap = options.maxLeap or 2,
        maxRepeat = options.maxRepeat or 1,
        arch = options.arch ~= false,
    }
    local stableIndexes = {}
    for i, entry in ipairs(ladder) do
        if entry.stable then
            stableIndexes[#stableIndexes + 1] = i
        end
    end
    for _ = 1, 100 do
        local walk = { pickFrom(stableIndexes) }
        local run = 1
        for _ = 2, count do
            local index = nextIndex(ladder, walk, run, walkOptions)
            if not index then
                break
            end
            run = index == walk[#walk] and run + 1 or 1
            walk[#walk + 1] = index
        end
        if #walk == count and (options.endStable == false or ladder[walk[count]].stable) then
            return walk
        end
    end
    return nil
end

local function pitchesOfWalk(ladder, walk)
    local pitches = {}
    for i, index in ipairs(walk) do
        pitches[i] = ladder[index].pitch
    end
    return pitches
end

-- A melody of count pitches over a ladder, see MelodyWalk.
function Melody(ladder, count, options)
    local walk = MelodyWalk(ladder, count, options)
    return walk and pitchesOfWalk(ladder, walk) or nil
end

local kMotifShifts = { 1, 2, -1, -2, 3, -3 }

local function motifShiftFits(ladder, walk, shift, repeats)
    for step = 0, repeats - 1 do
        for _, index in ipairs(walk) do
            local shifted = index + shift * step
            if shifted < 1 or shifted > #ladder then
                return false
            end
        end
    end
    return ladder[walk[#walk] + shift * (repeats - 1)].stable
end

-- A motif of motifLength notes played repeats times, each time shift scale steps further; the shift is chosen
-- so that the last note of the last repeat is a stable one. Returns nil when no motif and shift fit.
function MotifMelody(ladder, motifLength, repeats, options)
    options = options or {}
    local walkOptions = { maxLeap = options.maxLeap, maxRepeat = options.maxRepeat, arch = options.arch, endStable = false }
    for _ = 1, 100 do
        local walk = MelodyWalk(ladder, motifLength, walkOptions)
        for _, shift in ipairs(kMotifShifts) do
            if walk and motifShiftFits(ladder, walk, shift, repeats) then
                local all = {}
                for step = 0, repeats - 1 do
                    for _, index in ipairs(walk) do
                        all[#all + 1] = ladder[index + shift * step].pitch
                    end
                end
                return all
            end
        end
    end
    return nil
end

local function eligibleCells(palette, position, remaining, firstCell)
    local eligible = {}
    for _, name in ipairs(palette) do
        local cell = assert(Cells[name], "unknown rhythm cell " .. tostring(name))
        local fits = cell.span <= remaining + kEpsilon and isMultiple(position, cell.align)
        local startsSilent = #cell.notes == 0 or cell.notes[1][1] > 0
        if fits and not (firstCell and startsSilent) then
            eligible[#eligible + 1] = cell
        end
    end
    return eligible
end

local function fillRhythm(palette, beats, pickup)
    local notes = {}
    local position = 0
    while position < beats - kEpsilon do
        local eligible = eligibleCells(palette, position - pickup, beats - position, position == 0)
        if #eligible == 0 then
            return nil
        end
        local cell = pickFrom(eligible)
        for _, note in ipairs(cell.notes) do
            notes[#notes + 1] = { position = position - pickup + note[1], length = note[2] }
        end
        position = position + cell.span
    end
    return notes
end

-- Notes {position, length} filling beats (pickup included) with cells from the palette.
function BuildRhythm(palette, beats, pickup, minNotes)
    for _ = 1, 200 do
        local notes = fillRhythm(palette, beats + pickup, pickup)
        if notes and #notes >= (minNotes or 2) then
            return notes
        end
    end
    error("BuildRhythm: the cell palette cannot fill " .. beats .. " beats")
end

-- Notes {position, length} of count equal notes starting at the pickup.
function EvenRhythm(count, noteBeats, pickup)
    local notes = {}
    for i = 1, count do
        notes[i] = { position = (i - 1) * noteBeats - pickup, length = noteBeats }
    end
    return notes
end

-- One velocity per note for the dynamics mode.
function BuildDynamics(mode, count)
    local velocities = {}
    if mode == "accent" then
        local accented = count > 1 and math.random(2, count) or 1
        for i = 1, count do
            velocities[i] = i == accented and Dyn.f or Dyn.p + 20
        end
    elseif mode == "crescendo" or mode == "decrescendo" then
        for i = 1, count do
            local t = count > 1 and (i - 1) / (count - 1) or 0
            t = mode == "crescendo" and t or 1 - t
            velocities[i] = math.floor(Dyn.p + t * (Dyn.f - Dyn.p) + 0.5)
        end
    elseif mode == "backbeat" then
        for i = 1, count do
            velocities[i] = i % 2 == 0 and Dyn.f or Dyn.p + 15
        end
    elseif mode == "swell" then
        for i = 1, count do
            local t = count > 1 and 1 - math.abs(2 * (i - 1) / (count - 1) - 1) or 0
            velocities[i] = math.floor(Dyn.p + t * (Dyn.f - Dyn.p) + 0.5)
        end
    elseif mode == "echo" then
        for i = 1, count do
            velocities[i] = i % 2 == 1 and Dyn.f or Dyn.p
        end
    elseif mode == "levels" then
        local palette = { Dyn.p, Dyn.mf, Dyn.f }
        for i = 1, count do
            velocities[i] = pickFrom(palette)
        end
        if count > 1 and velocities[1] == velocities[count] then
            local same = true
            for i = 2, count do
                same = same and velocities[i] == velocities[1]
            end
            if same then
                velocities[count] = velocities[1] == Dyn.f and Dyn.p or Dyn.f
            end
        end
    else
        for i = 1, count do
            velocities[i] = Dyn.flat
        end
    end
    return velocities
end

local function applyScoring(o)
    if o.timing then
        SetTimingTolerance(o.timing[1], o.timing[2])
    end
    if o.dynamicsTolerance then
        SetDynamicsTolerance(o.dynamicsTolerance[1], o.dynamicsTolerance[2])
    end
    if o.pass then
        SetPassScore(o.pass)
    end
    if o.octaveFree ~= nil then
        SetOctaveFree(o.octaveFree)
    end
end

local function drawTempo(range)
    if range then
        SetTempo(math.random(range[1], range[2]))
    end
end

local function pitchesFromLadder(ladder, count, options)
    if options.fixedPitches then
        local pool = {}
        for _, entry in ipairs(ladder) do
            if entry.stable and #pool < options.fixedPitches then
                pool[#pool + 1] = entry.pitch
            end
        end
        local pitches = {}
        for i = 1, count do
            pitches[i] = pickFrom(pool)
        end
        return pitches
    end
    return Melody(ladder, count, options)
end

local function drawMelody(o, intervals, count)
    for _ = 1, 50 do
        local ladder = PickLadder(intervals, o.span or 12, o.degrees)
        local pitches = pitchesFromLadder(ladder, count, o)
        if pitches then
            return pitches, ladder
        end
    end
    error("Lesson: no melody fits these options")
end

local function drawMotif(o, intervals, motifLength)
    for _ = 1, 50 do
        local ladder = PickLadder(intervals, o.span or 12, o.degrees)
        local pitches = MotifMelody(ladder, motifLength, o.motif.repeats, o)
        if pitches then
            return pitches, ladder
        end
    end
    error("Lesson: no motif fits these options")
end

-- Pairs of notes a given interval apart; returns pitches (two per pair).
local function intervalPairs(pairs_, ladder)
    local inScale = {}
    for _, entry in ipairs(ladder) do
        inScale[entry.pitch] = true
    end
    local directions = pairs_.direction == "both" and { 1, -1 } or { pairs_.direction == "down" and -1 or 1 }
    local pitches = {}
    for _ = 1, pairs_.count do
        local placed = false
        for _ = 1, 200 do
            local first = pickFrom(ladder).pitch
            local second = first + pickFrom(pairs_.intervals) * pickFrom(directions)
            if second >= RangeLow and second <= RangeHigh and inScale[second] then
                pitches[#pitches + 1] = first
                pitches[#pitches + 1] = second
                placed = true
                break
            end
        end
        assert(placed, "intervalPairs: no pair of this interval fits the range")
    end
    return pitches
end

local function barsFor(notes)
    local lastEnd = 0
    for _, note in ipairs(notes) do
        lastEnd = math.max(lastEnd, note.position + note.length)
    end
    return math.max(1, math.ceil(lastEnd / BarBeats - kEpsilon))
end

local function repeatRhythm(first, repeats, beats)
    local notes = {}
    for step = 0, repeats - 1 do
        for _, note in ipairs(first) do
            notes[#notes + 1] = { position = note.position + step * beats, length = note.length }
        end
    end
    return notes
end

local function buildRhythm(o, pickup)
    if o.motif then
        assert(o.rhythm and pickup == 0, "Lesson: a motif needs a rhythm and no pickup")
        local first = BuildRhythm(o.rhythm, o.motif.beats, 0, o.minNotes)
        return repeatRhythm(first, o.motif.repeats, o.motif.beats), #first
    end
    if o.pairs then
        return EvenRhythm(o.pairs.count * 2, o.noteBeats or 1, pickup)
    end
    if o.rhythm then
        return BuildRhythm(o.rhythm, (o.bars or 1) * BarBeats, pickup, o.minNotes)
    end
    return EvenRhythm(o.count or 4, o.noteBeats or 1, pickup)
end

function Lesson(o)
    SetName(o.name)
    SetFocus(o.focus or FocusCombined)
    SetClickDefault(o.click ~= false)
    if o.beatsPerBar then
        SetBeatsPerBar(o.beatsPerBar)
    end
    applyScoring(o)
    drawTempo(o.tempo)
    local pickup = o.pickup or 0
    local rhythm, motifLength = buildRhythm(o, pickup)
    ClearPattern()
    SetPickup(pickup)
    SetPatternBars(barsFor(rhythm))
    local intervals = Music.Scales[o.scale or "Major"]
    local pitches, ladder
    if o.pairs then
        ladder = PickLadder(intervals, o.span or 12, o.degrees)
        pitches = intervalPairs(o.pairs, ladder)
    elseif o.motif then
        pitches, ladder = drawMotif(o, intervals, motifLength)
    else
        pitches, ladder = drawMelody(o, intervals, #rhythm)
    end
    SetTonic(TonicOf(ladder))
    local velocities = BuildDynamics(o.dynamics, #rhythm)
    for i, note in ipairs(rhythm) do
        AddNote(note.position, pitches[i], note.length, velocities[i])
    end
end
