#pragma once

#include "AudioDevice.h"
#include "Interfaces.h"

namespace SaneAudioRenderer
{
    class AudioDeviceManager final
    {
    public:

        AudioDeviceManager(HRESULT& result);
        AudioDeviceManager(const AudioDeviceManager&) = delete;
        AudioDeviceManager& operator=(const AudioDeviceManager&) = delete;
        ~AudioDeviceManager();

        bool BitstreamFormatSupported(SharedWaveFormat format, ISettings* pSettings);
        std::unique_ptr<AudioDevice> CreateDevice(SharedWaveFormat format, bool realtime, ISettings* pSettings);

    private:

        std::thread m_thread;
        std::atomic<bool> m_exit = false;
        CAMEvent m_wake;
        CAMEvent m_done;

        std::function<HRESULT(void)> m_function;
        HRESULT m_result = S_OK;
    };
}
