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

        void Initialize(ISettings* pSettings,
                        uint32_t inputChannels, DWORD inputMask,
                        uint32_t outputChannels, DWORD outputMask);

        std::wstring Name() override { return L"Matrix"; }

        bool Active() override;

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

        static DWORD GetChannelMask(const WAVEFORMATEX& format);
        static bool IsStereoFormat(const WAVEFORMATEX& format);

    protected:

        void SettingsUpdated() override;

    private:

        template <DspFormat MixFormat, typename MixSampleType = DspFormatTraits<MixFormat>::SampleType>
        void MixChunk(DspChunk& chunk, const std::array<MixSampleType, 18 * 18>& matrix);

        std::array<float, 18 * 18> m_matrixSingle;
        std::array<double, 18 * 18> m_matrixDouble;
        bool m_active = false;
        uint32_t m_inputChannels = 0;
        uint32_t m_outputChannels = 0;

        bool m_extraPrecision = false;
    };
}
