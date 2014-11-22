#include "pch.h"
#include "DspBalance.h"

#include "AudioRenderer.h"

namespace SaneAudioRenderer
{
    void DspBalance::Process(DspChunk& chunk)
    {
        float balance = m_renderer.GetBalance();

        if (!chunk.IsEmpty() && balance != 0.0f && chunk.GetChannelCount() == 2)
        {
            assert(balance >= -1.0f && balance <= 1.0f);
            DspChunk::ToFloat(chunk);

            auto data = reinterpret_cast<float*>(chunk.GetData());
            float gain = abs(balance);
            for (size_t i = (balance < 0 ? 1 : 0), n = chunk.GetSampleCount(); i < n; i += 2)
                data[i] *= gain;
        }
    }

    void DspBalance::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }
}
