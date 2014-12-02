#include "pch.h"
#include "DspCrossfeed.h"

namespace SaneAudioRenderer
{
    void DspCrossfeed::Initialize(bool enabled, uint32_t rate, uint32_t channels, DWORD mask)
    {
        m_active = false;

        if (enabled && channels == 2 && mask == KSAUDIO_SPEAKER_STEREO)
        {
            m_bs2b.set_level(BS2B_CMOY_CLEVEL);
            m_bs2b.set_srate(rate);

            m_active = true;
        }
    }

    bool DspCrossfeed::Active()
    {
        return m_active;
    }

    void DspCrossfeed::Process(DspChunk& chunk)
    {
        if (m_active && !chunk.IsEmpty())
        {
            DspChunk::ToFloat(chunk);

            assert(chunk.GetChannelCount() == 2);
            m_bs2b.cross_feed((float*)chunk.GetData(), (int)chunk.GetFrameCount());
        }
    }

    void DspCrossfeed::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }
}
