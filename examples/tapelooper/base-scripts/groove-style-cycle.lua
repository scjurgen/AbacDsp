-- Smoke test for SetGrooveStyle + OnLoopEnd: advances to the next style/variation each
-- time the recorded loop wraps - turn on the Groove switch to hear each swap take effect.
local kSequence = {
    { "Progressive/1_v", 0 },
    { "Progressive/1_v", 1 },
    { "Progressive/1_v", 2 },
    { "educational/four-four/backbeat", 0 },
}
local step = 0

function OnLoopEnd()
    local entry = kSequence[(step % #kSequence) + 1]
    SetGrooveStyle(entry[1], entry[2])
    step = step + 1
end
