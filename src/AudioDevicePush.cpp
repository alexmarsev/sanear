#include "pch.h"
#include "AudioDevicePush.h"

namespace SaneAudioRenderer
{
    AudioDevicePush::AudioDevicePush(std::shared_ptr<AudioDeviceBackend> backend)
        : m_woken(TRUE/*manual reset*/)
    {
        assert(backend);
        assert(!backend->eventMode);
        m_backend = backend;

        if (static_cast<HANDLE>(m_wake) == NULL ||
            static_cast<HANDLE>(m_woken) == NULL)
        {
            throw E_OUTOFMEMORY;
        }

        if (m_backend->realtime)
            m_thread = std::thread(std::bind(&AudioDevicePush::RealtimeFeed, this));
    }

    AudioDevicePush::~AudioDevicePush()
    {
        m_exit = true;
        m_wake.Set();

        if (m_thread.joinable())
            m_thread.join();

        assert(CheckLastInstances());
        m_backend = nullptr;
    }

    void AudioDevicePush::Push(DspChunk& chunk, CAMEvent* pFilledEvent)
    {
        assert(m_eos == 0);

        if (m_backend->realtime)
        {
            PushToBuffer(chunk);

            m_wake.Set();

            if (pFilledEvent && !chunk.IsEmpty())
                pFilledEvent->Set();
        }
        else
        {
            PushToDevice(chunk, pFilledEvent);
        }
    }

    REFERENCE_TIME AudioDevicePush::Finish(CAMEvent* pFilledEvent)
    {
        if (m_error)
            throw E_FAIL;

        if (m_eos == 0)
        {
            m_eos = GetEnd();

            try
            {
                if (!m_thread.joinable())
                {
                    assert(!m_exit);
                    m_thread = std::thread(std::bind(&AudioDevicePush::SilenceFeed, this));
                }
            }
            catch (std::system_error&)
            {
                throw E_OUTOFMEMORY;
            }
        }

        if (pFilledEvent)
            pFilledEvent->Set();

        return m_eos - GetPosition();
    }

    int64_t AudioDevicePush::GetPosition()
    {
        UINT64 deviceClockFrequency, deviceClockPosition;
        ThrowIfFailed(m_backend->audioClock->GetFrequency(&deviceClockFrequency));
        ThrowIfFailed(m_backend->audioClock->GetPosition(&deviceClockPosition, nullptr));

        return llMulDiv(deviceClockPosition, OneSecond, deviceClockFrequency, 0);
    }

    int64_t AudioDevicePush::GetEnd()
    {
        return llMulDiv(m_pushedFrames, OneSecond, m_backend->waveFormat->nSamplesPerSec, 0);
    }

    int64_t AudioDevicePush::GetSilence()
    {
        return llMulDiv(m_silenceFrames, OneSecond, m_backend->waveFormat->nSamplesPerSec, 0);
    }

    void AudioDevicePush::Start()
    {
        m_backend->audioClient->Start();
    }

    void AudioDevicePush::Stop()
    {
        m_backend->audioClient->Stop();
    }

    void AudioDevicePush::Reset()
    {
        if (!m_backend->realtime && m_thread.joinable())
        {
            m_exit = true;
            m_wake.Set();
            m_thread.join();
            m_exit = false;
        }

        {
            CAutoLock threadBusyLock(&m_threadBusyMutex);

            m_backend->audioClient->Reset();
            m_pushedFrames = 0;
            m_silenceFrames = 0;
            m_eos = 0;

            if (m_backend->realtime)
            {
                m_woken.Reset();

                CAutoLock lock(&m_bufferMutex);
                m_bufferFrameCount = 0;
                m_buffer.clear();
            }
        }

        if (m_backend->realtime)
        {
            m_wake.Set();
            m_woken.Wait();
        }
    }

    void AudioDevicePush::RealtimeFeed()
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
        TimePeriodHelper timePeriodHelper(1);

        while (!m_exit)
        {
            uint32_t sleepDuration = 0;

            {
                CAutoLock busyLock(&m_threadBusyMutex);

                if (m_error)
                {
                    sleepDuration = INFINITE;
                }
                else
                {
                    try
                    {
                        DspChunk chunk;

                        {
                            CAutoLock lock(&m_bufferMutex);

                            if (!m_buffer.empty())
                            {
                                chunk = std::move(m_buffer.front());
                                m_buffer.pop_front();
                                m_bufferFrameCount -= chunk.GetFrameCount();
                            }
                        }

                        if (chunk.IsEmpty())
                        {
                            REFERENCE_TIME latency = GetStreamLatency() + OneMillisecond * 2;
                            REFERENCE_TIME remaining = GetEnd() - GetPosition();

                            if (remaining < latency)
                                m_silenceFrames += PushSilenceToDevice(
                                    (UINT32)llMulDiv(m_backend->waveFormat->nSamplesPerSec,
                                                     latency - remaining, OneSecond, 0));

                            sleepDuration = 1;
                        }
                        else
                        {
                            PushToDevice(chunk, nullptr);

                            if (!chunk.IsEmpty())
                            {
                                {
                                    CAutoLock lock(&m_bufferMutex);
                                    m_bufferFrameCount += chunk.GetFrameCount();
                                    m_buffer.emplace_front(std::move(chunk));
                                }

                                sleepDuration = 1;
                            }
                        }
                    }
                    catch (HRESULT)
                    {
                        m_error = true;
                    }
                    catch (std::bad_alloc&)
                    {
                        m_error = true;
                    }
                }

                m_woken.Set();
            }

            m_wake.Wait(sleepDuration);
        }
    }

    void AudioDevicePush::SilenceFeed()
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        while (!m_exit && !m_error)
        {
            try
            {
                REFERENCE_TIME remaining = GetEnd() - GetPosition();
                REFERENCE_TIME buffer = m_backend->bufferDuration * OneMillisecond;

                if (remaining < buffer)
                {
                    m_silenceFrames += PushSilenceToDevice((UINT32)llMulDiv(m_backend->waveFormat->nSamplesPerSec,
                                                                            buffer - remaining, OneSecond, 0));
                }

                m_wake.Wait(m_backend->bufferDuration / 4);
            }
            catch (HRESULT)
            {
                m_error = true;
            }
        }
    }

    void AudioDevicePush::PushToDevice(DspChunk& chunk, CAMEvent* pFilledEvent)
    {
        // Get up-to-date information on the device buffer.
        UINT32 bufferFrames, bufferPadding;
        ThrowIfFailed(m_backend->audioClient->GetBufferSize(&bufferFrames));
        ThrowIfFailed(m_backend->audioClient->GetCurrentPadding(&bufferPadding));

        // Find out how many frames we can write this time.
        const UINT32 doFrames = std::min(bufferFrames - bufferPadding, (UINT32)chunk.GetFrameCount());

        if (doFrames == 0)
            return;

        // Write frames to the device buffer.
        BYTE* deviceBuffer;
        ThrowIfFailed(m_backend->audioRenderClient->GetBuffer(doFrames, &deviceBuffer));
        assert(chunk.GetFrameSize() == (m_backend->waveFormat->wBitsPerSample / 8 * m_backend->waveFormat->nChannels));
        memcpy(deviceBuffer, chunk.GetData(), doFrames * chunk.GetFrameSize());
        ThrowIfFailed(m_backend->audioRenderClient->ReleaseBuffer(doFrames, 0));

        // If the buffer is fully filled, set the corresponding event (if requested).
        if (pFilledEvent &&
            bufferPadding + doFrames == bufferFrames)
        {
            pFilledEvent->Set();
        }

        assert(doFrames <= chunk.GetFrameCount());
        chunk.ShrinkHead(chunk.GetFrameCount() - doFrames);

        m_pushedFrames += doFrames;
    }

    UINT32 AudioDevicePush::PushSilenceToDevice(UINT32 frames)
    {
        // Get up-to-date information on the device buffer.
        UINT32 bufferFrames, bufferPadding;
        ThrowIfFailed(m_backend->audioClient->GetBufferSize(&bufferFrames));
        ThrowIfFailed(m_backend->audioClient->GetCurrentPadding(&bufferPadding));

        // Find out how many frames we can write this time.
        const UINT32 doFrames = std::min(bufferFrames - bufferPadding, frames);

        if (doFrames == 0)
            return 0;

        // Write frames to the device buffer.
        BYTE* deviceBuffer;
        ThrowIfFailed(m_backend->audioRenderClient->GetBuffer(doFrames, &deviceBuffer));
        ThrowIfFailed(m_backend->audioRenderClient->ReleaseBuffer(doFrames, AUDCLNT_BUFFERFLAGS_SILENT));

        DebugOut("AudioDevice push", 1000. * doFrames / m_backend->waveFormat->nSamplesPerSec, "ms of silence");

        m_pushedFrames += doFrames;

        return doFrames;
    }

    void AudioDevicePush::PushToBuffer(DspChunk& chunk)
    {
        if (m_error)
            throw E_FAIL;

        if (chunk.IsEmpty())
            return;

        try
        {
            // Don't deny the allocator its right to reuse
            // IMediaSample while the chunk is hanging in the buffer.
            chunk.FreeMediaSample();

            CAutoLock lock(&m_bufferMutex);

            if (m_bufferFrameCount > m_backend->waveFormat->nSamplesPerSec / 4) // 250ms
                return;

            m_bufferFrameCount += chunk.GetFrameCount();
            m_buffer.emplace_back(std::move(chunk));
        }
        catch (std::bad_alloc&)
        {
            m_error = true;
            throw E_OUTOFMEMORY;
        }
    }
}
