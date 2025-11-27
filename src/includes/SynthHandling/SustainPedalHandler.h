#pragma once

#include <array>
#include <functional>
#include <optional>


/* rules:
 * noteOn Handler:
 *     if queue full: search note with lowest count and send noteOff, mark slow isUsed=false
 *     if not in sustain mode:
 *             call m_noteOn
 *             save as active note
 *     in sustain mode:
 *         save as active note (reset the scheduled for off state, call m_noteOn)
 *
 * noteOff Handler:
 *     if not sustaining:
 *          search in m_activeNotes, call m_noteOff, m_activeNotes isUsed=false
 *     if sustaining:
 *         mark active note as scheduled for off
 *
 * sustain from off to on:
 *     guess nothing to do
 * sustain from on to off
 *     retrieve notes scheduled for off and send note_off, remove from active notes
 *     don't touch notes that are still currently hold
 *
 *
 * allNotesOff
 */
class SustainPedalHandler
{
    struct ActiveNoteSlots
    {
        bool isUsed = false;
        bool isScheduledForOff = false;
        int channel = 0;
        int note = 0;
        int noteOffVelocity = 0;
        int count = 0;
    };

  public:
    explicit SustainPedalHandler(const size_t maxVoices = 16)
        : m_activeNotes(maxVoices)
        , m_counter(0)
    {
    }

    using MidiCallback = std::function<void(int, int, int)>;

    void configureCallbacks(MidiCallback noteOn, MidiCallback noteOff) noexcept
    {
        m_noteOn = std::move(noteOn);
        m_noteOff = std::move(noteOff);
    }

    void noteOn(const int channel, const int note, const int velocity) noexcept
    {
        int slotIndex = -1;

        // First check for existing note to replace in sustain mode
        if (m_sustainActive)
        {
            for (int i = 0; i < m_activeNotes.size(); ++i)
            {
                auto& slot = m_activeNotes[i];
                if (slot.isUsed && slot.channel == channel && slot.note == note)
                {
                    // Force release of existing note before retriggering
                    if (m_noteOff)
                    {
                        m_noteOff(channel, note, slot.noteOffVelocity);
                    }
                    slot.isUsed = false;
                    slotIndex = i;
                    break;
                }
            }
        }

        // If no replacement found, find new slot
        if (slotIndex == -1)
        {
            slotIndex = findFreeSlot();

            // Voice stealing when full
            if (slotIndex == -1)
            {
                slotIndex = findVoiceToSteal();
                auto& stolenSlot = m_activeNotes[slotIndex];
                if (m_noteOff)
                {
                    m_noteOff(stolenSlot.channel, stolenSlot.note, stolenSlot.noteOffVelocity);
                }
                stolenSlot.isUsed = false;
            }
        }

        // Update slot with new note information
        auto& slot = m_activeNotes[slotIndex];
        slot = {true, false, channel, note, velocity, m_counter++};

        // Always trigger noteOn (MIDI spec requirement)
        if (m_noteOn)
        {
            m_noteOn(channel, note, velocity);
        }
    }


    void noteOff(const int channel, const int note, const int velocity) noexcept
    {
        for (auto& slot : m_activeNotes)
        {
            if (slot.isUsed && slot.channel == channel && slot.note == note)
            {
                if (!m_sustainActive)
                {
                    if (m_noteOff)
                    {
                        m_noteOff(channel, note, velocity);
                    }
                    slot.isUsed = false;
                }
                else
                {
                    slot.isScheduledForOff = true;
                    slot.noteOffVelocity = velocity;
                }
                break;
            }
        }
    }

    void allNotesOff() noexcept
    {
        for (auto& slot : m_activeNotes)
        {
            if (slot.isUsed)
            {
                if (m_noteOff)
                {
                    m_noteOff(slot.channel, slot.note, slot.noteOffVelocity);
                }
                slot.isUsed = false;
                slot.isScheduledForOff = false;
            }
        }
        m_sustainActive = false;
        m_counter = 0;
    }

    void setSustain(const bool sustainOn) noexcept
    {
        if (m_sustainActive && !sustainOn)
        {
            for (auto& slot : m_activeNotes)
            {
                if (slot.isScheduledForOff)
                {
                    if (m_noteOff)
                    {
                        m_noteOff(slot.channel, slot.note, slot.noteOffVelocity);
                    }
                    slot.isUsed = false;
                }
                slot.isScheduledForOff = false;
            }
        }
        m_sustainActive = sustainOn;
    }

  private:
    int findFreeSlot() const
    {
        for (int i = 0; i < m_activeNotes.size(); ++i)
        {
            if (!m_activeNotes[i].isUsed)
            {
                return i;
            }
        }
        return -1;
    }

    int findVoiceToSteal() const
    {
        int lowestCount = std::numeric_limits<int>::max();
        int lowestIndex = -1;

        for (int i = 0; i < m_activeNotes.size(); ++i)
        {
            const auto& slot = m_activeNotes[i];
            if (slot.isUsed && slot.count < lowestCount)
            {
                lowestCount = slot.count;
                lowestIndex = i;
            }
        }
        return lowestIndex;
    }

    std::vector<ActiveNoteSlots> m_activeNotes;
    MidiCallback m_noteOn = nullptr;
    MidiCallback m_noteOff = nullptr;
    bool m_sustainActive = false;
    int m_counter;
};
