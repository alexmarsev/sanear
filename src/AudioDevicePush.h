#pragma once

#include "AudioDevice.h"
#include "DspChunk.h"
#include "DspFormat.h"

namespace SaneAudioRenderer
{
    class AudioDevicePush final
        : public AudioDevice
    {
    public:

        AudioDevicePush(std::shared_ptr<AudioDeviceBackend> backend);
        AudioDevicePush(const AudioDevicePush&) = delete;
        AudioDevicePush& operator=(const AudioDevicePush&) = delete;
        ~AudioDevicePush();

        void Push(DspChunk& chunk, CAMEvent* pFilledEvent) override;
        REFERENCE_TIME Finish(CAMEvent* pFilledEvent) override;

        int64_t GetPosition() override;
        int64_t GetEnd() override;
        int64_t GetSilence() override;

        void Start() override;
        void Stop() override;
        void Reset() override;

    private:

        void RealtimeFeed();
        void SilenceFeed();

        void PushToDevice(DspChunk& chunk, CAMEvent* pFilledEvent);
        UINT32 PushSilenceToDevice(UINT32 frames);
        void PushToBuffer(DspChunk& chunk);

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
