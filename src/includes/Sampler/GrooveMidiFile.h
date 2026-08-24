#pragma once

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "Sampler/MidiFile.h"

namespace AbacDsp
{

/// @ingroup sampler
/// @brief One note-on event read from a Standard MIDI File: absolute tick, note, velocity.
struct GrooveNoteEvent
{
    uint32_t tick{0};
    uint8_t note{0};
    uint8_t velocity{0};
};

/// @ingroup sampler
/// @brief One Set Tempo (0xFF 0x51) meta event read from a Standard MIDI File.
struct GrooveTempoEvent
{
    uint32_t tick{0};
    uint32_t microsecondsPerQuarterNote{500000};
};

/**
 * @ingroup sampler
 * @brief Reads note-on, tempo and time-signature events out of a Standard MIDI File,
 * for offline groove loading.
 *
 * Parses every track of a format 0 or 1 file (running status and sysex events skipped
 * correctly) and merges each event kind into its own tick-ordered list. Note-off and
 * zero-velocity note-on events are both treated as note-off and dropped, matching
 * standard MIDI convention. Deliberately not realtime-safe (uses std::vector/std::ifstream):
 * meant for a background load, not for driving playback directly - unlike AbacDsp::MidiFile,
 * which stays scoped to tempo/time-signature only and is written for that realtime use.
 */
class GrooveMidiFile
{
  public:
    [[nodiscard]] bool readFromFile(const std::string& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            return false;
        }
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return readFromBuffer(bytes);
    }

    [[nodiscard]] bool readFromBuffer(const std::vector<uint8_t>& bytes)
    {
        clear();
        if (bytes.size() < kHeaderSize || !matchesChunkId(bytes, 0, "MThd"))
        {
            return false;
        }
        const uint32_t headerLen = readU32BE(bytes, 4);
        if (headerLen < 6 || bytes.size() < 8 + headerLen)
        {
            return false;
        }
        const uint16_t trackCount = readU16BE(bytes, 10);
        const uint16_t division = readU16BE(bytes, 12);
        if (trackCount == 0 || (division & 0x8000) != 0) // top bit set: SMPTE frames, unsupported
        {
            return false;
        }
        m_ticksPerQuarterNote = division;

        size_t pos = 8 + headerLen;
        for (uint16_t i = 0; i < trackCount; ++i)
        {
            if (!parseTrack(bytes, pos))
            {
                return i > 0; // keep whatever tracks parsed before a truncated/malformed one
            }
        }
        std::ranges::stable_sort(m_events, {}, &GrooveNoteEvent::tick);
        std::ranges::stable_sort(m_tempoEvents, {}, &GrooveTempoEvent::tick);
        std::ranges::stable_sort(m_timeSignatures, {}, &MidiTimeSignatureEvent::tick);
        return true;
    }

    void clear() noexcept
    {
        m_events.clear();
        m_tempoEvents.clear();
        m_timeSignatures.clear();
        m_ticksPerQuarterNote = kDefaultTicksPerQuarterNote;
    }

    [[nodiscard]] const std::vector<GrooveNoteEvent>& noteEvents() const noexcept
    {
        return m_events;
    }

    [[nodiscard]] const std::vector<GrooveTempoEvent>& tempoEvents() const noexcept
    {
        return m_tempoEvents;
    }

    [[nodiscard]] const std::vector<MidiTimeSignatureEvent>& timeSignatures() const noexcept
    {
        return m_timeSignatures;
    }

    [[nodiscard]] uint16_t ticksPerQuarterNote() const noexcept
    {
        return m_ticksPerQuarterNote;
    }

  private:
    static constexpr size_t kHeaderSize = 14;
    static constexpr uint16_t kDefaultTicksPerQuarterNote = 480;

    // Advances pos past this track's chunk (including its length prefix) on success.
    [[nodiscard]] bool parseTrack(const std::vector<uint8_t>& bytes, size_t& pos)
    {
        if (pos + 8 > bytes.size() || !matchesChunkId(bytes, pos, "MTrk"))
        {
            return false;
        }
        const uint32_t length = readU32BE(bytes, pos + 4);
        pos += 8;
        if (length > bytes.size() - pos)
        {
            return false;
        }
        const size_t trackEnd = pos + length;

        uint32_t absoluteTick = 0;
        uint8_t runningStatus = 0;
        while (pos < trackEnd)
        {
            absoluteTick += readVlq(bytes, pos);
            if (pos >= trackEnd)
            {
                return false;
            }
            const uint8_t status = nextStatus(bytes, pos, runningStatus);
            if (!consumeEvent(bytes, pos, trackEnd, status, absoluteTick))
            {
                return false;
            }
        }
        pos = trackEnd;
        return true;
    }

    // Reads a new status byte (advancing pos) if present, else falls back to running status.
    // System/meta status bytes (0xF0-0xFF) never latch running status, per the MIDI spec.
    [[nodiscard]] static uint8_t nextStatus(const std::vector<uint8_t>& bytes, size_t& pos,
                                            uint8_t& runningStatus) noexcept
    {
        const uint8_t byte = bytes[pos];
        if ((byte & 0x80) == 0)
        {
            return runningStatus;
        }
        ++pos;
        if (byte < 0xF0)
        {
            runningStatus = byte;
        }
        return byte;
    }

    [[nodiscard]] bool consumeEvent(const std::vector<uint8_t>& bytes, size_t& pos, const size_t trackEnd,
                                    const uint8_t status, const uint32_t tick)
    {
        const uint8_t hi = status & 0xF0;
        if (hi == 0xF0)
        {
            return consumeMetaOrSysex(bytes, pos, trackEnd, status, tick);
        }
        if (pos >= trackEnd)
        {
            return false;
        }
        const uint8_t data1 = bytes[pos++];
        if (hi == 0xC0 || hi == 0xD0) // program change / channel pressure: one data byte
        {
            return true;
        }
        if (pos >= trackEnd)
        {
            return false;
        }
        const uint8_t data2 = bytes[pos++];
        if (hi == 0x90 && data2 > 0) // note-on with velocity > 0; zero-velocity is note-off
        {
            m_events.push_back({tick, data1, data2});
        }
        return true;
    }

    [[nodiscard]] bool consumeMetaOrSysex(const std::vector<uint8_t>& bytes, size_t& pos, const size_t trackEnd,
                                          const uint8_t status, const uint32_t tick)
    {
        uint8_t metaType = 0;
        const bool isMeta = status == 0xFF;
        if (isMeta)
        {
            if (pos >= trackEnd)
            {
                return false;
            }
            metaType = bytes[pos++];
        }
        else if (status != 0xF0 && status != 0xF7)
        {
            return false; // unrecognized status
        }
        const uint32_t len = readVlq(bytes, pos);
        if (len > trackEnd - pos)
        {
            return false;
        }
        if (isMeta && metaType == 0x51 && len == 3)
        {
            const uint32_t micros = (static_cast<uint32_t>(bytes[pos]) << 16) |
                                    (static_cast<uint32_t>(bytes[pos + 1]) << 8) |
                                    static_cast<uint32_t>(bytes[pos + 2]);
            m_tempoEvents.push_back({tick, micros});
        }
        else if (isMeta && metaType == 0x58 && len == 4)
        {
            m_timeSignatures.push_back({tick, bytes[pos], bytes[pos + 1]});
        }
        pos += len;
        return true;
    }

    [[nodiscard]] static bool matchesChunkId(const std::vector<uint8_t>& bytes, const size_t pos,
                                             const char (&id)[5]) noexcept
    {
        return pos + 4 <= bytes.size() && bytes[pos] == static_cast<uint8_t>(id[0]) &&
               bytes[pos + 1] == static_cast<uint8_t>(id[1]) && bytes[pos + 2] == static_cast<uint8_t>(id[2]) &&
               bytes[pos + 3] == static_cast<uint8_t>(id[3]);
    }

    [[nodiscard]] static uint32_t readVlq(const std::vector<uint8_t>& bytes, size_t& pos) noexcept
    {
        uint32_t value = 0;
        while (pos < bytes.size())
        {
            const uint8_t byte = bytes[pos++];
            value = (value << 7) | static_cast<uint32_t>(byte & 0x7F);
            if ((byte & 0x80) == 0)
            {
                break;
            }
        }
        return value;
    }

    [[nodiscard]] static uint32_t readU32BE(const std::vector<uint8_t>& bytes, const size_t pos) noexcept
    {
        return (static_cast<uint32_t>(bytes[pos]) << 24) | (static_cast<uint32_t>(bytes[pos + 1]) << 16) |
               (static_cast<uint32_t>(bytes[pos + 2]) << 8) | static_cast<uint32_t>(bytes[pos + 3]);
    }

    [[nodiscard]] static uint16_t readU16BE(const std::vector<uint8_t>& bytes, const size_t pos) noexcept
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(bytes[pos]) << 8) | bytes[pos + 1]);
    }

    std::vector<GrooveNoteEvent> m_events;
    std::vector<GrooveTempoEvent> m_tempoEvents;
    std::vector<MidiTimeSignatureEvent> m_timeSignatures;
    uint16_t m_ticksPerQuarterNote{kDefaultTicksPerQuarterNote};
};

}
