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

        void Adjust(REFERENCE_TIME time);

    private:

        bool m_active = false;
        VResampler m_resampler;
        uint32_t m_inputRate = 0;
        uint32_t m_outputRate = 0;
        uint32_t m_channels = 0;
        REFERENCE_TIME m_adjustTime = 0;
        int64_t m_inputFrames = 0;
        int64_t m_outputFrames = 0;
    };
}
