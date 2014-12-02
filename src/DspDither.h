#pragma once

#include "DspChunk.h"

namespace SaneAudioRenderer
{
    class DspDither final
    {
    public:

        DspDither() = default;
        DspDither(const DspDither&) = delete;
        DspDither& operator=(const DspDither&) = delete;

        std::wstring Name() { return L"Dither"; }

        void Initialize(DspFormat outputFormat);
        bool Active();

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

    private:

        bool m_active = false;
        std::array<float, 18> m_error1;
        std::array<float, 18> m_error2;
        std::minstd_rand m_rand;
    };
}
