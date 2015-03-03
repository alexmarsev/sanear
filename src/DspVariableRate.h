#pragma once

#include "DspBase.h"

#include <zita-resampler/vresampler.h>

namespace SaneAudioRenderer
{
    class DspVariableRate final
        : public DspBase
    {
    public:

        DspVariableRate() = default;
        DspVariableRate(const DspVariableRate&) = delete;
        DspVariableRate& operator=(const DspVariableRate&) = delete;
        ~DspVariableRate() = default;

        void Initialize(bool variable, uint32_t inputRate, uint32_t outputRate, uint32_t channels);

        std::wstring Name() override { return L"VariableRate"; }

        bool Active() override;

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

    private:

        bool m_active = false;
        VResampler m_resampler;
        uint32_t m_inputRate = 0;
        uint32_t m_outputRate = 0;
        uint32_t m_channels = 0;
    };
}
