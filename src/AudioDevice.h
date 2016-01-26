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
        UINT32                endpointFormFactor;

        IAudioClientPtr       audioClient;
        IAudioRenderClientPtr audioRenderClient;
        IAudioClockPtr        audioClock;

        SharedWaveFormat      mixFormat;

        SharedWaveFormat      waveFormat;
        DspFormat             dspFormat;
        uint32_t              bufferDuration;
        REFERENCE_TIME        latency;
        bool                  exclusive;
        bool                  bitstream;
        bool                  realtime;

        bool                  ignoredSystemChannelMixer;
    };

    class AudioDevice
    {
    public:

        virtual ~AudioDevice() = default;

        virtual void Push(DspChunk& chunk, CAMEvent* pFilledEvent) = 0;
        virtual REFERENCE_TIME Finish(CAMEvent* pFilledEvent) = 0;

        virtual int64_t GetPosition() = 0;
        virtual int64_t GetEnd() = 0;
        virtual int64_t GetSilence() = 0;

        virtual void Start() = 0;
        virtual void Stop() = 0;
        virtual void Reset() = 0;

        SharedString GetId()           const { return m_backend->id; }
        SharedString GetAdapterName()  const { return m_backend->adapterName; }
        SharedString GetEndpointName() const { return m_backend->endpointName; }

        IAudioClockPtr GetClock() { return m_backend->audioClock; }

        SharedWaveFormat GetMixFormat()      const { return m_backend->mixFormat; }

        SharedWaveFormat GetWaveFormat()     const { return m_backend->waveFormat; }
        DspFormat        GetDspFormat()      const { return m_backend->dspFormat; }
        uint32_t         GetBufferDuration() const { return m_backend->bufferDuration; }
        REFERENCE_TIME   GetStreamLatency()  const { return m_backend->latency; }

        bool IsExclusive() const { return m_backend->exclusive; }

        bool IgnoredSystemChannelMixer() const { return m_backend->ignoredSystemChannelMixer; }

    protected:

        std::shared_ptr<AudioDeviceBackend> m_backend;
    };
}
