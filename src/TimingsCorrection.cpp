#include "pch.h"
#include "TimingsCorrection.h"

namespace SaneAudioRenderer
{
    void TimingsCorrection::SetFormat(SharedWaveFormat format)
    {
        assert(format);

        if (m_format)
        {
            m_processedFramesTimeInPreviousFormats += llMulDiv(m_processedFrames, OneSecond, m_format->nSamplesPerSec, 0);
            m_processedFrames = 0;
        }

        m_format = format;
        m_bitstream = (DspFormatFromWaveFormat(*m_format) == DspFormat::Unknown);
    }

    void TimingsCorrection::NewSegment(double rate)
    {
        m_rate = rate;

        m_processedFramesTimeInPreviousFormats = 0;
        m_processedFrames = 0;
        m_firstSampleStart = 0;
        m_lastSampleEnd = 0;
    }

    DspChunk TimingsCorrection::ProcessSample(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps)
    {
        assert(m_format);

        FillMissingTimings(sampleProps);

        DspChunk chunk;

        if (m_bitstream || m_lastSampleEnd > 0)
        {
            // Leave the sample untouched.
            chunk = DspChunk(pSample, sampleProps, *m_format);
            AccumulateTimings(sampleProps, chunk.GetFrameCount());
        }
        else if (sampleProps.tStop <= m_lastSampleEnd)
        {
            // Drop the sample.
            assert(chunk.IsEmpty());
            AccumulateTimings(sampleProps, chunk.GetFrameCount());
        }
        else if (sampleProps.tStart < m_lastSampleEnd)
        {
            // Crop the sample.
            size_t cropFrames = TimeToFrames(m_lastSampleEnd - sampleProps.tStart);

            if (cropFrames > 0)
            {
                size_t cropBytes = cropFrames * m_format->nChannels * m_format->wBitsPerSample / 8;

                assert((int32_t)cropBytes < sampleProps.lActual);
                sampleProps.pbBuffer += cropBytes;
                sampleProps.lActual -= (int32_t)cropBytes;
                sampleProps.tStart += FramesToTime(cropFrames);

                chunk = DspChunk(pSample, sampleProps, *m_format);
                AccumulateTimings(sampleProps, chunk.GetFrameCount());
            }
            else
            {
                chunk = DspChunk(pSample, sampleProps, *m_format);
                AccumulateTimings(sampleProps, chunk.GetFrameCount());
            }
        }
        else if (sampleProps.tStart > m_lastSampleEnd)
        {
            // Zero-pad the sample.
            size_t extendFrames = TimeToFrames(sampleProps.tStart - m_lastSampleEnd);

            if (extendFrames > 0)
            {
                DspChunk tempChunk(pSample, sampleProps, *m_format);

                size_t extendBytes = extendFrames * tempChunk.GetFrameSize();
                sampleProps.pbBuffer = nullptr;
                sampleProps.lActual += extendBytes;
                sampleProps.tStart -= FramesToTime(extendFrames);

                AccumulateTimings(sampleProps, tempChunk.GetFrameCount() + extendFrames);

                chunk = DspChunk(tempChunk.GetFormat(), tempChunk.GetChannelCount(),
                                 tempChunk.GetFrameCount() + extendFrames, tempChunk.GetRate());

                assert(chunk.GetSize() == tempChunk.GetSize() + extendBytes);
                ZeroMemory(chunk.GetData(), extendBytes);
                memcpy(chunk.GetData() + extendBytes, tempChunk.GetConstData(), tempChunk.GetSize());
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

    void TimingsCorrection::FillMissingTimings(AM_SAMPLE2_PROPERTIES& sampleProps)
    {
        assert(m_format);

        if (!(sampleProps.dwSampleFlags & AM_SAMPLE_TIMEVALID))
        {
            sampleProps.tStart = m_firstSampleStart +
                                 (REFERENCE_TIME)((m_processedFramesTimeInPreviousFormats +
                                                  llMulDiv(m_processedFrames, OneSecond,
                                                           m_format->nSamplesPerSec, 0)) / m_rate);
            sampleProps.dwSampleFlags |= AM_SAMPLE_TIMEVALID;
        }

        if (!(sampleProps.dwSampleFlags & AM_SAMPLE_STOPVALID))
        {
            REFERENCE_TIME duration = sampleProps.lActual * 8 / m_format->wBitsPerSample /
                                      m_format->nChannels * OneSecond / m_format->nSamplesPerSec;
            sampleProps.tStop = sampleProps.tStart + (REFERENCE_TIME)(duration / m_rate);
            sampleProps.dwSampleFlags |= AM_SAMPLE_STOPVALID;
        }
    }

    size_t TimingsCorrection::TimeToFrames(REFERENCE_TIME time)
    {
        assert(m_format);
        return (size_t)(time * m_format->nSamplesPerSec / OneSecond * m_rate);
    }

    REFERENCE_TIME TimingsCorrection::FramesToTime(size_t frames)
    {
        assert(m_format);
        return (REFERENCE_TIME)(frames * OneSecond / m_format->nSamplesPerSec / m_rate);
    }

    void TimingsCorrection::AccumulateTimings(AM_SAMPLE2_PROPERTIES& sampleProps, size_t frames)
    {
        if (m_processedFrames == 0)
            m_firstSampleStart = sampleProps.tStart;

        m_processedFrames += frames;

        m_lastSampleEnd = sampleProps.tStop;

        m_timingsError = sampleProps.tStart -
                         (REFERENCE_TIME)(m_processedFramesTimeInPreviousFormats +
                                          llMulDiv(m_processedFrames - frames, OneSecond,
                                                   m_format->nSamplesPerSec, 0) / m_rate);
    }
}
