-- Click while any track is recording, groove otherwise. This is the plugin's own
-- out-of-the-box default (see TapeLooperScriptEngine::kStubScript) - kept here too so it's
-- browsable/importable from the script editor's library dropdown, and so a patch that has
-- since diverged can reload it as a starting point again.
local recording = {}

function OnRecordStateChanged(track, isRecording)
    recording[track] = isRecording
    local anyRecording = recording[0] or recording[1] or recording[2]
    SetGrooveSource(anyRecording and "click" or "groove")
end
