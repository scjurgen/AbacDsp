-- Organic Chorus script - claims the two extra live-tweak knobs beyond the six main
-- macros. Unclaimed slots simply don't show up in the Lua Controls area.
UICreateParameterSet({
    { id = "drift", name = "Drift", type = "knob", range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0.5,
      description = "Real mechanical speed wander on the tape clock, independent of Depth" },
    { id = "spread", name = "Spread", type = "knob", range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0.5,
      description = "Per-voice rate/delay detuning - 0 locks voices together, 1 is maximally independent" },
})
