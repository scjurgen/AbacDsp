#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace AbacDsp
{

// A single Time Signature meta event at an absolute tick position.
/// @ingroup sampler
/// @brief A time signature at a tick position.
/// denominatorPower follows the MIDI convention denominator = 2^denominatorPower,
/// so 2 means a quarter note and 3 an eighth.
struct MidiTimeSignatureEvent
{
    uint32_t tick{0};
    uint8_t numerator{4};
    uint8_t denominatorPower{2};
};

/**
 * @ingroup sampler
 * @brief Minimal Format 0 MIDI file holding only tempo and time-signature events.
 *
 * No note data and no general MIDI parsing. Writing the tempo map as a real
 * .mid rather than a private format means any DAW can import a take's timing
 * directly, and it costs a few hundred lines instead of a MIDI library.
 */
class MidiFile
{
  public:
    static constexpr uint16_t kTicksPerQuarterNote = 480;

    void setTempoBpm(const float bpm) noexcept
    {
        m_bpm = bpm;
    }

    [[nodiscard]] float tempoBpm() const noexcept
    {
        return m_bpm;
    }

    [[nodiscard]] uint16_t ticksPerQuarterNote() const noexcept
    {
        return m_division;
    }

    // Events must be added in increasing tick order; the first is expected at
    // tick 0 (mirrors MeterTimeline's own segment-ordering contract).
    void addTimeSignature(const uint32_t tick, const uint8_t numerator, const uint8_t denominatorPower)
    {
        m_timeSignatures.push_back({tick, numerator, denominatorPower});
    }

    [[nodiscard]] const std::vector<MidiTimeSignatureEvent>& timeSignatures() const noexcept
    {
        return m_timeSignatures;
    }

    void clear() noexcept
    {
        m_bpm = 120.f;
        m_division = kTicksPerQuarterNote;
        m_timeSignatures.clear();
    }

    // Encodes the current tempo + time-signature events into a Format 0 SMF byte buffer.
    [[nodiscard]] std::vector<uint8_t> writeToBuffer() const
    {
        std::vector<uint8_t> track;
        appendTempoEvent(track);
        uint32_t lastTick = 0;
        for (const auto& ts : m_timeSignatures)
        {
            appendVlq(track, ts.tick - lastTick);
            lastTick = ts.tick;
            track.push_back(0xFF);
            track.push_back(0x58);
            track.push_back(0x04);
            track.push_back(ts.numerator);
            track.push_back(ts.denominatorPower);
            track.push_back(24); // MIDI clocks per metronome click (standard default)
            track.push_back(8);  // 32nd notes per quarter note (standard default)
        }
        appendVlq(track, 0);
        track.push_back(0xFF);
        track.push_back(0x2F);
        track.push_back(0x00);

        std::vector<uint8_t> file;
        file.reserve(kHeaderSize + kChunkPrefixSize + track.size());
        appendChunkId(file, "MThd");
        appendU32BE(file, 6);
        appendU16BE(file, 0); // format 0
        appendU16BE(file, 1); // ntrks
        appendU16BE(file, kTicksPerQuarterNote);
        appendChunkId(file, "MTrk");
        appendU32BE(file, static_cast<uint32_t>(track.size()));
        file.insert(file.end(), track.begin(), track.end());
        return file;
    }

    [[nodiscard]] bool writeToFile(const std::string& path) const
    {
        const std::vector<uint8_t> bytes = writeToBuffer();
        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            return false;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return out.good();
    }

    // Parses a Format 0/1 SMF byte buffer (only the first track is read),
    // replacing any current tempo/time-signature state. Returns false if the
    // buffer isn't a recognizable, well-formed Standard MIDI File.
    [[nodiscard]] bool readFromBuffer(const std::vector<uint8_t>& bytes)
    {
        if (bytes.size() < kHeaderSize || !matchesChunkId(bytes, 0, "MThd"))
        {
            return false;
        }
        const uint32_t headerLen = readU32BE(bytes, 4);
        if (headerLen < 6 || bytes.size() < 8 + headerLen)
        {
            return false;
        }
        const uint16_t ntrks = readU16BE(bytes, 10);
        const uint16_t division = readU16BE(bytes, 12);
        if (ntrks == 0 || (division & 0x8000) != 0) // top bit set: SMPTE frames, unsupported
        {
            return false;
        }

        size_t pos = 8 + headerLen;
        if (pos + kChunkPrefixSize > bytes.size() || !matchesChunkId(bytes, pos, "MTrk"))
        {
            return false;
        }
        const uint32_t trackLen = readU32BE(bytes, pos + 4);
        pos += kChunkPrefixSize;
        const size_t trackEnd = pos + trackLen;
        if (trackLen > bytes.size() - pos)
        {
            return false;
        }

        clear();
        m_division = division;
        return parseTrack(bytes, pos, trackEnd);
    }

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

  private:
    static constexpr size_t kHeaderSize = 14;
    static constexpr size_t kChunkPrefixSize = 8; // 4-byte id + 4-byte length

    void appendTempoEvent(std::vector<uint8_t>& track) const
    {
        appendVlq(track, 0);
        const auto micros =
            static_cast<uint32_t>(std::lround(60000000.0 / static_cast<double>(m_bpm > 0.f ? m_bpm : 120.f)));
        track.push_back(0xFF);
        track.push_back(0x51);
        track.push_back(0x03);
        track.push_back(static_cast<uint8_t>((micros >> 16) & 0xFF));
        track.push_back(static_cast<uint8_t>((micros >> 8) & 0xFF));
        track.push_back(static_cast<uint8_t>(micros & 0xFF));
    }

    // Returns false on any structural inconsistency (truncated event, bad meta
    // length); whatever tempo/time-signatures were parsed before that point are
    // kept rather than discarded.
    [[nodiscard]] bool parseTrack(const std::vector<uint8_t>& bytes, size_t pos, const size_t trackEnd)
    {
        uint32_t absoluteTick = 0;
        while (pos < trackEnd)
        {
            absoluteTick += readVlq(bytes, pos);
            if (pos >= trackEnd)
            {
                return false;
            }
            const uint8_t status = bytes[pos++];
            if (status != 0xFF)
            {
                return false; // only meta events are supported (no notes expected)
            }
            if (pos >= trackEnd)
            {
                return false;
            }
            const uint8_t metaType = bytes[pos++];
            const uint32_t len = readVlq(bytes, pos);
            if (len > trackEnd - pos)
            {
                return false;
            }
            if (metaType == 0x51 && len == 3)
            {
                const uint32_t micros = (static_cast<uint32_t>(bytes[pos]) << 16) |
                                        (static_cast<uint32_t>(bytes[pos + 1]) << 8) |
                                        static_cast<uint32_t>(bytes[pos + 2]);
                if (micros > 0)
                {
                    m_bpm = static_cast<float>(60000000.0 / static_cast<double>(micros));
                }
            }
            else if (metaType == 0x58 && len == 4)
            {
                m_timeSignatures.push_back({absoluteTick, bytes[pos], bytes[pos + 1]});
            }
            else if (metaType == 0x2F)
            {
                return true; // End of Track
            }
            pos += len;
        }
        return true;
    }

    static void appendChunkId(std::vector<uint8_t>& buf, const char (&id)[5])
    {
        buf.insert(buf.end(), id, id + 4);
    }

    [[nodiscard]] static bool matchesChunkId(const std::vector<uint8_t>& bytes, const size_t pos, const char (&id)[5])
    {
        return pos + 4 <= bytes.size() && bytes[pos] == static_cast<uint8_t>(id[0]) &&
               bytes[pos + 1] == static_cast<uint8_t>(id[1]) && bytes[pos + 2] == static_cast<uint8_t>(id[2]) &&
               bytes[pos + 3] == static_cast<uint8_t>(id[3]);
    }

    static void appendVlq(std::vector<uint8_t>& buf, uint32_t value)
    {
        std::array<uint8_t, 5> stack{};
        size_t count = 0;
        stack[count++] = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        while (value > 0)
        {
            stack[count++] = static_cast<uint8_t>(value & 0x7F);
            value >>= 7;
        }
        for (size_t i = count; i-- > 0;)
        {
            const uint8_t byte = static_cast<uint8_t>(stack[i] | (i != 0 ? 0x80 : 0x00));
            buf.push_back(byte);
        }
    }

    [[nodiscard]] static uint32_t readVlq(const std::vector<uint8_t>& bytes, size_t& pos)
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

    static void appendU32BE(std::vector<uint8_t>& buf, const uint32_t value)
    {
        buf.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(value & 0xFF));
    }

    static void appendU16BE(std::vector<uint8_t>& buf, const uint16_t value)
    {
        buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(value & 0xFF));
    }

    [[nodiscard]] static uint32_t readU32BE(const std::vector<uint8_t>& bytes, const size_t pos)
    {
        return (static_cast<uint32_t>(bytes[pos]) << 24) | (static_cast<uint32_t>(bytes[pos + 1]) << 16) |
               (static_cast<uint32_t>(bytes[pos + 2]) << 8) | static_cast<uint32_t>(bytes[pos + 3]);
    }

    [[nodiscard]] static uint16_t readU16BE(const std::vector<uint8_t>& bytes, const size_t pos)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(bytes[pos]) << 8) | bytes[pos + 1]);
    }

    float m_bpm{120.f};
    uint16_t m_division{kTicksPerQuarterNote};
    std::vector<MidiTimeSignatureEvent> m_timeSignatures;
};

}
