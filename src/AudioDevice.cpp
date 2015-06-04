#include "pch.h"
#include "AudioDevice.h"

namespace SaneAudioRenderer
{
    namespace
    {
        template <class T>
        bool IsLastInstance(T& smartPointer)
        {
            bool ret = (smartPointer.GetInterfacePtr()->AddRef() == 2);
            smartPointer.GetInterfacePtr()->Release();
            return ret;
        }
    }

    AudioDevice::AudioDevice(std::shared_ptr<AudioDeviceBackend> backend)
        : m_backend(backend)
    {
        assert(m_backend);

        if (m_backend->realtime)
            m_thread = std::thread(
                [this]
                {
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
                    TimePeriodHelper timePeriodHelper(1);

                    while (!m_exit)
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

                        try
                        {
                            if (chunk.IsEmpty())
                            {
                                REFERENCE_TIME latency = std::max(m_backend->latency, MILLISECONDS_TO_100NS_UNITS(5)) +
                                                         MILLISECONDS_TO_100NS_UNITS(1);
                                REFERENCE_TIME remaining = GetEnd() - GetPosition();

                                if (remaining < latency)
                                {
                                    PushSilenceToDevice((UINT32)llMulDiv(m_backend->waveFormat->nSamplesPerSec,
                                                                         OneSecond, latency - remaining, 0));
                                    DebugOut("AudioDevice inserting", (latency - remaining) / 10000., "ms of silence");
                                }
                            }
                            else
                            {
                                PushToDevice(chunk, nullptr);

                                if (!chunk.IsEmpty())
                                {
                                    CAutoLock lock(&m_bufferMutex);
                                    m_bufferFrameCount += chunk.GetFrameCount();
                                    m_buffer.emplace_front(std::move(chunk));
                                }
                            }
                        }
                        catch (HRESULT)
                        {
                            m_exit = true;
                            break;
                        }
                        catch (std::bad_alloc&)
                        {
                            m_exit = true;
                            break;
                        }

                        m_wake.Wait(1);
                    }
                }
            );
    }

    AudioDevice::~AudioDevice()
    {
        m_exit = true;
        m_wake.Set();

        if (m_thread.joinable())
            m_thread.join();

        auto areLastInstances = [this]
        {
            if (!m_backend.unique())
                return false;

            if (m_backend->audioClock && !IsLastInstance(m_backend->audioClock))
                return false;

            m_backend->audioClock = nullptr;

            if (m_backend->audioRenderClient && !IsLastInstance(m_backend->audioRenderClient))
                return false;

            m_backend->audioRenderClient = nullptr;

            if (m_backend->audioClient && !IsLastInstance(m_backend->audioClient))
                return false;

            return true;
        };
        assert(areLastInstances());

        m_backend = nullptr;
    }

    void AudioDevice::Push(DspChunk& chunk, CAMEvent* pFilledEvent)
    {
        if (IsRealtime())
        {
            PushToBuffer(chunk);

            m_wake.Set();

            if (pFilledEvent)
                pFilledEvent->Set();
        }
        else
        {
            PushToDevice(chunk, pFilledEvent);
        }
    }

    int64_t AudioDevice::GetPosition()
    {
        UINT64 deviceClockFrequency, deviceClockPosition;
        ThrowIfFailed(m_backend->audioClock->GetFrequency(&deviceClockFrequency));
        ThrowIfFailed(m_backend->audioClock->GetPosition(&deviceClockPosition, nullptr));

        return llMulDiv(deviceClockPosition, OneSecond, deviceClockFrequency, 0);
    }

    int64_t AudioDevice::GetEnd()
    {
        return llMulDiv(m_pushedFrames, OneSecond, m_backend->waveFormat->nSamplesPerSec, 0);
    }

    void AudioDevice::Start()
    {
        m_backend->audioClient->Start();
    }

    void AudioDevice::Stop()
    {
        m_backend->audioClient->Stop();
    }

    void AudioDevice::Reset()
    {
        m_backend->audioClient->Reset();
        m_pushedFrames = 0;
    }

    void AudioDevice::PushToDevice(DspChunk& chunk, CAMEvent* pFilledEvent)
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

    void AudioDevice::PushSilenceToDevice(UINT32 frames)
    {
        // Get up-to-date information on the device buffer.
        UINT32 bufferFrames, bufferPadding;
        ThrowIfFailed(m_backend->audioClient->GetBufferSize(&bufferFrames));
        ThrowIfFailed(m_backend->audioClient->GetCurrentPadding(&bufferPadding));

        // Find out how many frames we can write this time.
        const UINT32 doFrames = std::min(bufferFrames - bufferPadding, frames);

        if (doFrames == 0)
            return;

        // Write frames to the device buffer.
        BYTE* deviceBuffer;
        ThrowIfFailed(m_backend->audioRenderClient->GetBuffer(doFrames, &deviceBuffer));
        ThrowIfFailed(m_backend->audioRenderClient->ReleaseBuffer(doFrames, AUDCLNT_BUFFERFLAGS_SILENT));

        m_pushedFrames += doFrames;
    }

    void AudioDevice::PushToBuffer(DspChunk& chunk)
    {
        if (m_exit)
            throw E_FAIL;

        if (chunk.IsEmpty())
            return;

        try
        {
            CAutoLock lock(&m_bufferMutex);
            m_bufferFrameCount += chunk.GetFrameCount();
            m_buffer.emplace_back(std::move(chunk));
        }
        catch (std::bad_alloc&)
        {
            m_exit = true;
            throw E_OUTOFMEMORY;
        }
    }
}
