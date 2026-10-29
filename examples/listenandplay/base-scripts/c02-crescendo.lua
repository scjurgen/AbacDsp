import "lessonlib"

local modes = { "crescendo", "decrescendo" }

Lesson {
    name = "C2 Crescendo and decrescendo", focus = FocusDynamics, tempo = { 66, 84 }, scale = "Major", fixedPitches = 1, count = 4, dynamics = modes[math.random(#modes)],
    timing = { 100, 300 }, dynamicsTolerance = { 4, 10 },
}
