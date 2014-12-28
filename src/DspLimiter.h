#pragma once

#include "DspBase.h"
#include "Interfaces.h"

namespace SaneAudioRenderer
{
    class DspLimiter final
        : public DspBase
    {
    public:

        DspLimiter() = default;
        DspLimiter(const DspLimiter&) = delete;
        DspLimiter& operator=(const DspLimiter&) = delete;

        void Initialize(ISettings* pSettings, uint32_t rate, bool exclusive);

        std::wstring Name() override { return L"Limiter"; }

        bool Active() override;

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

    private:

        void AnalyzeLastChunk();
        void ModifyFirstChunk();

        void UpdateSettings();

        ISettingsPtr m_settings;
        UINT32 m_settingsSerial = 0;

        bool m_exclusive = false;
        bool m_enabledShared = false;

        const float m_limit = 1.0f;
        uint32_t m_attackFrames = 0;
        uint32_t m_releaseFrames = 0;
        size_t m_windowFrames = 0;

        std::deque<DspChunk> m_buffer;
        size_t m_bufferFrameCount = 0;
        uint64_t m_bufferFirstFrame = 0;

        std::array<std::deque<std::pair<uint32_t, float>>, 18> m_peaks;
    };
}
