#include "pch.h"
#include "DspDither.h"

namespace SaneAudioRenderer
{
    namespace
    {
        template <DspFormat DitherFormat, typename T>
        inline T Expand(T input);

        template <>
        inline float Expand<DspFormat::Pcm16>(float input)
        {
            assert(std::abs(input) <= 1.0f);
            return input * (INT16_MAX - 1);
        }

        template <>
        inline double Expand<DspFormat::Pcm16>(double input)
        {
            assert(std::abs(input) <= 1.0);
            return input * (INT16_MAX - 1);
        }

        template <>
        inline double Expand<DspFormat::Pcm24>(double input)
        {
            assert(std::abs(input) <= 1.0);
            return input * ((INT32_MAX >> 8) - 1);
        }

        template <>
        inline double Expand<DspFormat::Pcm24in32>(double input)
        {
            return Expand<DspFormat::Pcm24>(input);
        }

        template <DspFormat OutputFormat, typename T>
        inline void Round(T input, typename DspFormatTraits<OutputFormat>::SampleType& output);

        template <>
        inline void Round<DspFormat::Pcm16, float>(float input, int16_t& output)
        {
            auto outputSample = std::round(input);
            assert(outputSample >= INT16_MIN && outputSample <= INT16_MAX);
            output = (int16_t)outputSample;
        }

        template <>
        inline void Round<DspFormat::Pcm16, double>(double input, int16_t& output)
        {
            auto outputSample = std::round(input);
            assert(outputSample >= INT16_MIN && outputSample <= INT16_MAX);
            output = (int16_t)outputSample;
        }

        template <>
        inline void Round<DspFormat::Pcm24, double>(double input, int24_t& output)
        {
            auto outputSample = std::round(input);
            assert(outputSample >= (INT32_MIN >> 8) && outputSample <= (INT32_MAX >> 8));
            *(reinterpret_cast<uint16_t*>(&output)) = (uint16_t)(input);
            *(reinterpret_cast<uint8_t*>(&output) + 2) = (uint8_t)((uint32_t)input >> 16);
        }

        template <>
        inline void Round<DspFormat::Pcm24in32, double>(double input, int32_t& output)
        {
            auto outputSample = std::round(input);
            assert(outputSample >= (INT32_MIN >> 8) && outputSample <= (INT32_MAX >> 8));
            output = (uint32_t)outputSample << 8;
        }
    }

    void DspDither::Initialize(ISettings* pSettings, DspFormat outputFormat)
    {
        assert(pSettings);

        m_outputFormat = outputFormat;
        m_active = false;

        for (size_t i = 0; i < 18; i++)
        {
            m_previous[i] = 0.0f;
            m_generatorSimple[i].seed((uint32_t)(GetPerformanceCounter() + i));
            m_generatorComplex[i].seed((uint32_t)(GetPerformanceCounter() + i));
            m_distributor[i] = std::uniform_real_distribution<float>(0, 1.0f);
        }

        SetSettings(pSettings);
    }

    bool DspDither::Active()
    {
        return m_active;
    }

    void DspDither::Process(DspChunk& chunk)
    {
        m_active = false;

        if (chunk.IsEmpty())
            return;

        CheckSettings();

        switch (m_outputFormat)
        {
            case DspFormat::Pcm16:
            {
                if (chunk.GetFormatSize() <= 2)
                    return;

                if (chunk.GetFormat() == DspFormat::Double)
                {
                    m_extraPrecision ? Dither<DspFormat::Double, DspFormat::Pcm16, true>(chunk) :
                                       Dither<DspFormat::Double, DspFormat::Pcm16, false>(chunk);
                }
                else
                {
                    m_extraPrecision ? Dither<DspFormat::Float, DspFormat::Pcm16, true>(chunk) :
                                       Dither<DspFormat::Float, DspFormat::Pcm16, false>(chunk);
                }

                break;
            }

            case DspFormat::Pcm24:
            {
                if (!m_extraPrecision || chunk.GetFormatSize() <= 3)
                    return;

                Dither<DspFormat::Double, DspFormat::Pcm24, true>(chunk);

                break;
            }

            case DspFormat::Pcm24in32:
            {
                if (!m_extraPrecision || chunk.GetFormatSize() <= 3)
                    return;

                Dither<DspFormat::Double, DspFormat::Pcm24in32, true>(chunk);

                break;
            }

            default:
                return;
        }

        m_active = true;
    }

    void DspDither::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }

    void DspDither::SettingsUpdated()
    {
        m_extraPrecision = !!m_settings->GetExtraPrecisionProcessing();
    }

    template <DspFormat DitherFormat, DspFormat OutputFormat, bool complex>
    void DspDither::Dither(DspChunk& chunk)
    {
        using DitherSampleType = DspFormatTraits<DitherFormat>::SampleType;
        using OutputSampleType = DspFormatTraits<OutputFormat>::SampleType;

        DspChunk::ToFormat(DitherFormat, chunk);

        DspChunk output(OutputFormat, chunk.GetChannelCount(), chunk.GetFrameCount(), chunk.GetRate());

        auto inputData = reinterpret_cast<const DitherSampleType*>(chunk.GetData());
        auto outputData = reinterpret_cast<OutputSampleType*>(output.GetData());
        const size_t channels = chunk.GetChannelCount();

        for (size_t frame = 0, frames = chunk.GetFrameCount(); frame < frames; frame++)
        {
            for (size_t channel = 0; channel < channels; channel++)
            {
                auto inputSample = Expand<OutputFormat>(inputData[frame * channels + channel]);

                // High-pass TPDF, 2 LSB amplitude.
                float r = complex ? m_distributor[channel](m_generatorComplex[channel]) :
                                    m_distributor[channel](m_generatorSimple[channel]);
                float noise = r - m_previous[channel];
                m_previous[channel] = r;

                Round<OutputFormat>(inputSample + noise, outputData[frame * channels + channel]);
            }
        }

        chunk = std::move(output);
    }
}
