#include "pch.h"
#include "DspChunk.h"

namespace SaneAudioRenderer
{
    static_assert((int32_t{-1} >> 31) == -1 && (int64_t{-1} >> 63) == -1, "Code relies on right signed shift UB");
    static_assert((int64_t)(float)((uint32_t)INT32_MAX + 1) == ((uint32_t)INT32_MAX + 1), "Rounding error");
    static_assert((int32_t)(float)(INT32_MAX - 127) == (INT32_MAX - 127), "Rounding error");
    static_assert((int32_t)(double)INT32_MAX == INT32_MAX, "Rounding error");

    namespace
    {
        inline int32_t UnpackPcm24(const int24_t& input)
        {
            int32_t x = *(reinterpret_cast<const uint16_t*>(&input));
            int32_t h = *(reinterpret_cast<const uint8_t*>(&input) + 2);
            x |= (h << 16);
            x <<= 8;
            return x;
        }

        inline void PackPcm24(const int32_t& input, int24_t& output)
        {
             *(reinterpret_cast<uint16_t*>(&output)) = (uint16_t)(input >> 8);
             *(reinterpret_cast<uint8_t*>(&output) + 2) = (uint8_t)(input >> 24);
        }

        template <DspFormat InputFormat, DspFormat OutputFormat>
        inline void ConvertSample(const typename DspFormatTraits<InputFormat>::SampleType& input,
                                  typename DspFormatTraits<OutputFormat>::SampleType& output);

        template <>
        inline void ConvertSample<DspFormat::Pcm8, DspFormat::Pcm8>(const int8_t& input, int8_t& output)
        {
            output = input;
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm8, DspFormat::Pcm16>(const int8_t& input, int16_t& output)
        {
            output = (int16_t)input << 8;
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm8, DspFormat::Pcm24>(const int8_t& input, int24_t& output)
        {
            PackPcm24((int32_t)input << 24, output);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm8, DspFormat::Pcm32>(const int8_t& input, int32_t& output)
        {
            output = (int32_t)input << 24;
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm8, DspFormat::Float>(const int8_t& input, float& output)
        {
            output = (float)input / ((int32_t)INT8_MAX + 1);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm8, DspFormat::Double>(const int8_t& input, double& output)
        {
            output = (double)input / ((int32_t)INT8_MAX + 1);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm16, DspFormat::Pcm16>(const int16_t& input, int16_t& output)
        {
            output = input;
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm16, DspFormat::Pcm24>(const int16_t& input, int24_t& output)
        {
            PackPcm24((int32_t)input << 16, output);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm16, DspFormat::Pcm32>(const int16_t& input, int32_t& output)
        {
            output = (int32_t)input << 16;
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm16, DspFormat::Float>(const int16_t& input, float& output)
        {
            output = (float)input / ((int32_t)INT16_MAX + 1);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm16, DspFormat::Double>(const int16_t& input, double& output)
        {
            output = (double)input / ((int32_t)INT16_MAX + 1);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm24, DspFormat::Pcm16>(const int24_t &input, int16_t& output)
        {
            output = *(int16_t*)(input.d + 1);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm24, DspFormat::Pcm24>(const int24_t& input, int24_t& output)
        {
            output = input;
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm24, DspFormat::Pcm32>(const int24_t& input, int32_t& output)
        {
            output = UnpackPcm24(input);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm24, DspFormat::Float>(const int24_t& input, float& output)
        {
            output = (float)UnpackPcm24(input) / ((uint32_t)INT32_MAX + 1);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm24, DspFormat::Double>(const int24_t& input, double& output)
        {
            output = (double)UnpackPcm24(input) / ((uint32_t)INT32_MAX + 1);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm32, DspFormat::Pcm16>(const int32_t& input, int16_t& output)
        {
            output = (int16_t)(input >> 16);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm32, DspFormat::Pcm24>(const int32_t& input, int24_t& output)
        {
            PackPcm24(input, output);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm32, DspFormat::Pcm32>(const int32_t& input, int32_t& output)
        {
            output = input;
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm32, DspFormat::Float>(const int32_t& input, float& output)
        {
            output = (float)input / ((uint32_t)INT32_MAX + 1);
        }

        template <>
        inline void ConvertSample<DspFormat::Pcm32, DspFormat::Double>(const int32_t& input, double& output)
        {
            output = (double)input / ((uint32_t)INT32_MAX + 1);
        }

        template <>
        inline void ConvertSample<DspFormat::Float, DspFormat::Pcm16>(const float& input, int16_t& output)
        {
            output = (int16_t)(input * INT16_MAX);
        }

        template <>
        inline void ConvertSample<DspFormat::Float, DspFormat::Pcm24>(const float& input, int24_t& output)
        {
            PackPcm24((int32_t)(input * (INT32_MAX - 127)), output);
        }

        template <>
        inline void ConvertSample<DspFormat::Float, DspFormat::Pcm32>(const float& input, int32_t& output)
        {
            //assert(fabs(input) <= 1.0f);
            output = (int32_t)(input * (INT32_MAX - 127));
        }

        template <>
        inline void ConvertSample<DspFormat::Float, DspFormat::Float>(const float& input, float& output)
        {
            output = input;
        }

        template <>
        inline void ConvertSample<DspFormat::Float, DspFormat::Double>(const float& input, double& output)
        {
            output = input;
        }

        template <>
        inline void ConvertSample<DspFormat::Double, DspFormat::Pcm16>(const double& input, int16_t& output)
        {
            output = (int16_t)(input * INT16_MAX);
        }

        template <>
        inline void ConvertSample<DspFormat::Double, DspFormat::Pcm24>(const double& input, int24_t& output)
        {
            PackPcm24((int32_t)(input * INT32_MAX), output);
        }

        template <>
        inline void ConvertSample<DspFormat::Double, DspFormat::Pcm32>(const double& input, int32_t& output)
        {
            output = (int32_t)(input * INT32_MAX);
        }

        template <>
        inline void ConvertSample<DspFormat::Double, DspFormat::Float>(const double& input, float& output)
        {
            output = (float)input;
        }

        template <>
        inline void ConvertSample<DspFormat::Double, DspFormat::Double>(const double& input, double& output)
        {
            output = input;
        }

        template <DspFormat InputFormat, DspFormat OutputFormat>
        void ConvertSamples(const char* input, typename DspFormatTraits<OutputFormat>::SampleType* output, size_t samples)
        {
            auto inputData = reinterpret_cast<const DspFormatTraits<InputFormat>::SampleType*>(input);

            for (size_t i = 0; i < samples; i++)
                ConvertSample<InputFormat, OutputFormat>(inputData[i], output[i]);
        }

        template <DspFormat OutputFormat>
        void ConvertChunk(DspChunk& chunk)
        {
            const DspFormat inputFormat = chunk.GetFormat();

            assert(!chunk.IsEmpty() && OutputFormat != inputFormat);

            DspChunk outputChunk(OutputFormat, chunk.GetChannelCount(), chunk.GetFrameCount(), chunk.GetRate());
            auto outputData = reinterpret_cast<DspFormatTraits<OutputFormat>::SampleType*>(outputChunk.GetData());

            switch (inputFormat)
            {
                case DspFormat::Pcm8:
                    ConvertSamples<DspFormat::Pcm8, OutputFormat>(chunk.GetConstData(), outputData, chunk.GetSampleCount());
                    break;

                case DspFormat::Pcm16:
                    ConvertSamples<DspFormat::Pcm16, OutputFormat>(chunk.GetConstData(), outputData, chunk.GetSampleCount());
                    break;

                case DspFormat::Pcm24:
                    ConvertSamples<DspFormat::Pcm24, OutputFormat>(chunk.GetConstData(), outputData, chunk.GetSampleCount());
                    break;

                case DspFormat::Pcm32:
                    ConvertSamples<DspFormat::Pcm32, OutputFormat>(chunk.GetConstData(), outputData, chunk.GetSampleCount());
                    break;

                case DspFormat::Float:
                    ConvertSamples<DspFormat::Float, OutputFormat>(chunk.GetConstData(), outputData, chunk.GetSampleCount());
                    break;

                case DspFormat::Double:
                    ConvertSamples<DspFormat::Double, OutputFormat>(chunk.GetConstData(), outputData, chunk.GetSampleCount());
                    break;
            }

            chunk = std::move(outputChunk);
        }
    }

    void DspChunk::ToFormat(DspFormat format, DspChunk& chunk)
    {
        assert(format != DspFormat::Pcm8);

        if (chunk.IsEmpty() || format == chunk.GetFormat())
            return;

        assert(chunk.GetFormat() != DspFormat::Unknown);

        switch (format)
        {
            case DspFormat::Pcm16:
                ConvertChunk<DspFormat::Pcm16>(chunk);
                break;

            case DspFormat::Pcm24:
                ConvertChunk<DspFormat::Pcm24>(chunk);
                break;

            case DspFormat::Pcm32:
                ConvertChunk<DspFormat::Pcm32>(chunk);
                break;

            case DspFormat::Float:
                ConvertChunk<DspFormat::Float>(chunk);
                break;

            case DspFormat::Double:
                ConvertChunk<DspFormat::Double>(chunk);
                break;
        }
    }

    DspChunk::DspChunk()
        : m_format(DspFormat::Unknown)
        , m_formatSize(1)
        , m_channels(1)
        , m_rate(1)
        , m_dataSize(0)
        , m_constData(nullptr)
        , m_delayedCopy(false)
        , m_dataOffset(0)
    {
    }

    DspChunk::DspChunk(DspFormat format, uint32_t channels, size_t frames, uint32_t rate)
        : m_format(format)
        , m_formatSize(DspFormatSize(m_format))
        , m_channels(channels)
        , m_rate(rate)
        , m_dataSize(m_formatSize * channels * frames)
        , m_constData(nullptr)
        , m_delayedCopy(false)
        , m_dataOffset(0)
    {
        assert(m_format != DspFormat::Unknown);
        Allocate();
    }

    DspChunk::DspChunk(IMediaSample* pSample, const AM_SAMPLE2_PROPERTIES& sampleProps, const WAVEFORMATEX& sampleFormat)
        : m_mediaSample(pSample)
        , m_format(DspFormatFromWaveFormat(sampleFormat))
        , m_formatSize(m_format != DspFormat::Unknown ? DspFormatSize(m_format) : sampleFormat.wBitsPerSample / 8)
        , m_channels(sampleFormat.nChannels)
        , m_rate(sampleFormat.nSamplesPerSec)
        , m_dataSize(sampleProps.lActual)
        , m_constData((char*)sampleProps.pbBuffer)
        , m_delayedCopy(true)
        , m_dataOffset(0)
    {
        assert(m_formatSize == sampleFormat.wBitsPerSample / 8);
        assert(m_mediaSample);
        assert(m_constData);

        assert(m_dataSize % GetFrameSize() == 0);
        m_dataSize = m_dataSize - m_dataSize % GetFrameSize();
    }

    DspChunk::DspChunk(DspChunk&& other)
        : m_mediaSample(other.m_mediaSample)
        , m_format(other.m_format)
        , m_formatSize(other.m_formatSize)
        , m_channels(other.m_channels)
        , m_rate(other.m_rate)
        , m_dataSize(other.m_dataSize)
        , m_constData(other.m_constData)
        , m_delayedCopy(other.m_delayedCopy)
        , m_dataOffset(0)
    {
        other.m_mediaSample = nullptr;
        std::swap(m_data, other.m_data);
        other.m_dataSize = 0;
    }

    DspChunk& DspChunk::operator=(DspChunk&& other)
    {
        if (this != &other)
        {
            m_mediaSample = other.m_mediaSample; other.m_mediaSample = nullptr;
            m_format = other.m_format;
            m_formatSize = other.m_formatSize;
            m_channels = other.m_channels;
            m_rate = other.m_rate;
            m_dataSize = other.m_dataSize; other.m_dataSize = 0;
            m_constData = other.m_constData;
            m_delayedCopy = other.m_delayedCopy;
            m_data = nullptr; std::swap(m_data, other.m_data);
            m_dataOffset = other.m_dataOffset;
        }
        return *this;
    }

    char* DspChunk::GetData()
    {
        InvokeDelayedCopy();
        return m_data.get() + m_dataOffset;
    }

    void DspChunk::ShrinkTail(size_t toFrames)
    {
        if (toFrames < GetFrameCount())
            m_dataSize = GetFormatSize() * GetChannelCount() * toFrames;
    }

    void DspChunk::ShrinkHead(size_t toFrames)
    {
        const size_t frameCount = GetFrameCount();
        if (toFrames < frameCount)
        {
            size_t shrinkBytes = (frameCount - toFrames) * GetFormatSize() * GetChannelCount();
            m_dataOffset += shrinkBytes;
            assert(m_dataSize >= shrinkBytes);
            m_dataSize -= shrinkBytes;
        }
    }

    void DspChunk::Allocate()
    {
        if (m_dataSize > 0)
        {
            m_data.reset((char*)_aligned_malloc(m_dataSize + m_dataOffset, 16));

            if (!m_data.get())
                throw std::bad_alloc();
        }
    }

    void DspChunk::InvokeDelayedCopy()
    {
        if (m_delayedCopy)
        {
            Allocate();
            assert(m_constData);
            memcpy(m_data.get(), m_constData, m_dataSize + m_dataOffset);
            m_delayedCopy = false;
        }
    }
}
