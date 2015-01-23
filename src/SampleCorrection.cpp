#include "pch.h"
#include "SampleCorrection.h"

namespace SaneAudioRenderer
{
    void SampleCorrection::NewFormat(SharedWaveFormat format)
    {
        assert(format);
        assert(format->nSamplesPerSec > 0);

        if (m_format)
        {
            m_segmentTimeInPreviousFormats += FramesToTime(m_segmentFramesInCurrentFormat);
            m_segmentFramesInCurrentFormat = 0;
        }

        m_format = format;
        m_bitstream = (DspFormatFromWaveFormat(*m_format) == DspFormat::Unknown);
    }

    void SampleCorrection::NewSegment(double rate)
    {
        assert(rate > 0.0);

        m_rate = rate;

        m_freshSegment = true;
        m_segmentStartTimestamp = 0;
        m_segmentTimeInPreviousFormats = 0;
        m_segmentFramesInCurrentFormat = 0;

        m_lastSampleEnd = 0;

        m_timingsError = 0;
    }

    void SampleCorrection::NewBuffer()
    {
        m_freshBuffer = true;
    }

    DspChunk SampleCorrection::ProcessSample(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps)
    {
        assert(m_format);

        FillMissingTimings(sampleProps);

        DspChunk chunk;

        const bool drop = (m_bitstream && m_freshBuffer && !(sampleProps.dwSampleFlags & AM_SAMPLE_SPLICEPOINT)) ||
                          (!m_bitstream && m_freshSegment && sampleProps.tStop <= 0);

        if (drop)
        {
            // Drop the sample.
            assert(chunk.IsEmpty());
        }
        else if (!m_bitstream && m_freshSegment && sampleProps.tStart < 0)
        {
            // Crop the sample.
            size_t cropFrames = (size_t)TimeToFrames(m_lastSampleEnd - sampleProps.tStart, m_rate);

            if (cropFrames > 0)
            {
                size_t cropBytes = cropFrames * m_format->nChannels * m_format->wBitsPerSample / 8;

                assert((int32_t)cropBytes < sampleProps.lActual);
                sampleProps.pbBuffer += cropBytes;
                sampleProps.lActual -= (int32_t)cropBytes;
                sampleProps.tStart += FramesToTime(cropFrames, m_rate);

                chunk = DspChunk(pSample, sampleProps, *m_format);
                AccumulateTimings(sampleProps, chunk.GetFrameCount());
            }
            else
            {
                chunk = DspChunk(pSample, sampleProps, *m_format);
                AccumulateTimings(sampleProps, chunk.GetFrameCount());
            }
        }
        else if (!m_bitstream && m_freshSegment && sampleProps.tStart > 0)
        {
            // Zero-pad the sample.
            size_t padFrames = (size_t)TimeToFrames(sampleProps.tStart - m_lastSampleEnd, m_rate);

            if (padFrames > 0)
            {
                DspChunk tempChunk(pSample, sampleProps, *m_format);

                size_t padBytes = padFrames * tempChunk.GetFrameSize();
                sampleProps.pbBuffer = nullptr;
                sampleProps.lActual += padBytes;
                sampleProps.tStart -= FramesToTime(padFrames);

                AccumulateTimings(sampleProps, tempChunk.GetFrameCount() + padFrames);

                chunk = DspChunk(tempChunk.GetFormat(), tempChunk.GetChannelCount(),
                                 tempChunk.GetFrameCount() + padFrames, tempChunk.GetRate());

                assert(chunk.GetSize() == tempChunk.GetSize() + padBytes);
                ZeroMemory(chunk.GetData(), padBytes);
                memcpy(chunk.GetData() + padBytes, tempChunk.GetConstData(), tempChunk.GetSize());
            }
            else
            {
                chunk = DspChunk(pSample, sampleProps, *m_format);
                AccumulateTimings(sampleProps, chunk.GetFrameCount());
            }
        }
        else
        {
            // Leave the sample untouched.
            chunk = DspChunk(pSample, sampleProps, *m_format);
            AccumulateTimings(sampleProps, chunk.GetFrameCount());
        }

        return chunk;
    }

    void SampleCorrection::FillMissingTimings(AM_SAMPLE2_PROPERTIES& sampleProps)
    {
        assert(m_format);
        assert(m_rate > 0.0);

        if (!(sampleProps.dwSampleFlags & AM_SAMPLE_TIMEVALID))
        {
            REFERENCE_TIME time = m_segmentTimeInPreviousFormats + FramesToTime(m_segmentFramesInCurrentFormat);

            sampleProps.tStart = m_segmentStartTimestamp + (REFERENCE_TIME)(time / m_rate);
            sampleProps.dwSampleFlags |= AM_SAMPLE_TIMEVALID;
        }

        if (!(sampleProps.dwSampleFlags & AM_SAMPLE_STOPVALID))
        {
            REFERENCE_TIME time = sampleProps.lActual * 8 / m_format->wBitsPerSample /
                                  m_format->nChannels * OneSecond / m_format->nSamplesPerSec;

            sampleProps.tStop = sampleProps.tStart + (REFERENCE_TIME)(time / m_rate);
            sampleProps.dwSampleFlags |= AM_SAMPLE_STOPVALID;
        }
    }

    uint64_t SampleCorrection::TimeToFrames(REFERENCE_TIME time)
    {
        assert(m_format);
        return (size_t)llMulDiv(time, m_format->nSamplesPerSec, OneSecond, 0);
    }

    uint64_t SampleCorrection::TimeToFrames(REFERENCE_TIME time, double rate)
    {
        assert(m_rate > 0.0);
        return (size_t)(TimeToFrames(time) * m_rate);
    }

    REFERENCE_TIME SampleCorrection::FramesToTime(uint64_t frames)
    {
        assert(m_format);
        return llMulDiv(frames, OneSecond, m_format->nSamplesPerSec, 0);
    }

    REFERENCE_TIME SampleCorrection::FramesToTime(uint64_t frames, double rate)
    {
        assert(m_rate > 0.0);
        return (REFERENCE_TIME)(FramesToTime(frames) / m_rate);
    }

    void SampleCorrection::AccumulateTimings(AM_SAMPLE2_PROPERTIES& sampleProps, size_t frames)
    {
        assert(m_format);
        assert(m_format->nSamplesPerSec > 0);
        assert(m_rate > 0.0);

        if (frames == 0)
            return;

        if (m_freshSegment)
        {
            assert(m_segmentStartTimestamp == 0);
            m_segmentStartTimestamp = sampleProps.tStart;
            m_freshSegment = false;
        }

        if (!m_freshSegment)
            m_lastSampleEnd = sampleProps.tStop;

        REFERENCE_TIME time = m_segmentTimeInPreviousFormats + FramesToTime(m_segmentFramesInCurrentFormat);

        m_timingsError = sampleProps.tStart - (REFERENCE_TIME)(time / m_rate);

        m_segmentFramesInCurrentFormat += frames;

        if (m_freshBuffer)
            m_freshBuffer = false;
    }
}
