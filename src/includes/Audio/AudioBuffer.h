#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <span>
#include <stdexcept>

namespace AbacDsp
{

/**
 * @ingroup audio
 * @brief Fixed-capacity interleaved multichannel buffer with frame-wise iteration.
 *
 * Storage is one std::array, so the buffer lives inline and never allocates.
 * Interleaved layout puts a frame's channels on the same cache line, at the
 * cost of making single-channel sweeps strided.
 *
 * mux() and demux() throw on a size mismatch rather than truncating, since a
 * partially filled buffer is harder to diagnose than a thrown exception.
 */
template <size_t Channels, size_t NumFrames>
class AudioBuffer
{
  public:
    using SampleType = float;
    using Frame = std::array<SampleType, Channels>;

    class FrameProxy
    {
      public:
        explicit FrameProxy(SampleType* ptr)
            : m_ptr(ptr)
        {
        }

        [[nodiscard]] SampleType& operator[](const size_t channel) noexcept
        {
            return m_ptr[channel];
        }

        [[nodiscard]] const SampleType& operator[](const size_t channel) const noexcept
        {
            return m_ptr[channel];
        }

        [[nodiscard]] operator Frame() const noexcept
        {
            Frame frame;
            std::copy_n(m_ptr, Channels, frame.begin());
            return frame;
        }

        FrameProxy& operator=(const Frame& frame)
        {
            std::copy_n(frame.begin(), Channels, m_ptr);
            return *this;
        }

      private:
        SampleType* m_ptr;
    };

    class ConstFrameProxy
    {
      public:
        explicit ConstFrameProxy(const SampleType* ptr)
            : m_ptr(ptr)
        {
        }

        [[nodiscard]] const SampleType& operator[](const size_t channel) const noexcept
        {
            return m_ptr[channel];
        }

        [[nodiscard]] operator Frame() const noexcept
        {
            Frame frame;
            std::copy_n(m_ptr, Channels, frame.begin());
            return frame;
        }

      private:
        const SampleType* m_ptr;
    };

    class FrameIterator
    {
      public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = Frame;
        using difference_type = std::ptrdiff_t;
        using pointer = Frame*;
        using reference = Frame&;

        explicit FrameIterator(SampleType* ptr)
            : m_ptr(ptr)
        {
        }

        explicit FrameIterator(const SampleType* ptr)
            : m_ptr(const_cast<SampleType*>(ptr))
        {
        }

        [[nodiscard]] Frame operator*() const noexcept
        {
            Frame frame;
            std::copy_n(m_ptr, Channels, frame.begin());
            return frame;
        }

        [[nodiscard]] const SampleType* operator->() const noexcept
        {
            return m_ptr;
        }

        FrameIterator& operator++() noexcept
        {
            m_ptr += Channels;
            return *this;
        }

        FrameIterator operator++(int) noexcept
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        FrameIterator& operator--() noexcept
        {
            m_ptr -= Channels;
            return *this;
        }

        FrameIterator operator--(int) noexcept
        {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        FrameIterator& operator+=(const difference_type n) noexcept
        {
            m_ptr += n * static_cast<difference_type>(Channels);
            return *this;
        }

        [[nodiscard]] FrameIterator operator+(const difference_type n) const noexcept
        {
            auto tmp = *this;
            return tmp += n;
        }

        [[nodiscard]] friend FrameIterator operator+(const difference_type n, const FrameIterator& it) noexcept
        {
            return it + n;
        }

        FrameIterator& operator-=(const difference_type n) noexcept
        {
            m_ptr -= n * static_cast<difference_type>(Channels);
            return *this;
        }

        [[nodiscard]] FrameIterator operator-(const difference_type n) const noexcept
        {
            auto tmp = *this;
            return tmp -= n;
        }

        [[nodiscard]] difference_type operator-(const FrameIterator& other) const noexcept
        {
            return (m_ptr - other.m_ptr) / static_cast<difference_type>(Channels);
        }

        [[nodiscard]] Frame operator[](const difference_type n) const noexcept
        {
            return *(*this + n);
        }

        [[nodiscard]] bool operator==(const FrameIterator& other) const noexcept
        {
            return m_ptr == other.m_ptr;
        }

        [[nodiscard]] std::strong_ordering operator<=>(const FrameIterator& other) const noexcept
        {
            return m_ptr <=> other.m_ptr;
        }

      private:
        SampleType* m_ptr;
    };

    AudioBuffer() = default;

    [[nodiscard]] constexpr size_t numFrames() const noexcept
    {
        return NumFrames;
    }

    [[nodiscard]] constexpr size_t numChannels() const noexcept
    {
        return Channels;
    }

    SampleType& operator()(const size_t frame, const size_t channel)
    {
        return m_data[frame * Channels + channel];
    }

    const SampleType& operator()(const size_t frame, const size_t channel) const
    {
        return m_data[frame * Channels + channel];
    }

    FrameProxy operator[](size_t frame)
    {
        return FrameProxy(m_data.data() + frame * Channels);
    }

    ConstFrameProxy operator[](size_t frame) const
    {
        return ConstFrameProxy(m_data.data() + frame * Channels);
    }

    FrameIterator begin()
    {
        return FrameIterator(m_data.data());
    }

    FrameIterator end()
    {
        return FrameIterator(m_data.data() + m_data.size());
    }

    auto cbegin() const
    {
        return FrameIterator(m_data.data());
    }

    auto cend() const
    {
        return FrameIterator(m_data.data() + m_data.size());
    }

    void mux(const std::array<std::span<const SampleType>, Channels>& inputs)
    {
        for (size_t channel = 0; channel < Channels; ++channel)
        {
            if (inputs[channel].size() != NumFrames)
            {
                throw std::invalid_argument("Input size mismatch");
            }
            for (size_t frame = 0; frame < NumFrames; ++frame)
            {
                (*this)(frame, channel) = inputs[channel][frame];
            }
        }
    }

    void demux(const std::array<std::span<SampleType>, Channels>& outputs) const
    {
        for (size_t channel = 0; channel < Channels; ++channel)
        {
            if (outputs[channel].size() != NumFrames)
            {
                throw std::invalid_argument("Output size mismatch");
            }
            for (size_t frame = 0; frame < NumFrames; ++frame)
            {
                outputs[channel][frame] = (*this)(frame, channel);
            }
        }
    }

  private:
    std::array<SampleType, Channels * NumFrames> m_data{};
};

template <size_t NumFrames>
using MonoAudioBuffer = AudioBuffer<1, NumFrames>;

template <size_t NumFrames>
using StereoAudioBuffer = AudioBuffer<2, NumFrames>;

template <size_t NumFrames>
using SideChainStereoMonoAudioBuffer = AudioBuffer<3, NumFrames>;

template <size_t NumFrames>
using SideChainStereoStereoAudioBuffer = AudioBuffer<4, NumFrames>;

template <size_t NumFrames>
using QuadAudioBuffer = AudioBuffer<4, NumFrames>;

template <size_t NumFrames>
using CH51AudioBuffer = AudioBuffer<6, NumFrames>;

}
