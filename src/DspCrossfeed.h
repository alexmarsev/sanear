#pragma once

#include "DspChunk.h"

#include <bs2bclass.h>

namespace SaneAudioRenderer
{
    class DspCrossfeed final
    {
    public:

        DspCrossfeed() = default;
        DspCrossfeed(const DspCrossfeed&) = delete;
        DspCrossfeed& operator=(const DspCrossfeed&) = delete;

        void Initialize(bool enable, uint32_t rate, uint32_t channels, DWORD mask);

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

    private:

        bs2b_base m_bs2b;

        bool m_active = false;
        uint32_t m_rate;
    };
}
