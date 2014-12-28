#pragma once

#include "DspBase.h"

namespace SaneAudioRenderer
{
    class DspMatrix final
        : public DspBase
    {
    public:

        DspMatrix() = default;
        DspMatrix(const DspMatrix&) = delete;
        DspMatrix& operator=(const DspMatrix&) = delete;

        void Initialize(uint32_t inputChannels, DWORD inputMask,
                        uint32_t outputChannels, DWORD outputMask);

        std::wstring Name() override { return L"Matrix"; }

        bool Active() override;

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

        static DWORD GetChannelMask(const WAVEFORMATEX& format);

    private:

        std::unique_ptr<float[]> m_matrix;
        uint32_t m_inputChannels = 0;
        uint32_t m_outputChannels = 0;
    };
}
