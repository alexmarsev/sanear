#pragma once

#include "DspChunk.h"

namespace SaneAudioRenderer
{
    class TimingsCorrection final
    {
    public:

        TimingsCorrection() = default;

        void SetFormat(SharedWaveFormat format);
        void NewSegment(double rate);

        DspChunk ProcessSample(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps);

        REFERENCE_TIME GetLastSampleEnd() { return m_lastSampleEnd; }
        REFERENCE_TIME GetTimingsError() { return m_timingsError; };

    private:

        void FillMissingTimings(AM_SAMPLE2_PROPERTIES& sampleProps);
        void AccumulateTimings(AM_SAMPLE2_PROPERTIES& sampleProps, size_t frames);

        size_t TimeToFrames(REFERENCE_TIME time);
        REFERENCE_TIME FramesToTime(size_t frames);

        SharedWaveFormat m_format;
        bool m_bitstream = false;
        double m_rate = 1.0;

        REFERENCE_TIME m_processedFramesTimeInPreviousFormats = 0;
        uint64_t m_processedFrames = 0;
        REFERENCE_TIME m_firstSampleStart = 0;
        REFERENCE_TIME m_lastSampleEnd = 0;
        REFERENCE_TIME m_timingsError = 0;
    };
}
