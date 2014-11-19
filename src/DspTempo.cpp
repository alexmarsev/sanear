#include "pch.h"
#include "DspTempo.h"

namespace SaneAudioRenderer
{
    void DspTempo::Initialize(float tempo, uint32_t rate, uint32_t channels)
    {
        m_stouch.clear();

        m_active = false;

        m_rate = rate;
        m_channels = channels;

        if (tempo != 1.0f)
        {
            m_stouch.setSampleRate(rate);
            m_stouch.setChannels(channels);

            m_stouch.setTempo(tempo);

            m_stouch.setSetting(SETTING_SEQUENCE_MS, 40);
            m_stouch.setSetting(SETTING_SEEKWINDOW_MS, 15);
            m_stouch.setSetting(SETTING_OVERLAP_MS, 8);

            m_active = true;
        }
    }

    void DspTempo::Process(DspChunk& chunk)
    {
        if (m_active && !chunk.IsEmpty())
        {
            DspChunk::ToFloat(chunk);

            assert(chunk.GetRate() == m_rate);
            assert(chunk.GetChannelCount() == m_channels);

            m_stouch.putSamples((const float*)chunk.GetConstData(), (uint32_t)chunk.GetFrameCount());

            DspChunk output(DspFormat::Float, m_channels, m_stouch.numSamples(), m_rate);

            uint32_t done = m_stouch.receiveSamples((float*)output.GetData(), (uint32_t)output.GetFrameCount());
            assert(done == output.GetFrameCount());
            output.Shrink(done);

            chunk = std::move(output);
        }
    }

    void DspTempo::Finish(DspChunk& chunk)
    {
        if (m_active)
        {
            Process(chunk);

            m_stouch.flush();
            uint32_t undone = m_stouch.numSamples();

            if (undone > 0)
            {
                DspChunk output(DspFormat::Float, m_channels, chunk.GetFrameCount() + undone, m_rate);

                if (!chunk.IsEmpty())
                    memcpy(output.GetData(), chunk.GetConstData(), chunk.GetSize());

                m_stouch.flush();

                uint32_t done = m_stouch.receiveSamples((float*)output.GetData() + chunk.GetSampleCount(), undone);
                assert(done == undone);
                output.Shrink(chunk.GetFrameCount() + done);

                chunk = std::move(output);
            }
        }
    }
}
