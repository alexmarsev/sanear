#pragma once

#include "DspChunk.h"

namespace SaneAudioRenderer
{
    class SampleCorrection final
    {
    public:

        SampleCorrection() = default;

        void NewFormat(SharedWaveFormat format);
        void NewSegment(double rate);
        void NewBuffer();

        DspChunk ProcessSample(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps);

        REFERENCE_TIME GetLastSampleEnd() { return m_lastSampleEnd; }
        REFERENCE_TIME GetTimingsError() { return m_timingsError; }

    private:

        void FillMissingTimings(AM_SAMPLE2_PROPERTIES& sampleProps);
        void AccumulateTimings(AM_SAMPLE2_PROPERTIES& sampleProps, size_t frames);

        uint64_t TimeToFrames(REFERENCE_TIME time);
        uint64_t TimeToFrames(REFERENCE_TIME time, double rate);
        REFERENCE_TIME FramesToTime(uint64_t frames);
        REFERENCE_TIME FramesToTime(uint64_t frames, double rate);

        SharedWaveFormat m_format;
        bool m_bitstream = false;

        double m_rate = 1.0;

        bool m_freshSegment = true;
        REFERENCE_TIME m_segmentStartTimestamp = 0;
        REFERENCE_TIME m_segmentTimeInPreviousFormats = 0;
        uint64_t m_segmentFramesInCurrentFormat = 0;

        REFERENCE_TIME m_lastSampleEnd = 0;

        REFERENCE_TIME m_timingsError = 0;

        bool m_freshBuffer = true;
    };
}
