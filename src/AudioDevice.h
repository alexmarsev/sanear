#pragma once

#include "DspChunk.h"
#include "DspFormat.h"

namespace SaneAudioRenderer
{
    struct AudioDeviceBackend final
    {
        SharedString          id;
        SharedString          adapterName;
        SharedString          endpointName;

        IAudioClientPtr       audioClient;
        IAudioRenderClientPtr audioRenderClient;
        IAudioClockPtr        audioClock;

        SharedWaveFormat      waveFormat;
        DspFormat             dspFormat;
        uint32_t              bufferDuration;
        REFERENCE_TIME        latency;
        bool                  exclusive;
        bool                  bitstream;
        bool                  realtime;
    };

    class AudioDevice final
    {
    public:

        AudioDevice(std::shared_ptr<AudioDeviceBackend> backend);
        AudioDevice(const AudioDevice&) = delete;
        AudioDevice& operator=(const AudioDevice&) = delete;
        ~AudioDevice();

        void Push(DspChunk& chunk, CAMEvent* pFilledEvent);
        REFERENCE_TIME Finish(CAMEvent* pFilledEvent);

        int64_t GetPosition();
        int64_t GetEnd();
        int64_t GetSilence();

        void Start();
        void Stop();
        void Reset();

        SharedString GetId()           const { return m_backend->id; }
        SharedString GetAdapterName()  const { return m_backend->adapterName; }
        SharedString GetEndpointName() const { return m_backend->endpointName; }

        IAudioClockPtr GetClock() { return m_backend->audioClock; }

        SharedWaveFormat GetWaveFormat()     const { return m_backend->waveFormat; }
        DspFormat        GetDspFormat()      const { return m_backend->dspFormat; }
        uint32_t         GetBufferDuration() const { return m_backend->bufferDuration; }
        REFERENCE_TIME   GetStreamLatency()  const { return m_backend->latency; }

        bool IsExclusive() const { return m_backend->exclusive; }
        bool IsBitstream() const { return m_backend->bitstream; }
        bool IsRealtime()  const { return m_backend->realtime; }

    private:

        void RealtimeFeed();
        void SilenceFeed();

        void PushToDevice(DspChunk& chunk, CAMEvent* pFilledEvent);
        UINT32 PushSilenceToDevice(UINT32 frames);
        void PushToBuffer(DspChunk& chunk);
        void RetrieveFromBuffer(DspChunk& chunk);

        std::shared_ptr<AudioDeviceBackend> m_backend;
        std::atomic<uint64_t> m_pushedFrames = 0;
        std::atomic<uint64_t> m_silenceFrames = 0;
        int64_t m_eos = 0;

        std::thread m_thread;
        CAMEvent m_wake;
        CAMEvent m_woken;
        CCritSec m_threadBusyMutex;
        std::atomic<bool> m_exit = false;
        std::atomic<bool> m_error = false;

        std::deque<DspChunk> m_buffer;
        size_t m_bufferFrameCount = 0;
        CCritSec m_bufferMutex;
    };
}
