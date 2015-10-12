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

        void Initialize(ISettings* pSettings, DspFormat outputFormat);

        std::wstring Name() override { return L"Dither"; }

        bool Active() override;

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

    protected:

        void SettingsUpdated() override;

    private:

        template <DspFormat DitherFormat, DspFormat OutputFormat, bool complex>
        void Dither(DspChunk& chunk);

        DspFormat m_outputFormat = DspFormat::Unknown;
        bool m_active = false;

        bool m_extraPrecision = false;

        std::array<float, 18> m_previous;
        std::array<std::minstd_rand, 18> m_generatorSimple;
        std::array<std::mt19937, 18> m_generatorComplex;
        std::array<std::uniform_real_distribution<float>, 18> m_distributor;
    };
}
