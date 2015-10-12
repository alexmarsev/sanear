#include "pch.h"
#include "DspVolume.h"

#include "AudioRenderer.h"

namespace SaneAudioRenderer
{
    namespace
    {
        template <typename T>
        void Gain(T* data, size_t sampleCount, float volume)
        {
            for (size_t i = 0; i < sampleCount; i++)
                data[i] *= volume;
        }
    }

    void DspVolume::Initialize(ISettings* pSettings)
    {
        assert(pSettings);
        SetSettings(pSettings);
    }

    bool DspVolume::Active()
    {
        return m_renderer.GetVolume() != 1.0f;
    }

    void DspVolume::Process(DspChunk& chunk)
    {
        const float volume = m_renderer.GetVolume();
        assert(volume >= 0.0f && volume <= 1.0f);

        if (volume == 1.0f || chunk.IsEmpty())
            return;

        CheckSettings();

        if (chunk.GetFormat() == DspFormat::Double || m_extraPrecision)
        {
            DspChunk::ToDouble(chunk);
            Gain((double*)chunk.GetData(), chunk.GetSampleCount(), volume);
        }
        else
        {
            DspChunk::ToFloat(chunk);
            Gain((float*)chunk.GetData(), chunk.GetSampleCount(), volume);
        }
    }

    void DspVolume::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }

    void DspVolume::SettingsUpdated()
    {
        m_extraPrecision = !!m_settings->GetExtraPrecisionProcessing();
    }
}
