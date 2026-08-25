-- Smoke test for SetTrackFilter: a slow resonant filter sweep on track B, running as soon
-- as this loads - make sure track B has something recorded and is playing to hear it.
local phase = 0
Timer.Every(50, function()
    phase = phase + 0.02
    local cutoff = 400 + (math.sin(phase) * 0.5 + 0.5) * 3000
    SetTrackFilter(1, cutoff, 0.6, "LP4")
end)
