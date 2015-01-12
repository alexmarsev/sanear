#include "pch.h"
#include "DspVolume.h"

#include "AudioRenderer.h"

namespace SaneAudioRenderer
{
    void DspVolume::Initialize(bool exclusive)
    {
        m_exclusive = exclusive;
    }

    bool DspVolume::Active()
    {
        return !m_exclusive || m_renderer.GetVolume() != 1.0f;
    }

    void DspVolume::Process(DspChunk& chunk)
    {
        const float volume = std::min(m_renderer.GetVolume(), m_exclusive ? 1.0f : 0.98f);
        assert(volume >= 0.0f && volume <= 1.0f);

        if (volume == 1.0f || chunk.IsEmpty())
            return;

        DspChunk::ToFloat(chunk);

        auto data = reinterpret_cast<float*>(chunk.GetData());
        for (size_t i = 0, n = chunk.GetSampleCount(); i < n; i++)
            data[i] *= volume;
    }

    void DspVolume::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }
}
