#include "pch.h"
#include "AudioRenderer.h"

namespace SaneAudioRenderer
{
    AudioRenderer::AudioRenderer(ISettings* pSettings, IMyClock* pClock, HRESULT& result)
        : m_deviceManager(result)
        , m_myClock(pClock)
        , m_flush(TRUE/*manual reset*/)
        , m_dspVolume(*this)
        , m_dspBalance(*this)
        , m_settings(pSettings)
    {
        if (FAILED(result))
            return;

        try
        {
            if (!m_settings || !m_myClock)
                throw E_UNEXPECTED;

            ThrowIfFailed(m_myClock->QueryInterface(IID_PPV_ARGS(&m_myGraphClock)));

            if (static_cast<HANDLE>(m_flush) == NULL)
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

        m_graphClock = pClock;

        if (m_graphClock && m_graphClock != m_myGraphClock)
        {
            if (!m_externalClock)
                ClearDevice();

            m_externalClock = true;
        }
        else
        {
            if (m_externalClock)
                ClearDevice();

            m_externalClock = false;
        }
    }

    bool AudioRenderer::OnExternalClock()
    {
        CAutoLock objectLock(this);

        return m_externalClock;
    }

    bool AudioRenderer::IsLive()
    {
        CAutoLock objectLock(this);

        return m_live;
    }

    bool AudioRenderer::Enqueue(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps, CAMEvent* pFilledEvent)
    {
        DspChunk chunk;

        {
            CAutoLock objectLock(this);
            assert(m_inputFormat);
            assert(m_state != State_Stopped);

            try
            {
                // Clear the device if related settings were changed.
                CheckDeviceSettings();

                // Create the device if needed.
                if (!m_device)
                    CreateDevice();

                // Apply sample corrections (pad, crop, guess timings).
                chunk = m_sampleCorrection.ProcessSample(pSample, sampleProps);

                // Apply clock corrections (what we couldn't correct with sample correction).
                if (!m_live && m_device && m_state == State_Running)
                    ApplyClockCorrection();

                // Apply dsp chain.
                if (m_device && !m_device->IsBitstream())
                {
                    auto f = [&](DspBase* pDsp)
                    {
                        pDsp->Process(chunk);
                    };

                    EnumerateProcessors(f);

                    DspChunk::ToFormat(m_device->GetDspFormat(), chunk);
                }

                // Apply rate corrections (rate matching or external clock).
                if (m_device && m_device->IsRealtime() && m_state == State_Running)
                    ApplyRateCorrection(chunk);

                // Don't deny the allocator to reuse IMediaSample while the chunk is hanging in the buffer.
                if (m_device && m_device->IsRealtime())
                    chunk.FreeMediaSample();
            }
            catch (HRESULT)
            {
                ClearDevice();
            }
            catch (std::bad_alloc&)
            {
                ClearDevice();
                chunk = DspChunk();
            }
        }

        // Send processed sample to the device.
        return Push(chunk, pFilledEvent);
    }

    bool AudioRenderer::Finish(bool blockUntilEnd, CAMEvent* pFilledEvent)
    {
        DspChunk chunk;

        {
            CAutoLock objectLock(this);
            assert(m_state != State_Stopped);

            // No device - nothing to block on.
            if (!m_device)
                blockUntilEnd = false;

            try
            {
                // Apply dsp chain.
                if (m_device && !m_device->IsBitstream())
                {
                    auto f = [&](DspBase* pDsp)
                    {
                        pDsp->Finish(chunk);
                    };

                    EnumerateProcessors(f);

                    DspChunk::ToFormat(m_device->GetDspFormat(), chunk);
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
            // Increase system timer resolution.
            TimePeriodHelper timePeriodHelper(1);

            // Unslave the clock because no more samples are going to be pushed.
            m_myClock->UnslaveClockFromAudio();

            for (;;)
            {
                int64_t actual = INT64_MAX;
                int64_t target;

                {
                    CAutoLock objectLock(this);

                    if (!m_device)
                        return true;

                    const auto previous = actual;
                    actual = m_device->GetPosition();
                    target = m_device->GetEnd();

                    // Return if the end of stream is reached.
                    if (actual == target)
                        return true;

                    // Stalling protection.
                    if (actual == previous && m_state == State_Running)
                        return true;
                }

                // Sleep until predicted end of stream.
                if (m_flush.Wait(std::max(1, (int32_t)((target - actual) * 1000 / OneSecond))))
                    return false;
            }
        };

        // Send processed sample to the device, and block until the buffer is drained (if requested).
        return Push(chunk, pFilledEvent) && (!blockUntilEnd || doBlock());
    }

    void AudioRenderer::BeginFlush()
    {
        m_flush.Set();
    }

    void AudioRenderer::EndFlush()
    {
        CAutoLock objectLock(this);
        assert(m_state != State_Running);

        if (m_device)
        {
            m_device->Reset();
            m_sampleCorrection.NewDeviceBuffer();
        }

        m_flush.Reset();
    }

    bool AudioRenderer::CheckFormat(SharedWaveFormat inputFormat, bool live)
    {
        assert(inputFormat);

        if (DspFormatFromWaveFormat(*inputFormat) != DspFormat::Unknown)
            return true;

        BOOL exclusive;
        m_settings->GetOuputDevice(nullptr, &exclusive, nullptr);
        BOOL bitstreamingAllowed;
        m_settings->GetAllowBitstreaming(&bitstreamingAllowed);

        if (!exclusive || !bitstreamingAllowed || live)
            return false;

        CAutoLock objectLock(this);

        return m_deviceManager.BitstreamFormatSupported(inputFormat, m_settings);
    }

    void AudioRenderer::SetFormat(SharedWaveFormat inputFormat, bool live)
    {
        CAutoLock objectLock(this);

        m_inputFormat = inputFormat;
        m_live = live;

        m_sampleCorrection.NewFormat(inputFormat);

        ClearDevice();
    }

    void AudioRenderer::NewSegment(double rate)
    {
        CAutoLock objectLock(this);

        m_startClockOffset = 0;
        m_rate = rate;

        m_sampleCorrection.NewSegment(m_rate);

        assert(m_inputFormat);
        if (m_device)
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

        if (m_device)
        {
            m_myClock->UnslaveClockFromAudio();
            m_device->Stop();
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

    const AudioDevice* AudioRenderer::GetAudioDevice()
    {
        assert(CritCheckIn(this));

        return m_device.get();
    }

    std::vector<std::wstring> AudioRenderer::GetActiveProcessors()
    {
        CAutoLock objectLock(this);

        std::vector<std::wstring> ret;

        if (m_inputFormat && m_device && !m_device->IsBitstream())
        {
            auto f = [&](DspBase* pDsp)
            {
                if (pDsp->Active())
                    ret.emplace_back(pDsp->Name());
            };

            EnumerateProcessors(f);
        }

        return ret;
    }

    void AudioRenderer::CheckDeviceSettings()
    {
        CAutoLock objectLock(this);

        UINT32 serial = m_settings->GetSerial();

        if (m_device && m_deviceSettingsSerial != serial)
        {
            LPWSTR pDeviceName = nullptr;
            BOOL exclusive;
            UINT32 buffer;
            if (SUCCEEDED(m_settings->GetOuputDevice(&pDeviceName, &exclusive, &buffer)))
            {
                if (m_device->IsExclusive() != !!exclusive ||
                    m_device->GetBufferDuration() != buffer ||
                    (pDeviceName && *pDeviceName && wcscmp(pDeviceName, m_device->GetFriendlyName()->c_str())) ||
                    ((!pDeviceName || !*pDeviceName) && !m_device->IsDefault()))
                {
                    ClearDevice();
                    assert(!m_device);
                }
                else
                {
                    m_deviceSettingsSerial = serial;
                }
                CoTaskMemFree(pDeviceName);
            }
        }
    }

    void AudioRenderer::StartDevice()
    {
        CAutoLock objectLock(this);
        assert(m_state == State_Running);

        if (m_device)
        {
            if (!m_live)
                m_myClock->SlaveClockToAudio(m_device->GetClock(), m_startTime + m_startClockOffset);

            m_device->Start();
        }
    }

    void AudioRenderer::CreateDevice()
    {
        CAutoLock objectLock(this);

        assert(!m_device);
        assert(m_inputFormat);

        m_deviceSettingsSerial = m_settings->GetSerial();
        m_device = m_deviceManager.CreateDevice(m_inputFormat, m_live || m_externalClock, m_settings);

        if (m_device)
        {
            m_sampleCorrection.NewDeviceBuffer();

            InitializeProcessors();

            m_startClockOffset = m_sampleCorrection.GetLastSampleEnd();

            if (m_state == State_Running)
                StartDevice();
        }
    }

    void AudioRenderer::ClearDevice()
    {
        CAutoLock objectLock(this);

        if (m_device)
        {
            m_myClock->UnslaveClockFromAudio();
            m_device->Stop();
            m_device = nullptr;
        }
    }

    void AudioRenderer::ApplyClockCorrection()
    {
        CAutoLock objectLock(this);
        assert(m_device);
        assert(m_state == State_Running);

        // Apply corrections to internal clock.
        {
            REFERENCE_TIME offset = m_sampleCorrection.GetTimingsError() - m_myClock->GetSlavedClockOffset();
            if (std::abs(offset) > 100)
            {
                m_myClock->OffsetSlavedClock(offset);
                DebugOut("AudioRenderer offset internal clock by", offset / 10000., "ms");
            }
        }
    }

    void AudioRenderer::ApplyRateCorrection(DspChunk& chunk)
    {
        CAutoLock objectLock(this);
        assert(m_device);
        assert(m_state == State_Running);

        if (chunk.IsEmpty())
            return;

        if (m_live)
        {
            // Rate matching.

            REFERENCE_TIME latency = llMulDiv(chunk.GetFrameCount(), OneSecond, chunk.GetRate(), 0) +
                                     m_device->GetStreamLatency() + OneMillisecond * 10;

            REFERENCE_TIME remaining = m_device->GetEnd() - m_device->GetPosition();

            if (remaining > latency)
            {
                size_t dropFrames = (size_t)llMulDiv(m_device->GetWaveFormat()->nSamplesPerSec,
                                                     remaining - latency, OneSecond, 0);

                dropFrames = std::min(chunk.GetFrameCount(), dropFrames);
                chunk.ShrinkHead(dropFrames);

                DebugOut("AudioRenderer drop", dropFrames, "frames for rate matching");
            }
        }
        else
        {
            // Clock matching.
            assert(m_externalClock);

            // TODO: write it.
        }

        /*
        // Try to match internal clock with graph clock if they're different.
        // We do it in the roundabout way by dynamically changing audio sampling rate.
        if (m_externalClock && !m_device->bitstream)
        {
            assert(m_dspRate.Active());
            REFERENCE_TIME graphTime, myTime, myStartTime;
            if (SUCCEEDED(m_myClock->GetAudioClockStartTime(&myStartTime)) &&
                SUCCEEDED(m_myClock->GetAudioClockTime(&myTime, nullptr)) &&
                SUCCEEDED(GetGraphTime(graphTime)) &&
                myTime > myStartTime)
            {
                REFERENCE_TIME offset = graphTime - myTime - m_correctedWithRateDsp;
                if (std::abs(offset) > MILLISECONDS_TO_100NS_UNITS(2))
                {
                    m_dspRate.Adjust(offset);
                    m_correctedWithRateDsp += offset;
                    DebugOut("AudioRenderer offset internal clock indirectly by", offset / 10000., "ms");
                }
            }
        }
        */
    }

    HRESULT AudioRenderer::GetGraphTime(REFERENCE_TIME& time)
    {
        CAutoLock objectLock(this);

        return m_graphClock ?
                   m_graphClock->GetTime(&time) :
                   m_myGraphClock->GetTime(&time);
    }

    void AudioRenderer::InitializeProcessors()
    {
        CAutoLock objectLock(this);
        assert(m_inputFormat);
        assert(m_device);

        m_correctedWithRateDsp = 0;

        if (m_device->IsBitstream())
            return;

        const auto inRate = m_inputFormat->nSamplesPerSec;
        const auto inChannels = m_inputFormat->nChannels;
        const auto inMask = DspMatrix::GetChannelMask(*m_inputFormat);
        const auto outRate = m_device->GetWaveFormat()->nSamplesPerSec;
        const auto outChannels = m_device->GetWaveFormat()->nChannels;
        const auto outMask = DspMatrix::GetChannelMask(*m_device->GetWaveFormat());

        m_dspMatrix.Initialize(inChannels, inMask, outChannels, outMask);
        m_dspRate.Initialize(m_live || m_externalClock, inRate, outRate, outChannels);
        m_dspVariableRate.Initialize(m_live || m_externalClock, inRate, outRate, outChannels);
        m_dspTempo.Initialize(m_rate, outRate, outChannels);
        m_dspCrossfeed.Initialize(m_settings, outRate, outChannels, outMask);
        m_dspLimiter.Initialize(outRate, outChannels, m_device->IsExclusive());
        m_dspDither.Initialize(m_device->GetDspFormat());
    }

    bool AudioRenderer::Push(DspChunk& chunk, CAMEvent* pFilledEvent)
    {
        bool firstIteration = true;
        uint32_t sleepDuration = 0;
        while (!chunk.IsEmpty())
        {
            // The device buffer is full or almost full at the beginning of the second and subsequent iterations.
            // Sleep until the buffer may have significant amount of free space. Unless interrupted.
            if (!firstIteration && m_flush.Wait(sleepDuration))
                return false;

            firstIteration = false;

            CAutoLock objectLock(this);

            assert(m_state != State_Stopped);

            if (m_device)
            {
                try
                {
                    m_device->Push(chunk, pFilledEvent);
                    sleepDuration = m_device->GetBufferDuration() / 4;
                }
                catch (HRESULT)
                {
                    ClearDevice();
                    sleepDuration = 0;
                }
            }
            else
            {
                // The code below emulates null audio device.

                if (pFilledEvent)
                    pFilledEvent->Set();

                sleepDuration = 1;

                // Loop until the graph time passes the current sample end.
                REFERENCE_TIME graphTime;
                if (m_state == State_Running &&
                    SUCCEEDED(GetGraphTime(graphTime)) &&
                    graphTime > m_startTime + m_sampleCorrection.GetLastSampleEnd())
                {
                    break;
                }
            }
        }

        return true;
    }
}
