#include "pch.h"
#include "AudioRenderer.h"

namespace SaneAudioRenderer
{
    AudioRenderer::AudioRenderer(ISettings* pSettings, IMyClock* pClock, CAMEvent& bufferFilled, HRESULT& result)
        : m_deviceManager(result)
        , m_myClock(pClock)
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
            if (!m_settings || !m_myClock)
                throw E_UNEXPECTED;

            ThrowIfFailed(m_myClock->QueryInterface(IID_PPV_ARGS(&m_graphClock)));

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
    }

    void AudioRenderer::SetClock(IReferenceClock* pClock)
    {
        CAutoLock objectLock(this);

        ThrowIfFailed(m_myClock->QueryInterface(IID_PPV_ARGS(&m_graphClock)));
        m_externalClock = false;

        if (pClock && m_graphClock != pClock)
        {
            m_graphClock = pClock;

            if (!m_externalClock)
            {
                m_externalClock = true;
                ClearDevice();
            }
        }
    }

    bool AudioRenderer::OnExternalClock()
    {
        CAutoLock objectLock(this);

        return m_externalClock;
    }

    bool AudioRenderer::Enqueue(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps)
    {
        DspChunk chunk;

        {
            CAutoLock objectLock(this);
            assert(m_inputFormat);
            assert(m_state != State_Stopped);

            CheckDeviceSettings();

            if (!m_deviceInitialized &&
                m_deviceManager.CreateDevice(m_device, m_inputFormat, m_settings))
            {
                m_deviceInitialized = true;

                InitializeProcessors();

                m_startOffset = m_lastSampleEnd;

                if (m_state == State_Running)
                    StartDevice();
            }

            if (!(sampleProps.dwSampleFlags & AM_SAMPLE_TIMEVALID))
            {
                sampleProps.tStart = m_firstSampleStart;
                sampleProps.tStart += (REFERENCE_TIME)((m_receivedFramesTimeInPreviousFormats +
                                                        llMulDiv(m_receivedFrames, OneSecond,
                                                                 m_inputFormat->nSamplesPerSec, 0)) / m_rate);
                sampleProps.dwSampleFlags |= AM_SAMPLE_TIMEVALID;
            }

            if (!(sampleProps.dwSampleFlags & AM_SAMPLE_STOPVALID))
            {
                REFERENCE_TIME duration = sampleProps.lActual * 8 / m_inputFormat->wBitsPerSample /
                                          m_inputFormat->nChannels * OneSecond / m_inputFormat->nSamplesPerSec;
                sampleProps.tStop = sampleProps.tStart + (REFERENCE_TIME)(duration / m_rate);
                sampleProps.dwSampleFlags |= AM_SAMPLE_STOPVALID;
            }

            try
            {
                if (m_device.dspFormat == DspFormat::Unknown)
                {
                    chunk = DspChunk(pSample, sampleProps, *m_inputFormat);

                    if (m_receivedFrames == 0)
                        m_firstSampleStart = sampleProps.tStart;
                    m_receivedFrames += chunk.GetFrameCount();
                }
                else if (m_lastSampleEnd > 0)
                {
                    chunk = DspChunk(pSample, sampleProps, *m_inputFormat);

                    assert(m_receivedFrames > 0 || m_receivedFramesTimeInPreviousFormats > 0);
                    m_receivedFrames += chunk.GetFrameCount();
                }
                else
                {
                    chunk = PreProcessFirstSamples(pSample, sampleProps);
                }

                if (m_deviceInitialized && m_state == State_Running)
                {
                    {
                        REFERENCE_TIME offset = sampleProps.tStart - m_myClock->GetSlavedClockOffset() -
                                                (REFERENCE_TIME)(m_receivedFramesTimeInPreviousFormats +
                                                                 llMulDiv(m_receivedFrames - chunk.GetFrameCount(), OneSecond,
                                                                          m_inputFormat->nSamplesPerSec, 0) / m_rate);
                        if (std::abs(offset) > 1000)
                        {
                            m_myClock->OffsetSlavedClock(offset);
                            //DbgOutString((std::to_wstring(offset) + L" " + std::to_wstring(sampleProps.tStop - sampleProps.tStart) + L"\n").c_str());
                        }
                    }

                    if (m_externalClock && m_device.dspFormat != DspFormat::Unknown)
                    {
                        assert(m_dspRate.Active());
                        REFERENCE_TIME graphTime, myTime, myStartTime;
                        if (SUCCEEDED(m_myClock->GetAudioClockStartTime(&myStartTime)) &&
                            SUCCEEDED(m_myClock->GetAudioClockTime(&myTime, nullptr)) &&
                            SUCCEEDED(m_graphClock->GetTime(&graphTime)) &&
                            myTime > myStartTime)
                        {
                            REFERENCE_TIME offset = graphTime - myTime - m_correctedWithRateDsp;
                            if (std::abs(offset) > MILLISECONDS_TO_100NS_UNITS(2))
                            {
                                //DbgOutString((std::to_wstring(offset) + L" " + std::to_wstring(m_corrected) + L"\n").c_str());
                                m_dspRate.Adjust(offset);
                                m_correctedWithRateDsp += offset;
                            }
                        }
                    }
                }

                if (m_deviceInitialized && m_device.dspFormat != DspFormat::Unknown)
                {
                    m_dspMatrix.Process(chunk);
                    m_dspRate.Process(chunk);
                    m_dspTempo.Process(chunk);
                    m_dspCrossfeed.Process(chunk);
                    m_dspVolume.Process(chunk);
                    m_dspBalance.Process(chunk);
                    m_dspLimiter.Process(chunk);
                    m_dspDither.Process(chunk);

                    DspChunk::ToFormat(m_device.dspFormat, chunk);
                }
            }
            catch (std::bad_alloc&)
            {
                ClearDevice();
                chunk = DspChunk();
            }

            m_lastSampleEnd = sampleProps.tStop;
        }

        return Push(chunk);
    }

    bool AudioRenderer::Finish(bool blockUntilEnd)
    {
        DspChunk chunk;

        {
            CAutoLock objectLock(this);
            assert(m_state != State_Stopped);

            if (!m_deviceInitialized)
                blockUntilEnd = false;

            try
            {
                if (m_deviceInitialized && m_device.dspFormat != DspFormat::Unknown)
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

            m_myClock->UnslaveClockFromAudio();

            for (;;)
            {
                int64_t actual = INT64_MAX;
                int64_t target;

                {
                    CAutoLock objectLock(this);

                    if (!m_deviceInitialized)
                        return true;

                    UINT64 deviceClockFrequency, deviceClockPosition;

                    try
                    {
                        ThrowIfFailed(m_device.audioClock->GetFrequency(&deviceClockFrequency));
                        ThrowIfFailed(m_device.audioClock->GetPosition(&deviceClockPosition, nullptr));
                    }
                    catch (HRESULT)
                    {
                        ClearDevice();
                        return true;
                    }

                    const auto previous = actual;
                    actual = llMulDiv(deviceClockPosition, OneSecond, deviceClockFrequency, 0);
                    target = llMulDiv(m_pushedFrames, OneSecond, m_device.waveFormat->nSamplesPerSec, 0);

                    if (actual == target)
                        return true;

                    if (actual == previous && m_state == State_Running)
                        return true;
                }

                if (m_flush.Wait(std::max(1, (int32_t)((target - actual) * 1000 / OneSecond))))
                    return false;
            }
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

        if (m_deviceInitialized)
        {
            m_device.audioClient->Reset();
            m_bufferFilled.Reset();
        }

        m_flush.Reset();

        m_pushedFrames = 0;
    }

    bool AudioRenderer::CheckFormat(SharedWaveFormat inputFormat)
    {
        assert(inputFormat);

        if (DspFormatFromWaveFormat(*inputFormat) != DspFormat::Unknown)
            return true;

        BOOL exclusive;
        m_settings->GetOuputDevice(nullptr, &exclusive);
        BOOL bitstreamingAllowed;
        m_settings->GetAllowBitstreaming(&bitstreamingAllowed);

        if (!exclusive || !bitstreamingAllowed)
            return false;

        CAutoLock objectLock(this);

        return m_deviceManager.BitstreamFormatSupported(inputFormat, m_settings);
    }

    void AudioRenderer::SetFormat(SharedWaveFormat inputFormat)
    {
        CAutoLock objectLock(this);

        if (m_inputFormat && m_state != State_Stopped)
        {
            m_receivedFramesTimeInPreviousFormats += llMulDiv(m_receivedFrames, OneSecond,
                                                              m_inputFormat->nSamplesPerSec, 0);
            m_receivedFrames = 0;
        }

        m_inputFormat = inputFormat;

        ClearDevice();
    }

    void AudioRenderer::NewSegment(double rate)
    {
        CAutoLock objectLock(this);

        // It makes things a lot easier when rate is within float precision,
        // please add a cast to your player's code.
        assert((double)(float)rate == rate);

        m_startOffset = 0;
        m_receivedFramesTimeInPreviousFormats = 0;
        m_receivedFrames = 0;
        m_firstSampleStart = 0;
        m_lastSampleEnd = 0;
        m_rate = rate;

        assert(m_inputFormat);
        if (m_deviceInitialized)
            InitializeProcessors();
    }

    void AudioRenderer::Play(REFERENCE_TIME startTime)
    {
        CAutoLock objectLock(this);
        assert(m_state != State_Running);
        m_state = State_Running;

        m_startTime = startTime;
        StartDevice();
    }

    void AudioRenderer::Pause()
    {
        CAutoLock objectLock(this);
        m_state = State_Paused;

        if (m_deviceInitialized)
        {
            m_myClock->UnslaveClockFromAudio();
            m_device.audioClient->Stop();
        }
    }

    void AudioRenderer::Stop()
    {
        CAutoLock objectLock(this);
        m_state = State_Stopped;

        ClearDevice();
    }

    SharedWaveFormat AudioRenderer::GetInputFormat()
    {
        CAutoLock objectLock(this);

        return m_inputFormat;
    }

    std::unique_ptr<AudioDevice> AudioRenderer::GetDeviceFormat()
    {
        CAutoLock objectLock(this);

        if (!m_deviceInitialized)
            return nullptr;

        return std::make_unique<AudioDevice>(m_device);
    }

    std::vector<std::wstring> AudioRenderer::GetActiveProcessors()
    {
        CAutoLock objectLock(this);

        std::vector<std::wstring> ret;

        if (m_inputFormat && m_deviceInitialized && m_device.dspFormat != DspFormat::Unknown)
        {
            if (m_dspMatrix.Active())
                ret.emplace_back(m_dspMatrix.Name());

            if (m_dspRate.Active())
                ret.emplace_back(m_dspRate.Name());

            if (m_dspTempo.Active())
                ret.emplace_back(m_dspTempo.Name());

            if (m_dspCrossfeed.Active())
                ret.emplace_back(m_dspCrossfeed.Name());

            if (m_dspVolume.Active())
                ret.emplace_back(m_dspVolume.Name());

            if (m_dspBalance.Active())
                ret.emplace_back(m_dspBalance.Name());

            if (m_dspLimiter.Active())
                ret.emplace_back(m_dspLimiter.Name());

            if (m_dspDither.Active())
                ret.emplace_back(m_dspDither.Name());
        }

        return ret;
    }

    DspChunk AudioRenderer::PreProcessFirstSamples(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps)
    {
        CAutoLock objectLock(this);

        assert(m_inputFormat);

        DspChunk chunk;

        auto timeToFrames = [&](REFERENCE_TIME time)
        {
            return (size_t)(time * m_inputFormat->nSamplesPerSec / OneSecond * m_rate);
        };

        auto framesToTime = [&](size_t frames)
        {
            return (REFERENCE_TIME)(frames * OneSecond / m_inputFormat->nSamplesPerSec / m_rate);
        };

        if (sampleProps.tStop <= m_lastSampleEnd)
        {
            // Drop the sample.
            assert(chunk.IsEmpty());
        }
        else if (sampleProps.tStart < m_lastSampleEnd)
        {
            // Crop the sample.
            size_t cropFrames = timeToFrames(m_lastSampleEnd - sampleProps.tStart);

            if (cropFrames > 0)
            {
                size_t cropBytes = cropFrames * m_inputFormat->nChannels * m_inputFormat->wBitsPerSample / 8;

                assert((int32_t)cropBytes < sampleProps.lActual);
                sampleProps.pbBuffer += cropBytes;
                sampleProps.lActual -= (int32_t)cropBytes;
                sampleProps.tStart += framesToTime(cropFrames);

                chunk = DspChunk(pSample, sampleProps, *m_inputFormat);
            }
            else
            {
                chunk = DspChunk(pSample, sampleProps, *m_inputFormat);
            }

            if (m_receivedFrames == 0)
                m_firstSampleStart = sampleProps.tStart;
            m_receivedFrames += chunk.GetFrameCount();
        }
        else if (sampleProps.tStart > m_lastSampleEnd)
        {
            // Zero-pad the sample.
            size_t extendFrames = timeToFrames(sampleProps.tStart - m_lastSampleEnd);

            if (extendFrames > 0)
            {
                DspChunk tempChunk(pSample, sampleProps, *m_inputFormat);

                size_t extendBytes = extendFrames * tempChunk.GetFrameSize();
                sampleProps.pbBuffer = nullptr;
                sampleProps.lActual += extendBytes;
                sampleProps.tStart -= framesToTime(extendFrames);

                if (m_receivedFrames == 0)
                    m_firstSampleStart = sampleProps.tStart;
                m_receivedFrames += tempChunk.GetFrameCount() + extendFrames;

                chunk = DspChunk(tempChunk.GetFormat(), tempChunk.GetChannelCount(),
                                 tempChunk.GetFrameCount() + extendFrames, tempChunk.GetRate());

                assert(chunk.GetSize() == tempChunk.GetSize() + extendBytes);
                ZeroMemory(chunk.GetData(), extendBytes);
                memcpy(chunk.GetData() + extendBytes, tempChunk.GetConstData(), tempChunk.GetSize());
            }
            else
            {
                chunk = DspChunk(pSample, sampleProps, *m_inputFormat);

                if (m_receivedFrames == 0)
                    m_firstSampleStart = sampleProps.tStart;
                m_receivedFrames += chunk.GetFrameCount();
            }
        }
        else
        {
            // Leave the sample untouched.
            chunk = DspChunk(pSample, sampleProps, *m_inputFormat);

            if (m_receivedFrames == 0)
                m_firstSampleStart = sampleProps.tStart;
            m_receivedFrames += chunk.GetFrameCount();
        }

        return chunk;
    }

    void AudioRenderer::CheckDeviceSettings()
    {
        CAutoLock objectLock(this);

        UINT32 serial = m_settings->GetSerial();

        if (m_deviceInitialized &&
            m_device.settingsSerial != serial)
        {
            LPWSTR pDeviceName = nullptr;
            BOOL exclusive;
            if (SUCCEEDED(m_settings->GetOuputDevice(&pDeviceName, &exclusive)))
            {
                if (m_device.exclusive != !!exclusive ||
                    (pDeviceName && *pDeviceName && wcscmp(pDeviceName, m_device.friendlyName->c_str())) ||
                    ((!pDeviceName || !*pDeviceName) && !m_device.default))
                {
                    ClearDevice();
                    assert(!m_deviceInitialized);
                }
                else
                {
                    m_device.settingsSerial = serial;
                }
                CoTaskMemFree(pDeviceName);
            }
        }
    }

    void AudioRenderer::StartDevice()
    {
        CAutoLock objectLock(this);
        assert(m_state == State_Running);

        if (m_deviceInitialized)
        {
            m_myClock->SlaveClockToAudio(m_device.audioClock, m_startTime + m_startOffset);
            m_startOffset = 0;
            m_device.audioClient->Start();
            //assert(m_bufferFilled.Check());
        }
    }

    void AudioRenderer::ClearDevice()
    {
        CAutoLock objectLock(this);

        if (m_deviceInitialized)
        {
            m_myClock->UnslaveClockFromAudio();
            m_device.audioClient->Stop();
            m_bufferFilled.Reset();
        }

        m_deviceInitialized = false;
        m_device = {};
        m_deviceManager.ReleaseDevice();

        m_pushedFrames = 0;
    }

    void AudioRenderer::InitializeProcessors()
    {
        CAutoLock objectLock(this);
        assert(m_inputFormat);
        assert(m_deviceInitialized);

        m_correctedWithRateDsp = 0;

        if (m_device.dspFormat == DspFormat::Unknown)
            return;

        const auto inRate = m_inputFormat->nSamplesPerSec;
        const auto inChannels = m_inputFormat->nChannels;
        const auto inMask = DspMatrix::GetChannelMask(*m_inputFormat);
        const auto outRate = m_device.waveFormat->nSamplesPerSec;
        const auto outChannels = m_device.waveFormat->nChannels;
        const auto outMask = DspMatrix::GetChannelMask(*m_device.waveFormat);

        m_dspMatrix.Initialize(inChannels, inMask, outChannels, outMask);
        m_dspRate.Initialize(m_externalClock, inRate, outRate, outChannels);
        m_dspTempo.Initialize((float)m_rate, outRate, outChannels);
        m_dspCrossfeed.Initialize(m_settings, outRate, outChannels, outMask);
        m_dspVolume.Initialize(m_device.exclusive);
        m_dspLimiter.Initialize(m_settings, outRate, m_device.exclusive);
        m_dspDither.Initialize(m_device.dspFormat);
    }

    bool AudioRenderer::Push(DspChunk& chunk)
    {
        if (chunk.IsEmpty())
            return true;

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

            if (m_deviceInitialized)
            {
                try
                {
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
                    assert(frameSize == (m_device.waveFormat->wBitsPerSample / 8 * m_device.waveFormat->nChannels));
                    memcpy(deviceBuffer, chunk.GetConstData() + doneFrames * frameSize, doFrames * frameSize);
                    ThrowIfFailed(m_device.audioRenderClient->ReleaseBuffer(doFrames, 0));

                    // If the buffer is fully filled, set the corresponding event.
                    (bufferPadding + doFrames == bufferFrames) ? m_bufferFilled.Set() : m_bufferFilled.Reset();

                    doneFrames += doFrames;
                    m_pushedFrames += doFrames;

                    continue;
                }
                catch (HRESULT)
                {
                    ClearDevice();
                }
            }

            assert(!m_deviceInitialized);
            m_bufferFilled.Set();
            REFERENCE_TIME graphTime;
            if (m_state == State_Running &&
                SUCCEEDED(m_graphClock->GetTime(&graphTime)) &&
                graphTime + MILLISECONDS_TO_100NS_UNITS(20) > m_startTime + m_lastSampleEnd)
            {
                break;
            }
        }

        return true;
    }
}
