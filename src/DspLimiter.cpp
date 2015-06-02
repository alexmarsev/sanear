#include "pch.h"
#include "DspLimiter.h"

namespace SaneAudioRenderer
{
    namespace
    {
        const float slope = 1.0f - 1.0f / 20.0f; // 20:1 ratio
    }

    void DspLimiter::Initialize(uint32_t rate, uint32_t channels, bool exclusive)
    {
        m_exclusive = exclusive;
        m_rate = rate;
        m_channels = channels;

        m_active = false;
        m_holdWindow = 0;
        m_peak = 0.0f;
        m_threshold = 0.0f;
    }

    bool DspLimiter::Active()
    {
        return m_active;
    }

    void DspLimiter::Process(DspChunk& chunk)
    {
        if (chunk.IsEmpty())
            return;

        if (!m_exclusive ||
            chunk.GetFormat() != DspFormat::Float)
        {
            m_active = false;
            return;
        }

        m_active = true;

        auto data = reinterpret_cast<float*>(chunk.GetData());

        // Analyze samples
        float peak = 0.0f;
        for (size_t i = 0, n = chunk.GetSampleCount(); i < n; i++)
        {
            const float absSample = std::fabs(data[i]);

            if (std::fabs(absSample) > 1.0f)
                peak = std::fmax(peak, absSample);
        }

        // Configure limiter
        if (peak > 1.0f)
        {
            if (m_holdWindow <= 0)
            {
                NewTreshold(std::fmax(peak, 1.4f));
            }
            else if (peak > m_peak)
            {
                NewTreshold(peak);
            }

            m_holdWindow = m_rate * m_channels * 30; // 30 seconds
        }

        // Apply limiter
        if (m_holdWindow > 0)
        {
            for (size_t i = 0, n = chunk.GetSampleCount(); i < n; i++)
            {
                float& sample = data[i];
                const float absSample = std::fabs(sample);

                if (absSample > m_threshold)
                    sample *= pow(m_threshold / absSample, slope);

                assert(std::fabs(sample) <= 1.0f);
            }

            m_holdWindow -= chunk.GetSampleCount();
        }
    }

    void DspLimiter::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }

    void DspLimiter::NewTreshold(float peak)
    {
        m_peak = peak;
        m_threshold = pow(1.0f / peak, 1.0f / slope - 1.0f) - 0.0001f;
        DebugOut("DspLimiter active with", m_peak, "peak and", m_threshold, "threshold");
    }
}
