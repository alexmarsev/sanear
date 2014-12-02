#pragma once

#include "DspChunk.h"

namespace SaneAudioRenderer
{
    class AudioRenderer;

    class DspBalance final
    {
    public:

        DspBalance(AudioRenderer& renderer) : m_renderer(renderer) {}
        DspBalance(const DspBalance&) = delete;
        DspBalance& operator=(const DspBalance&) = delete;

        std::wstring Name() { return L"Balance"; }

        bool Active();

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

    private:

        const AudioRenderer& m_renderer;
    };
}
