#pragma once

#include "DspChunk.h"

#include <SoundTouch.h>

namespace SaneAudioRenderer
{
    class DspTempo final
    {
    public:

        DspTempo() = default;
        DspTempo(const DspTempo&) = delete;
        DspTempo& operator=(const DspTempo&) = delete;

        std::wstring Name() { return L"Tempo"; }

        void Initialize(float tempo, uint32_t rate, uint32_t channels);
        bool Active();

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

    private:

        soundtouch::SoundTouch m_stouch;

        bool m_active = false;

        uint32_t m_rate;
        uint32_t m_channels;
    };
}
