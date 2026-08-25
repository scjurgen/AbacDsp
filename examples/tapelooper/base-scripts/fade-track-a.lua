-- Smoke test for SetTrackGain: record something onto track A, stop recording, and it
-- fades itself out over 4 seconds - no extra steps needed to see the effect.
local fadeId = nil

function OnRecordStateChanged(track, isRecording)
    if track == 0 and not isRecording then
        local steps, stepMs = 40, 100
        local i = 0
        fadeId = Timer.Every(stepMs, function()
            i = i + 1
            SetTrackGain(0, math.max(0, 1 - i / steps))
            if i >= steps then Timer.Cancel(fadeId) end
        end)
    end
end
