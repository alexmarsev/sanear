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

        void Initialize(uint32_t rate);

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

    private:

        void AnalyzeLastChunk(float limit);
        void ModifyFirstChunk();

        int32_t m_attackFrames;
        int32_t m_releaseFrames;
        size_t m_windowFrames;

        std::deque<DspChunk> m_buffer;
        size_t m_bufferFrameCount;
        uint64_t m_bufferFirstFrame;

        std::deque<std::pair<int64_t, float>> m_peaks;
    };
}
