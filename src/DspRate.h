#pragma once

#include "DspBase.h"

#include <soxr.h>

namespace SaneAudioRenderer
{
    class DspRate final
        : public DspBase
    {
    public:

        DspRate() = default;
        DspRate(const DspRate&) = delete;
        DspRate& operator=(const DspRate&) = delete;
        ~DspRate();

        void Initialize(bool variable, uint32_t inputRate, uint32_t outputRate, uint32_t channels);

        std::wstring Name() override { return L"Rate"; }

        bool Active() override;

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

    private:

        enum class State
        {
            Passthrough,
            Constant,
            Variable,
        };

        DspChunk ProcessChunk(soxr_t soxr, DspChunk& chunk);
        DspChunk ProcessEosChunk(soxr_t soxr, DspChunk& chunk);

        void FinishStateTransition(DspChunk& processedChunk, DspChunk& unprocessedChunk, bool eos);

        soxr_t GetBackend();
        void DestroyBackends();

        soxr_t m_soxrc = nullptr;
        soxr_t m_soxrv = nullptr;

        State m_state = State::Passthrough;

        bool m_inStateTransition = false;
        std::pair<bool, size_t> m_transitionCorrelation;
        std::pair<DspChunk, DspChunk> m_transitionChunks;

        uint32_t m_inputRate = 0;
        uint32_t m_outputRate = 0;
        uint32_t m_channels = 0;
    };
}
