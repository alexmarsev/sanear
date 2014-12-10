#pragma once

#include "DspChunk.h"

#include <soxr.h>

namespace SaneAudioRenderer
{
    class DspRate final
    {
    public:

        DspRate() = default;
        DspRate(const DspRate&) = delete;
        DspRate& operator=(const DspRate&) = delete;
        ~DspRate();

        std::wstring Name() { return L"Rate"; }

        void Initialize(bool variable, uint32_t inputRate, uint32_t outputRate, uint32_t channels);
        bool Active();

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

        void Adjust(REFERENCE_TIME delta);

    private:

        void DestroyBackend();

        soxr_t m_soxr = nullptr;
        bool m_variable = false;
        uint32_t m_inputRate = 0;
        uint32_t m_outputRate = 0;
        uint32_t m_channels = 0;
        REFERENCE_TIME m_delta = 0;

        const double m_maxVariableRateMultiplier = 1.003;
    };
}
