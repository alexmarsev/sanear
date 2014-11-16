#pragma once

#include "DspChunk.h"

#include <SoundTouch.h>

namespace SaneAudioRenderer
{
    class DspPitch final
    {
    public:

        DspPitch() = default;
        DspPitch(const DspPitch&) = delete;
        DspPitch& operator=(const DspPitch&) = delete;

        void Initialize(float pitch, uint32_t rate, uint32_t channels);

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

    private:

        soundtouch::SoundTouch m_stouch;

        bool m_active = false;

        uint32_t m_rate;
        uint32_t m_channels;

        int32_t m_offset;
    };
}
