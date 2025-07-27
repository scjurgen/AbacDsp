#pragma once

#include <array>
#include <span>
#include <stdexcept>

template <size_t Channels, size_t NumFrames>
class AudioBuffer
{
  public:
    using SampleType = float;
    using Frame = std::array<SampleType, Channels>;

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

        Frame operator*() const
        {
            Frame frame;
            std::copy_n(m_ptr, Channels, frame.begin());
            return frame;
        }

        const SampleType* operator->() const
        {
            return m_ptr;
        }

        FrameIterator& operator++()
        {
            m_ptr += Channels;
            return *this;
        }
        FrameIterator operator++(int)
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }
        FrameIterator& operator--()
        {
            m_ptr -= Channels;
            return *this;
        }
        FrameIterator operator--(int)
        {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        FrameIterator& operator+=(difference_type n)
        {
            m_ptr += n * Channels;
            return *this;
        }

        FrameIterator operator+(difference_type n) const
        {
            auto tmp = *this;
            return tmp += n;
        }

        friend FrameIterator operator+(difference_type n, const FrameIterator& it)
        {
            return it + n;
        }

        FrameIterator& operator-=(difference_type n)
        {
            m_ptr -= n * Channels;
            return *this;
        }

        FrameIterator operator-(difference_type n) const
        {
            auto tmp = *this;
            return tmp -= n;
        }

        difference_type operator-(const FrameIterator& other) const
        {
            return (m_ptr - other.m_ptr) / Channels;
        }

        Frame operator[](difference_type n) const
        {
            return *(*this + n);
        }

        bool operator==(const FrameIterator& other) const
        {
            return m_ptr == other.m_ptr;
        }

        bool operator!=(const FrameIterator& other) const
        {
            return !(*this == other);
        }

        bool operator<(const FrameIterator& other) const
        {
            return m_ptr < other.m_ptr;
        }

        bool operator>(const FrameIterator& other) const
        {
            return other < *this;
        }

        bool operator<=(const FrameIterator& other) const
        {
            return !(other < *this);
        }

        bool operator>=(const FrameIterator& other) const
        {
            return !(*this < other);
        }

        std::strong_ordering operator<=>(const FrameIterator& other) const
        {
            return m_ptr <=> other.m_ptr;
        }

      private:
        SampleType* m_ptr;
    };

    AudioBuffer() = default;

    [[nodiscard]] constexpr size_t numFrames() const
    {
        return NumFrames;
    }
    [[nodiscard]] constexpr size_t numChannels() const
    {
        return Channels;
    }

    SampleType& operator()(size_t frame, size_t channel)
    {
        return m_data[frame * Channels + channel];
    }

    const SampleType& operator()(size_t frame, size_t channel) const
    {
        return m_data[frame * Channels + channel];
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

// Type aliases
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
