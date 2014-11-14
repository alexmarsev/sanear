#pragma once

#include "DspChunk.h"

namespace SaneAudioRenderer
{
    class DspMatrix final
    {
    public:

        DspMatrix() = default;
        DspMatrix(const DspMatrix&) = delete;
        DspMatrix& operator=(const DspMatrix&) = delete;

        void Initialize(uint32_t inputChannels, DWORD inputMask,
                        uint32_t outputChannels, DWORD outputMask);

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

        static DWORD GetDefaultChannelMask(uint32_t channels);

    private:

        std::unique_ptr<float[]> m_matrix;
        uint32_t m_inputChannels;
        uint32_t m_outputChannels;
    };
}
