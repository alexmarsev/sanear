#pragma once

#include "DspBase.h"

#include <zita-resampler/vresampler.h>

namespace SaneAudioRenderer
{
    class DspRealtimeRate final
        : public DspBase
    {
    public:

        DspRealtimeRate() = default;
        DspRealtimeRate(const DspRealtimeRate&) = delete;
        DspRealtimeRate& operator=(const DspRealtimeRate&) = delete;
        ~DspRealtimeRate() = default;

        void Initialize(bool realtime, uint32_t inputRate, uint32_t outputRate, uint32_t channels);

        std::wstring Name() override { return L"RealtimeRate"; }

        bool Active() override;

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

    private:

        bool m_active = false;
        VResampler m_resampler;
        uint32_t m_inputRate = 0;
        uint32_t m_outputRate = 0;
        uint32_t m_channels = 0;
    };
}
