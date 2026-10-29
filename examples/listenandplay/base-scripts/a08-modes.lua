import "lessonlib"

local scales = { "NaturalMinor", "Dorian", "Mixolydian", "HarmonicMinor" }

Lesson {
    name = "A8 Minor and modes", focus = FocusPitch, tempo = { 66, 88 }, scale = scales[math.random(#scales)], count = 6, maxLeap = 2,
    timing = { 80, 250 },
}
