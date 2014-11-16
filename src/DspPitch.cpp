#include "pch.h"
#include "DspPitch.h"

namespace SaneAudioRenderer
{
    void DspPitch::Initialize(float pitch, uint32_t rate, uint32_t channels)
    {
        m_stouch.clear();
        m_active = false;

        if (pitch != 1.0f)
        {
            m_stouch.setSampleRate(rate);
            m_stouch.setChannels(channels);

            m_stouch.setPitch(pitch);

            m_active = true;
        }
    }

    void DspPitch::Process(DspChunk& chunk)
    {
        if (m_active && !chunk.IsEmpty())
        {
            DspChunk::ToFloat(chunk);

            m_stouch.putSamples((const float*)chunk.GetConstData(), chunk.GetFrameCount());

            DspChunk output(chunk.GetFormat(), chunk.GetChannelCount(), m_stouch.numSamples(), chunk.GetRate());

            ZeroMemory(output.GetData(), output.GetSize());
            size_t done = m_stouch.receiveSamples((float*)output.GetData(), output.GetFrameCount());
            assert(done == output.GetFrameCount());
            output.Shrink(done);

            chunk = std::move(output);
        }
    }

    void DspPitch::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }
}
