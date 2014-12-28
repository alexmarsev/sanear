#pragma once

#include "DspBase.h"

#include <SoundTouch.h>

namespace SaneAudioRenderer
{
    class DspTempo final
        : public DspBase
    {
    public:

        DspTempo() = default;
        DspTempo(const DspTempo&) = delete;
        DspTempo& operator=(const DspTempo&) = delete;

        void Initialize(float tempo, uint32_t rate, uint32_t channels);

        std::wstring Name() override { return L"Tempo"; }

        bool Active() override;

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

    private:

        soundtouch::SoundTouch m_stouch;

        bool m_active = false;

        uint32_t m_rate = 0;
        uint32_t m_channels = 0;
    };
}
