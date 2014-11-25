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

            // TODO: can gradually accumulate error - decide what to do with it

            auto timeToFrames = [&](REFERENCE_TIME time)
            {
                return (size_t)(time * sampleFormat.nSamplesPerSec / OneSecond);
            };

            if (sampleProps.tStop <= previousSampleEndTime)
            {
                // Drop the sample.
                assert(chunk.IsEmpty());
            }
            else if (sampleProps.tStart < previousSampleEndTime)
            {
                // Crop the sample.
                size_t cropFrames = timeToFrames(previousSampleEndTime - sampleProps.tStart);
                if (cropFrames > 0)
                {
                    size_t cropBytes = cropFrames * sampleFormat.nChannels * sampleFormat.wBitsPerSample / 8;

                    AM_SAMPLE2_PROPERTIES croppedSampleProps = sampleProps;
                    assert((int32_t)cropBytes < croppedSampleProps.lActual);
                    croppedSampleProps.pbBuffer += cropBytes;
                    croppedSampleProps.lActual -= (int32_t)cropBytes;

                    chunk = DspChunk(pSample, croppedSampleProps, sampleFormat);
                }
                else
                {
                    chunk = DspChunk(pSample, sampleProps, sampleFormat);
                }
            }
            else if (sampleProps.tStart > previousSampleEndTime)
            {
                // Zero-pad the sample.
                size_t extendFrames = timeToFrames(sampleProps.tStart - previousSampleEndTime);
                if (extendFrames > 0)
                {
                    DspChunk tempChunk(pSample, sampleProps, sampleFormat);

                    chunk = DspChunk(tempChunk.GetFormat(), tempChunk.GetChannelCount(),
                                     tempChunk.GetFrameCount() + extendFrames, tempChunk.GetRate());

                    size_t extendBytes = extendFrames * chunk.GetFrameSize();

                    assert(chunk.GetSize() == tempChunk.GetSize() + extendBytes);
                    ZeroMemory(chunk.GetData(), extendBytes);
                    memcpy(chunk.GetData() + extendBytes, tempChunk.GetConstData(), tempChunk.GetSize());
                }
                else
                {
                    chunk = DspChunk(pSample, sampleProps, sampleFormat);
                }
            }
            else
            {
                // Leave the sample untouched.
                chunk = DspChunk(pSample, sampleProps, sampleFormat);
            }

            return chunk;
        }
    }

    AudioRenderer::AudioRenderer(ISettings* pSettings, IMyClock* pClock, CAMEvent& bufferFilled, HRESULT& result)
        : m_deviceManager(result)
        , m_graphClock(pClock)
        , m_flush(TRUE/*manual reset*/)
        , m_dspVolume(*this)
        , m_dspBalance(*this)
        , m_bufferFilled(bufferFilled)
        , m_settings(pSettings)
    {
        if (FAILED(result))
            return;

        try
        {
            if (!m_settings || !m_graphClock)
                throw E_UNEXPECTED;

            if (static_cast<HANDLE>(m_flush) == NULL ||
                static_cast<HANDLE>(m_bufferFilled) == NULL)
            {
                throw E_OUTOFMEMORY;
            }
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
                m_dspRate.Process(chunk);
                m_dspTempo.Process(chunk);
                m_dspCrossfeed.Process(chunk);
                m_dspVolume.Process(chunk);
                m_dspBalance.Process(chunk);
                m_dspLimiter.Process(chunk);
                m_dspDither.Process(chunk);

                DspChunk::ToFormat(m_device.dspFormat, chunk);

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
                m_dspRate.Finish(chunk);
                m_dspTempo.Finish(chunk);
                m_dspCrossfeed.Finish(chunk);
                m_dspVolume.Finish(chunk);
                m_dspBalance.Finish(chunk);
                m_dspLimiter.Finish(chunk);
                m_dspDither.Finish(chunk);

                DspChunk::ToFormat(m_device.dspFormat, chunk);
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

            m_graphClock->UnslaveClockFromAudio();

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

        //if (m_inputFormatInitialized)
        //    InitializeProcessors();

        m_flush.Reset();

        m_pushedFrames = 0;
    }

    void AudioRenderer::SetFormat(const WAVEFORMATEX& inputFormat)
    {
        CAutoLock objectLock(this);

        m_inputFormat = inputFormat.wFormatTag == WAVE_FORMAT_EXTENSIBLE ?
                            reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(inputFormat) :
                            WAVEFORMATEXTENSIBLE{inputFormat};

        m_deviceManager.CreateDevice(m_device, m_inputFormat, !!m_settings->UseExclusiveMode());

        m_inputFormatInitialized = true;

        InitializeProcessors();
    }

    void AudioRenderer::NewSegment(double rate)
    {
        CAutoLock objectLock(this);
        //assert(m_state != State_Running);

        m_lastSampleEnd = 0;
        m_rate = rate;

        if (m_inputFormatInitialized)
            InitializeProcessors();
    }

    void AudioRenderer::Play(REFERENCE_TIME startTime)
    {
        CAutoLock objectLock(this);
        assert(m_state != State_Running);
        m_state = State_Running;

        m_graphClock->SlaveClockToAudio(m_device.audioClock, startTime);
        m_device.audioClient->Start();
        //assert(m_bufferFilled.Check());
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
        // TODO: release audio device here

        CAutoLock objectLock(this);
        m_state = State_Stopped;

        m_graphClock->UnslaveClockFromAudio();
        m_device.audioClient->Stop();
    }

    std::unique_ptr<WAVEFORMATEXTENSIBLE> AudioRenderer::GetInputFormat()
    {
        CAutoLock objectLock(this);

        if (!m_inputFormatInitialized)
            return nullptr;

        return std::make_unique<WAVEFORMATEXTENSIBLE>(m_inputFormat);
    }

    std::unique_ptr<AudioDevice> AudioRenderer::GetDeviceFormat()
    {
        CAutoLock objectLock(this);

        return std::make_unique<AudioDevice>(m_device);
    }

    void AudioRenderer::InitializeProcessors()
    {
        CAutoLock objectLock(this);
        assert(m_inputFormatInitialized);

        const auto inRate = m_inputFormat.Format.nSamplesPerSec;
        const auto inChannels = m_inputFormat.Format.nChannels;
        const auto inMask = DspMatrix::GetChannelMask(m_inputFormat);
        const auto outRate = m_device.format.Format.nSamplesPerSec;
        const auto outChannels = m_device.format.Format.nChannels;
        const auto outMask = DspMatrix::GetChannelMask(m_device.format);

        m_dspMatrix.Initialize(inChannels, inMask, outChannels, outMask);
        m_dspRate.Initialize(inRate, outRate, outChannels);
        m_dspTempo.Initialize((float)m_rate, outRate, outChannels);
        m_dspCrossfeed.Initialize(!!m_settings->UseStereoCrossfeed(), outRate, outChannels, outMask);
        m_dspVolume.Initialize(m_device.exclusive);
        m_dspLimiter.Initialize(outRate, m_device.exclusive);
        m_dspDither.Initialize(m_device.dspFormat);
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

            // If the buffer is fully filled, set the corresponding event.
            (bufferPadding + doFrames == bufferFrames) ? m_bufferFilled.Set() : m_bufferFilled.Reset();

            doneFrames += doFrames;
            m_pushedFrames += doFrames;
        }

        return true;
    }
}
