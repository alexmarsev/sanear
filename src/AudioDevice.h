#pragma once

#include "DspChunk.h"
#include "DspFormat.h"

namespace SaneAudioRenderer
{
    struct AudioDeviceBackend final
    {
        SharedString          friendlyName;
        SharedString          adapterName;
        SharedString          endpointName;

        IAudioClientPtr       audioClient;
        IAudioRenderClientPtr audioRenderClient;
        IAudioClockPtr        audioClock;

        SharedWaveFormat      waveFormat;
        DspFormat             dspFormat;
        uint32_t              bufferDuration;
        bool                  exclusive;
        bool                  bitstream;
        bool                  default;
    };

    class AudioDevice final
    {
    public:

        AudioDevice(std::shared_ptr<AudioDeviceBackend> backend);
        AudioDevice(const AudioDevice&) = delete;
        AudioDevice& operator=(const AudioDevice&) = delete;
        ~AudioDevice();

        void Push(DspChunk& chunk, CAMEvent* pFilledEvent);
        int64_t GetPosition();
        int64_t GetEnd();

        void Start();
        void Stop();
        void Reset();

        SharedString GetFriendlyName() const { return m_backend->friendlyName; }
        SharedString GetAdapterName()  const { return m_backend->adapterName; }
        SharedString GetEndpointName() const { return m_backend->endpointName; }

        IAudioClockPtr GetClock() { return m_backend->audioClock; }

        SharedWaveFormat GetWaveFormat()     const { return m_backend->waveFormat; }
        DspFormat        GetDspFormat()      const { return m_backend->dspFormat; }
        uint32_t         GetBufferDuration() const { return m_backend->bufferDuration; }

        bool IsExclusive() const { return m_backend->exclusive; }
        bool IsBitstream() const { return m_backend->bitstream; }
        bool IsDefault()   const { return m_backend->default; }

    private:

        std::shared_ptr<AudioDeviceBackend> m_backend;
        uint64_t m_pushedFrames = 0;
    };
}
