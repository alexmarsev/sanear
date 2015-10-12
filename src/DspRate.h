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

        void Initialize(ISettings* pSettings, bool variable,
                        uint32_t inputRate, uint32_t outputRate, uint32_t channels);

        std::wstring Name() override { return L"Rate"; }

        bool Active() override;

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

        void Adjust(REFERENCE_TIME time);

    protected:

        void SettingsUpdated() override;

    private:

        enum class State
        {
            Passthrough,
            ConstantSingle,
            ConstantDouble,
            Variable,
        };

        DspChunk ProcessChunk(soxr_t soxr, DspChunk& chunk, bool doublePrecision);
        DspChunk ProcessEosChunk(soxr_t soxr, DspChunk& chunk, bool doublePrecision);

        void FinishStateTransition(DspChunk& processedChunk, DspChunk& unprocessedChunk, bool eos);

        void CreateBackend();
        soxr_t GetBackend();
        void DestroyBackends();

        soxr_t m_soxrcs = nullptr;
        soxr_t m_soxrcd = nullptr;
        soxr_t m_soxrv = nullptr;

        bool m_extraPrecision = false;

        State m_state = State::Passthrough;

        bool m_inStateTransition = false;
        std::pair<bool, size_t> m_transitionCorrelation;
        std::pair<DspChunk, DspChunk> m_transitionChunks;

        uint32_t m_inputRate = 0;
        uint32_t m_outputRate = 0;
        uint32_t m_channels = 0;

        uint64_t m_variableInputFrames = 0;
        uint64_t m_variableOutputFrames = 0;
        uint64_t m_variableDelay = 0; // In input samples.

        REFERENCE_TIME m_adjustTime = 0; // Negative time - less samples, positive time - more samples.
    };
}
