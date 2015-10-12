#include "pch.h"
#include "DspBalance.h"

#include "AudioRenderer.h"

namespace SaneAudioRenderer
{
    namespace
    {
        template <typename T>
        void GainChannel(T* data, size_t sampleCount, float gain, size_t channel)
        {
            for (size_t i = channel; i < sampleCount; i += 2)
                data[i] *= gain;
        }
    }

    void DspBalance::Initialize(ISettings* pSettings)
    {
        assert(pSettings);
        SetSettings(pSettings);
    }

    bool DspBalance::Active()
    {
        return m_renderer.GetBalance() != 0.0f;
    }

    void DspBalance::Process(DspChunk& chunk)
    {
        const float balance = m_renderer.GetBalance();
        assert(balance >= -1.0f && balance <= 1.0f);

        if (balance == 0.0f || chunk.IsEmpty() || chunk.GetChannelCount() != 2)
            return;

        CheckSettings();

        const float gain = std::abs(balance);
        const size_t channel = (balance < 0.0f ? 1 : 0);

        if (chunk.GetFormat() == DspFormat::Double || m_extraPrecision)
        {
            DspChunk::ToDouble(chunk);
            GainChannel((double*)chunk.GetData(), chunk.GetSampleCount(), gain, channel);
        }
        else
        {
            DspChunk::ToFloat(chunk);
            GainChannel((float*)chunk.GetData(), chunk.GetSampleCount(), gain, channel);
        }
    }

    void DspBalance::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }

    void DspBalance::SettingsUpdated()
    {
        m_extraPrecision = !!m_settings->GetExtraPrecisionProcessing();
    }
}
