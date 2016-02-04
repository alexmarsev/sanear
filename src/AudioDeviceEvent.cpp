#include "pch.h"
#include "AudioDeviceEvent.h"

namespace SaneAudioRenderer
{
    namespace
    {
        WinapiFunc<decltype(AvSetMmThreadCharacteristicsW)>
        AvSetMmThreadCharacteristicsFunction(L"avrt.dll", "AvSetMmThreadCharacteristicsW");

        WinapiFunc<decltype(AvRevertMmThreadCharacteristics)>
        AvRevertMmThreadCharacteristicsFunction(L"avrt.dll", "AvRevertMmThreadCharacteristics");
    }

    AudioDeviceEvent::AudioDeviceEvent(std::shared_ptr<AudioDeviceBackend> backend)
    {
        DebugOut(ClassName(this), "create");

        assert(backend);
        assert(backend->eventMode);
        m_backend = backend;

        if (static_cast<HANDLE>(m_wake) == NULL)
            throw E_OUTOFMEMORY;

        ThrowIfFailed(backend->audioClient->SetEventHandle(m_wake));

        m_thread = std::thread(std::bind(&AudioDeviceEvent::EventFeed, this));
    }

    AudioDeviceEvent::~AudioDeviceEvent()
    {
        DebugOut(ClassName(this), "destroy");

        m_exit = true;
        m_wake.Set();

        if (m_thread.joinable())
            m_thread.join();

        assert(CheckLastInstances());
        m_backend = nullptr;
    }

    void AudioDeviceEvent::Push(DspChunk& chunk, CAMEvent* pFilledEvent)
    {
        assert(!m_endOfStream);

        if (m_error)
            throw E_FAIL;

        PushChunkToBuffer(chunk);

        if (pFilledEvent && !chunk.IsEmpty())
            pFilledEvent->Set();
    }

    REFERENCE_TIME AudioDeviceEvent::Finish(CAMEvent* pFilledEvent)
    {
        if (m_error)
            throw E_FAIL;

        if (!m_endOfStream)
        {
            DebugOut(ClassName(this), "finish");
            m_endOfStream = true;
            m_endOfStreamPos = GetEnd();
        }

        if (pFilledEvent)
            pFilledEvent->Set();

        return m_endOfStreamPos - GetPosition();
    }

    int64_t AudioDeviceEvent::GetPosition()
    {
        UINT64 deviceClockFrequency, deviceClockPosition;
        ThrowIfFailed(m_backend->audioClock->GetFrequency(&deviceClockFrequency));
        ThrowIfFailed(m_backend->audioClock->GetPosition(&deviceClockPosition, nullptr));

        return llMulDiv(deviceClockPosition, OneSecond, deviceClockFrequency, 0);
    }

    int64_t AudioDeviceEvent::GetEnd()
    {
        return llMulDiv(m_receivedFrames, OneSecond, m_backend->waveFormat->nSamplesPerSec, 0);
    }

    int64_t AudioDeviceEvent::GetSilence()
    {
        return llMulDiv(m_silenceFrames, OneSecond, m_backend->waveFormat->nSamplesPerSec, 0);
    }

    void AudioDeviceEvent::Start()
    {
        bool delegateStart = false;

        {
            CAutoLock threadLock(&m_threadMutex);

            if (m_sentFrames == 0)
            {
                m_queuedStart = true;
                delegateStart = true;
            }
        }

        if (delegateStart)
        {
            DebugOut(ClassName(this), "queue start");
            m_wake.Set();
        }
        else
        {
            DebugOut(ClassName(this), "start");
            m_backend->audioClient->Start();
        }
    }

    void AudioDeviceEvent::Stop()
    {
        DebugOut(ClassName(this), "stop");

        {
            CAutoLock threadLock(&m_threadMutex);
            m_queuedStart = false;
        }

        m_backend->audioClient->Stop();
    }

    void AudioDeviceEvent::Reset()
    {
        DebugOut(ClassName(this), "reset");

        {
            CAutoLock threadLock(&m_threadMutex);

            m_backend->audioClient->Reset();

            m_endOfStream = false;
            m_endOfStreamPos = 0;

            m_receivedFrames = 0;
            m_sentFrames = 0;
            m_silenceFrames = 0;

            {
                CAutoLock bufferLock(&m_bufferMutex);
                m_bufferFrames = 0;
                m_buffer.clear();
            }
        }
    }

    void AudioDeviceEvent::EventFeed()
    {
        HANDLE taskHandle = NULL;
        if (AvSetMmThreadCharacteristicsFunction && AvRevertMmThreadCharacteristicsFunction)
        {
            DWORD taskIndex = 0;
            taskHandle = AvSetMmThreadCharacteristicsFunction(L"Pro Audio", &taskIndex);
            assert(taskHandle != NULL);
        }

        if (taskHandle == NULL)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        while (!m_exit)
        {
            {
                CAutoLock threadLock(&m_threadMutex);

                if (!m_error)
                {
                    try
                    {
                        PushBufferToDevice();

                        if (m_queuedStart)
                        {
                            DebugOut(ClassName(this), "start");
                            m_backend->audioClient->Start();
                            m_queuedStart = false;
                        }
                    }
                    catch (HRESULT)
                    {
                        m_error = true;
                    }
                }
            }

            m_wake.Wait();
        }

        if (taskHandle != NULL)
            AvRevertMmThreadCharacteristicsFunction(taskHandle);
    }

    void AudioDeviceEvent::PushBufferToDevice()
    {
        UINT32 deviceFrames = m_backend->deviceBufferSize;

        if (!m_backend->exclusive)
        {
            UINT32 bufferPadding;
            ThrowIfFailed(m_backend->audioClient->GetCurrentPadding(&bufferPadding));
            deviceFrames -= bufferPadding;
        }

        if (deviceFrames == 0)
            return;

        CAutoLock bufferLock(&m_bufferMutex);

        if (deviceFrames > m_bufferFrames && !m_endOfStream && !m_backend->realtime)
            return;

        BYTE* deviceBuffer;
        ThrowIfFailed(m_backend->audioRenderClient->GetBuffer(deviceFrames, &deviceBuffer));

        const size_t frameSize = m_backend->waveFormat->wBitsPerSample / 8 * m_backend->waveFormat->nChannels;

        for (UINT32 doneFrames = 0;;)
        {
            if (m_buffer.empty())
            {
                assert(m_endOfStream || m_backend->realtime);
                UINT32 doFrames = deviceFrames - doneFrames;

                if (doneFrames == 0)
                {
                    ThrowIfFailed(m_backend->audioRenderClient->ReleaseBuffer(deviceFrames, AUDCLNT_BUFFERFLAGS_SILENT));
                }
                else
                {
                    ZeroMemory(deviceBuffer + doneFrames * frameSize, doFrames * frameSize);
                    ThrowIfFailed(m_backend->audioRenderClient->ReleaseBuffer(deviceFrames, 0));
                }

                DebugOut(ClassName(this), "silence", doFrames * 1000. / m_backend->waveFormat->nSamplesPerSec, "ms");

                m_silenceFrames += doFrames;

                break;
            }
            else
            {
                DspChunk& chunk = m_buffer.front();
                UINT32 doFrames = std::min(deviceFrames - doneFrames, (UINT32)chunk.GetFrameCount());
                assert(chunk.GetFrameSize() == frameSize);
                memcpy(deviceBuffer + doneFrames * frameSize, chunk.GetData(), doFrames * frameSize);

                doneFrames += doFrames;
                m_bufferFrames -= doFrames;

                if (deviceFrames == doneFrames)
                {
                    ThrowIfFailed(m_backend->audioRenderClient->ReleaseBuffer(deviceFrames, 0));

                    chunk.ShrinkHead(chunk.GetFrameCount() - doFrames);
                    if (chunk.IsEmpty())
                        m_buffer.pop_front();

                    break;
                }

                m_buffer.pop_front();
            }
        }

        m_sentFrames += deviceFrames;
    }

    void AudioDeviceEvent::PushChunkToBuffer(DspChunk& chunk)
    {
        if (chunk.IsEmpty())
            return;

        try
        {
            // Don't deny the allocator its right to reuse
            // IMediaSample while the chunk is hanging in the buffer.
            chunk.FreeMediaSample();

            size_t targetFrames = (size_t)llMulDiv(m_backend->bufferDuration,
                                                   m_backend->waveFormat->nSamplesPerSec, 1000, 0);

            CAutoLock bufferLock(&m_bufferMutex);

            if (m_bufferFrames > targetFrames)
                return;

            size_t chunkFrames = chunk.GetFrameCount();

            m_bufferFrames += chunkFrames;
            m_buffer.emplace_back(std::move(chunk));

            m_receivedFrames += chunkFrames;
        }
        catch (std::bad_alloc&)
        {
            m_error = true;
            throw E_OUTOFMEMORY;
        }
    }
}
