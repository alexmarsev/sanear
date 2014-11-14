#include "pch.h"
#include "AudioRenderer.h"

namespace SaneAudioRenderer
{
    namespace
    {
        DspChunk PreProcess(IMediaSample* pSample, const AM_SAMPLE2_PROPERTIES& sampleProps,
                            const WAVEFORMATEX& sampleFormat, REFERENCE_TIME previousSampleEndTime)
        {
            DspChunk chunk;

            auto timeToBytes = [&](REFERENCE_TIME time)
            {
                size_t frames = (size_t)(time * sampleFormat.nSamplesPerSec / OneSecond);
                return frames * sampleFormat.nChannels * sampleFormat.wBitsPerSample / 8;
            };

            if (sampleProps.tStop <= previousSampleEndTime)
            {
                // Drop the sample.
                assert(chunk.IsEmpty());
            }
            else if (sampleProps.tStart < previousSampleEndTime)
            {
                // Crop the sample.
                size_t cropBytes = timeToBytes(previousSampleEndTime - sampleProps.tStart);

                AM_SAMPLE2_PROPERTIES croppedSampleProps = sampleProps;
                assert(cropBytes < croppedSampleProps.lActual);
                croppedSampleProps.pbBuffer += cropBytes;
                croppedSampleProps.lActual -= (int)cropBytes;

                chunk = DspChunk(pSample, croppedSampleProps, sampleFormat);
            }
            else if (sampleProps.tStart > previousSampleEndTime)
            {
                // Zero-pad the sample.
                size_t extendBytes = timeToBytes(sampleProps.tStart - previousSampleEndTime);

                DspChunk tempChunk(pSample, sampleProps, sampleFormat);

                chunk = DspChunk(tempChunk.GetFormat(), tempChunk.GetChannelCount(),
                                 tempChunk.GetFrameCount() + extendBytes / tempChunk.GetFrameSize(), tempChunk.GetRate());

                assert(chunk.GetSize() == tempChunk.GetSize() + extendBytes);
                ZeroMemory(chunk.GetData(), extendBytes);
                memcpy(chunk.GetData() + extendBytes, tempChunk.GetConstData(), tempChunk.GetSize());
            }
            else
            {
                // Leave the sample untouched.
                chunk = DspChunk(pSample, sampleProps, sampleFormat);
            }

            return chunk;
        }
    }

    AudioRenderer::AudioRenderer(IMyClock* pClock, CAMEvent& bufferFilled, HRESULT& result)
        : m_deviceManager(result)
        , m_graphClock(pClock)
        , m_flush(TRUE/*manual reset*/)
        , m_bufferFilled(bufferFilled)
    {
        if (FAILED(result))
            return;

        try
        {
            if (!m_graphClock)
                throw E_UNEXPECTED;

            if (static_cast<HANDLE>(m_flush) == NULL ||
                static_cast<HANDLE>(m_bufferFilled) == NULL)
            {
                throw E_OUTOFMEMORY;
            }

            if (!m_deviceManager.CreateDevice(m_device))
                throw E_OUTOFMEMORY;
        }
        catch (HRESULT ex)
        {
            result = ex;
        }
    }

    AudioRenderer::~AudioRenderer()
    {
        // Just in case.
        if (m_state != State_Stopped)
            Stop();

        m_device = {};
    }

    bool AudioRenderer::Enqueue(IMediaSample* pSample, const AM_SAMPLE2_PROPERTIES& sampleProps)
    {
        DspChunk chunk;

        {
            CAutoLock objectLock(this);
            assert(m_state != State_Stopped);

            try
            {
                chunk = PreProcess(pSample, sampleProps, m_inputFormat.Format, m_lastSampleEnd);

                if (chunk.IsEmpty())
                    return true;

                m_dspMatrix.Process(chunk);
                //m_dspGain.Process(chunk);
                m_dspRate.Process(chunk);
                //m_dspCrossfeed.Process(chunk);
                //m_dspLimiter.Process(chunk);

                DspChunk::ToFloat(chunk);

                m_lastSampleEnd = sampleProps.tStop;
            }
            catch (std::bad_alloc&)
            {
                chunk = DspChunk();
                assert(chunk.IsEmpty());
            }
        }

        return Push(chunk);
    }

    bool AudioRenderer::Finish(bool blockUntilEnd)
    {
        DspChunk chunk;

        {
            CAutoLock objectLock(this);
            assert(m_state != State_Stopped);

            try
            {
                m_dspMatrix.Finish(chunk);
                //m_dspGain.Finish(chunk);
                m_dspRate.Finish(chunk);
                //m_dspCrossfeed.Finish(chunk);
                //m_dspLimiter.Finish(chunk);

                DspChunk::ToFloat(chunk);
            }
            catch (std::bad_alloc&)
            {
                chunk = DspChunk();
                assert(chunk.IsEmpty());
            }
        }

        auto doBlock = [this]
        {
            TimePeriodHelper timePeriodHelper(1);

            try
            {
                for (;;)
                {
                    int64_t actual, target;

                    {
                        CAutoLock objectLock(this);

                        UINT64 deviceClockFrequency, deviceClockPosition;
                        ThrowIfFailed(m_device.audioClock->GetFrequency(&deviceClockFrequency));
                        ThrowIfFailed(m_device.audioClock->GetPosition(&deviceClockPosition, nullptr));

                        actual = llMulDiv(deviceClockPosition, OneSecond, deviceClockFrequency, 0);
                        target = llMulDiv(m_pushedFrames, OneSecond, m_device.format.Format.nSamplesPerSec, 0);

                        if (actual >= target)
                        {
                            assert(actual == target);
                            break;
                        }
                    }

                    if (m_flush.Wait(std::max(1, (int32_t)((target - actual) * 1000 / OneSecond))))
                        return false;
                }
            }
            catch (HRESULT)
            {
                assert(false);
            }

            return true;
        };

        return Push(chunk) && (!blockUntilEnd || doBlock());
    }

    void AudioRenderer::BeginFlush()
    {
        m_flush.Set();
    }

    void AudioRenderer::EndFlush()
    {
        CAutoLock objectLock(this);
        assert(m_state != State_Running);

        m_device.audioClient->Reset();
        m_bufferFilled.Reset();

        if (m_inputFormatInitialized)
            InitializeProcessors();

        m_flush.Reset();

        m_pushedFrames = 0;
    }

    void AudioRenderer::SetFormat(const WAVEFORMATEX& inputFormat)
    {
        CAutoLock objectLock(this);

        m_inputFormat = inputFormat.wFormatTag == WAVE_FORMAT_EXTENSIBLE ?
                            reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(inputFormat) :
                            WAVEFORMATEXTENSIBLE{inputFormat};

        m_inputFormatInitialized = true;

        InitializeProcessors();
    }

    void AudioRenderer::NewSegment()
    {
        CAutoLock objectLock(this);
        assert(m_state != State_Running);

        m_lastSampleEnd = 0;
    }

    void AudioRenderer::Play(REFERENCE_TIME startTime)
    {
        CAutoLock objectLock(this);
        assert(m_state != State_Running);
        m_state = State_Running;

        m_graphClock->SlaveClockToAudio(m_device.audioClock, startTime);
        m_device.audioClient->Start();
        assert(m_bufferFilled.Check());
    }

    void AudioRenderer::Pause()
    {
        CAutoLock objectLock(this);
        m_state = State_Paused;

        m_graphClock->UnslaveClockFromAudio();
        m_device.audioClient->Stop();
    }

    void AudioRenderer::Stop()
    {
        CAutoLock objectLock(this);
        m_state = State_Stopped;

        m_graphClock->UnslaveClockFromAudio();
        m_device.audioClient->Stop();
    }

    void AudioRenderer::InitializeProcessors()
    {
        CAutoLock objectLock(this);
        assert(m_inputFormatInitialized);

        auto getChannelMask = [](const WAVEFORMATEXTENSIBLE& format)
        {
            return format.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE ?
                       reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format).dwChannelMask :
                       DspMatrix::GetDefaultChannelMask(format.Format.nChannels);
        };

        m_dspMatrix.Initialize(m_inputFormat.Format.nChannels, getChannelMask(m_inputFormat),
                               m_device.format.Format.nChannels, getChannelMask(m_device.format));
        //m_dspGain.Initialize(m_inputFormat.Format.nSamplesPerSec, m_device.format.Format.nChannels);
        m_dspRate.Initialize(m_inputFormat.Format.nSamplesPerSec, m_device.format.Format.nSamplesPerSec,
                             m_device.format.Format.nChannels);
        //m_dspCrossfeed.Initialize(m_device.format.Format.nSamplesPerSec, m_device.format.Format.nChannels,
        //                          getChannelMask(m_device.format));
        //m_dspLimiter.Initialize(m_device.format.Format.nSamplesPerSec);
    }

    bool AudioRenderer::Push(DspChunk& chunk)
    {
        const uint32_t frameSize = chunk.GetFrameSize();
        const size_t chunkFrames = chunk.GetFrameCount();

        bool firstIteration = true;
        for (size_t doneFrames = 0; doneFrames < chunkFrames;)
        {
            // The device buffer is full or almost full at the beginning of the second and subsequent iterations.
            // Sleep until the buffer may have significant amount of free space. Unless interrupted.
            if (!firstIteration && m_flush.Wait(m_device.bufferDuration / 4))
                return false;

            firstIteration = false;

            CAutoLock objectLock(this);

            assert(m_state != State_Stopped);

            // Get up-to-date information on the device buffer.
            UINT32 bufferFrames, bufferPadding;
            ThrowIfFailed(m_device.audioClient->GetBufferSize(&bufferFrames));
            ThrowIfFailed(m_device.audioClient->GetCurrentPadding(&bufferPadding));

            // Find out how many frames we can write this time.
            const UINT32 doFrames = std::min(bufferFrames - bufferPadding, (UINT32)(chunkFrames - doneFrames));

            if (doFrames == 0)
                continue;

            // Write frames to the device buffer.
            BYTE* deviceBuffer;
            ThrowIfFailed(m_device.audioRenderClient->GetBuffer(doFrames, &deviceBuffer));
            assert(frameSize == (m_device.format.Format.wBitsPerSample / 8 * m_device.format.Format.nChannels));
            memcpy(deviceBuffer, chunk.GetConstData() + doneFrames * frameSize, doFrames * frameSize);
            ThrowIfFailed(m_device.audioRenderClient->ReleaseBuffer(doFrames, 0));

            // If the buffer is filled enough to start playback, set the corresponding event. Otherwise reset it.
            OneSecond * (bufferPadding + doFrames) / m_device.format.Format.nSamplesPerSec > m_device.streamLatency ?
                m_bufferFilled.Set() : m_bufferFilled.Reset();

            doneFrames += doFrames;
            m_pushedFrames += doFrames;
        }

        return true;
    }
}