import "lessonlib"

local scales = { "NaturalMinor", "Dorian", "HarmonicMinor" }

Lesson {
    name = "D4 Minor melody, syncopated", focus = FocusCombined, tempo = { 70, 92 }, scale = scales[math.random(#scales)], maxLeap = 3,
    rhythm = { "quarter", "eighths", "syncopation", "offbeat", "restQuarter" }, bars = 2, minNotes = 6,
    dynamics = "levels",
    timing = { 60, 200 }, dynamicsTolerance = { 3.5, 10 }, pass = 60,
}
