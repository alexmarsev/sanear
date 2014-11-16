#include "pch.h"
#include "DspPitch.h"

namespace SaneAudioRenderer
{
    void DspPitch::Initialize(float pitch, uint32_t rate, uint32_t channels)
    {
        m_stouch.clear();

        m_active = false;

        m_rate = rate;
        m_channels = channels;

        m_offset = 0;

        if (pitch != 1.0f)
        {
            m_stouch.setSampleRate(rate);
            m_stouch.setChannels(channels);

            m_stouch.setPitch(pitch);

            m_stouch.setSetting(SETTING_SEQUENCE_MS, 40);
            m_stouch.setSetting(SETTING_SEEKWINDOW_MS, 15);
            m_stouch.setSetting(SETTING_OVERLAP_MS, 8);

            m_active = true;
        }
    }

    void DspPitch::Process(DspChunk& chunk)
    {
        if (m_active && !chunk.IsEmpty())
        {
            DspChunk::ToFloat(chunk);

            assert(chunk.GetRate() == m_rate);
            assert(chunk.GetChannelCount() == m_channels);

            m_stouch.putSamples((const float*)chunk.GetConstData(), chunk.GetFrameCount());
            m_offset += chunk.GetFrameCount();

            DspChunk output(DspFormat::Float, m_channels, m_stouch.numSamples(), m_rate);

            uint32_t done = m_stouch.receiveSamples((float*)output.GetData(), output.GetFrameCount());
            assert(done == output.GetFrameCount());
            output.Shrink(done);
            m_offset -= done;

            chunk = std::move(output);
        }
    }

    void DspPitch::Finish(DspChunk& chunk)
    {
        if (m_active)
        {
            Process(chunk);

            assert(m_offset >= 0);
            if (m_offset > 0)
            {
                DspChunk output(DspFormat::Float, m_channels, chunk.GetFrameCount() + m_offset, m_rate);

                if (!chunk.IsEmpty())
                    memcpy(output.GetData(), chunk.GetConstData(), chunk.GetSize());

                m_stouch.flush();

                uint32_t done = m_stouch.receiveSamples((float*)output.GetData() + chunk.GetSampleCount(), m_offset);
                assert(done == m_offset);
                m_offset -= done;

                chunk = std::move(output);
            }
        }
    }
}
