#pragma once

#include "DspChunk.h"

namespace SaneAudioRenderer
{
    class AudioRenderer;

    class DspVolume final
    {
    public:

        DspVolume(AudioRenderer& renderer) : m_renderer(renderer) {}
        DspVolume(const DspVolume&) = delete;
        DspVolume& operator=(const DspVolume&) = delete;

        std::wstring Name() { return L"Volume"; }

        void Initialize(bool exclusive);
        bool Active();

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

    private:

        const AudioRenderer& m_renderer;
        bool m_exclusive;
    };
}
