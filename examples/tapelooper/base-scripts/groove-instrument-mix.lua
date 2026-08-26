-- Smoke test for SetInstrumentGain/MuteInstrument/SetInstrumentReverbSend: turn on the
-- Groove switch to hear it - kick and snare drop out, and the closed hihat's reverb send
-- swells in over ~4 seconds. Instrument names match the shipped default kit's own pieces.
MuteInstrument("kick")
MuteInstrument("snare")

local i = 0
Timer.Every(200, function()
    i = i + 1
    SetInstrumentReverbSend("hihat_closed", math.min(0.6, i / 20))
end)
