-- Placeholder base library script - pulled in with:
--     import "example"
-- Replace with real shared helpers; this only exists to exercise the import mechanism.

function PickFromScale(root, scaleName, degree)
    local intervals = Music.Scales[scaleName] or Music.Scales.Major
    local index = ((degree - 1) % #intervals) + 1
    return root + intervals[index]
end
