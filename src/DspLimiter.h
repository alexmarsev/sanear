#pragma once

#include "DspChunk.h"

namespace SaneAudioRenderer
{
    class DspLimiter final
    {
    public:

        DspLimiter() = default;
        DspLimiter(const DspLimiter&) = delete;
        DspLimiter& operator=(const DspLimiter&) = delete;

        std::wstring Name() { return L"Limiter"; }

        void Initialize(uint32_t rate, bool exclusive);
        bool Active();

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

    private:

        void AnalyzeLastChunk();
        void ModifyFirstChunk();

        float m_limit;
        int32_t m_attackFrames;
        int32_t m_releaseFrames;
        size_t m_windowFrames;

        std::deque<DspChunk> m_buffer;
        size_t m_bufferFrameCount;
        uint64_t m_bufferFirstFrame;

        std::deque<std::pair<uint64_t, float>> m_peaks;
    };
}
