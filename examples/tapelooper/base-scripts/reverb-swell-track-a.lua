-- Smoke test for SetTrackReverbSend: reverb swells in on track A over ~4 seconds as soon as
-- it stops recording - record something onto track A and stop recording to hear it build.
function OnRecordStateChanged(track, isRecording)
    if track == 0 and not isRecording then
        local i = 0
        Timer.Every(200, function()
            i = i + 1
            SetTrackReverbSend(0, math.min(0.6, i / 20))
        end)
    end
end
