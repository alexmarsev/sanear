#pragma once

#include "DspBase.h"

namespace SaneAudioRenderer
{
    class DspDither final
        : public DspBase
    {
    public:

        DspDither() = default;
        DspDither(const DspDither&) = delete;
        DspDither& operator=(const DspDither&) = delete;

        void Initialize(DspFormat outputFormat);

        std::wstring Name() override { return L"Dither"; }

        bool Active() override;

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

    private:

        bool m_active = false;
        std::array<float, 18> m_error1;
        std::array<float, 18> m_error2;
        std::minstd_rand m_rand;
    };
}
