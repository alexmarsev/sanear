#include "pch.h"
#include "DspVolume.h"

#include "AudioRenderer.h"

namespace SaneAudioRenderer
{
    void DspVolume::Process(DspChunk& chunk)
    {
        float volume = m_renderer.GetVolume();

        if (!chunk.IsEmpty() && volume != 1.0f)
        {
            assert(volume >= 0.0f && volume <= 1.0f);
            DspChunk::ToFloat(chunk);

            auto data = reinterpret_cast<float*>(chunk.GetData());
            for (size_t i = 0, n = chunk.GetSampleCount(); i < n; i++)
                data[i] *= volume;
        }
    }

    void DspVolume::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }
}
