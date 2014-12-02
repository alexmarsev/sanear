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

        void Initialize(uint32_t inputRate, uint32_t outputRate, uint32_t channels);
        bool Active();

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

    private:

        void DestroyBackend();

        soxr_t m_soxr = nullptr;
        uint32_t m_inputRate;
        uint32_t m_outputRate;
        uint32_t m_channels;
    };
}
