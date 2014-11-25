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

        void Initialize(bool exclusive);
        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

    private:

        const AudioRenderer& m_renderer;
        bool m_exclusive;
    };
}
