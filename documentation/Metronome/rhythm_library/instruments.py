"""Note-number palette, drawn from this project's own GrooveNoteMap (see
src/includes/Sampler/GrooveNoteMap.h and MidiDrums/README.md), not General MIDI. See PLAN.md,
"Instrument palette", for the rationale behind each substitution (in particular: no dedicated
claves voice, so clave/cascara patterns use WOODBLOCK/RIMSHOT/SIDESTICK)."""

from __future__ import annotations

KICK = 36
CLICK_LOW = 100
CLICK_HIGH = 101
SNARE = 38
SNARE_ALT = 40
RIMSHOT = 37
SIDESTICK = 71
HIHAT_CLOSED = 42
HIHAT_OPEN = 49
WOODBLOCK = 56
RIDE = 51
CRASH = 27
TOM1 = 43

# Middle Eastern dum/tak notation.
DUM = KICK
TAK = RIMSHOT

VEL_CUE = 118  # section/start cue
VEL_ACCENT = 108  # primary downbeat accent
VEL_SECONDARY = 96  # secondary accent
VEL_NORMAL = 78  # normal click
VEL_LIGHT = 54  # light subdivision
VEL_GHOST = 38  # ghost/subtle guide

CLICK_DURATION_TICKS = 30
CUE_DURATION_TICKS = 45

NOTE_NAMES = {
    KICK: "kick",
    CLICK_LOW: "click_low",
    CLICK_HIGH: "click_high",
    SNARE: "snare",
    SNARE_ALT: "snare_alt",
    RIMSHOT: "rimshot",
    SIDESTICK: "sidestick",
    HIHAT_CLOSED: "hihat_closed",
    HIHAT_OPEN: "hihat_open",
    WOODBLOCK: "woodblock",
    RIDE: "ride",
    CRASH: "crash",
    TOM1: "tom1",
}
