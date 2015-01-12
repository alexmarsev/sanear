#include "pch.h"
#include "DspDither.h"

namespace SaneAudioRenderer
{
    void DspDither::Initialize(DspFormat outputFormat)
    {
        m_active = (outputFormat == DspFormat::Pcm16);
        m_error1 = {};
        m_error2 = {};
    }

    bool DspDither::Active()
    {
        return m_active;
    }

    void DspDither::Process(DspChunk& chunk)
    {
        // TODO: handle >18 channels situation gracefully (both here and in DspMatrix)
        if (m_active && !chunk.IsEmpty() && chunk.GetFormatSize() > DspFormatSize(DspFormat::Pcm16))
        {
            DspChunk::ToFloat(chunk);

            DspChunk output(DspFormat::Pcm16, chunk.GetChannelCount(), chunk.GetFrameCount(), chunk.GetRate());

            auto inputData = reinterpret_cast<const float*>(chunk.GetData());
            auto outputData = reinterpret_cast<int16_t*>(output.GetData());
            const size_t channels = chunk.GetChannelCount();

            for (size_t frame = 0, frames = chunk.GetFrameCount(); frame < frames; frame++)
            {
                for (size_t channel = 0; channel < channels; channel++)
                {
                    // Rectangular dither with simple second-order noise shaping.
                    float inputSample = inputData[frame * channels + channel] * (INT16_MAX - 4);
                    float noise = (float)m_rand() / m_rand.max() + 0.5f * m_error1[channel] - m_error2[channel];
                    float outputSample = round(inputSample + noise);
                    m_error2[channel] = m_error1[channel];
                    m_error1[channel] = outputSample - inputSample;
                    assert(outputSample >= INT16_MIN && outputSample <= INT16_MAX);
                    outputData[frame * channels + channel] = (int16_t)outputSample;
                }
            }

            chunk = std::move(output);
        }
    }

    void DspDither::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }
}
