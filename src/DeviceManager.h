#pragma once

#include "ApartmentInvokeHelper.h"
#include "DspFormat.h"
#include "Interfaces.h"

namespace SaneAudioRenderer
{
    struct AudioDevice final
    {
        std::shared_ptr<const std::wstring> friendlyName;
        std::shared_ptr<const std::wstring> adapterName;
        std::shared_ptr<const std::wstring> endpointName;
        IAudioClientPtr                     audioClient;
        SharedWaveFormat                    waveFormat;
        uint32_t                            bufferDuration;
        IAudioRenderClientPtr               audioRenderClient;
        IAudioClockPtr                      audioClock;
        DspFormat                           dspFormat;
        bool                                exclusive;
        bool                                bitstream;
        bool                                default;
    };

    typedef std::shared_ptr<const AudioDevice> SharedAudioDevice;

    class DeviceManager final
    {
    public:

        DeviceManager(HRESULT& result);
        DeviceManager(const DeviceManager&) = delete;
        DeviceManager& operator=(const DeviceManager&) = delete;
        ~DeviceManager();

        bool BitstreamFormatSupported(SharedWaveFormat format, ISettings* pSettings);
        SharedAudioDevice CreateDevice(SharedWaveFormat format, ISettings* pSettings);
        void ReleaseDevice();

    private:

        HRESULT OnCheckBitstreamFormat();
        HRESULT OnCreateDevice();
        HRESULT OnReleaseDevice();

        // MSDN states:
        // "In Windows 8, the first use of IAudioClient to access the audio device should be on the STA thread.
        //  Calls from an MTA thread may result in undefined behavior."
        // We abide.
        ApartmentInvokeHelper m_staHelper;

        std::thread m_thread;
        std::atomic<bool> m_exit = false;
        CAMEvent m_wake;
        CAMEvent m_done;

        std::function<HRESULT(void)> m_function;
        HRESULT m_result = S_OK;

        SharedWaveFormat m_format;
        ISettings* m_settings = nullptr;
        std::shared_ptr<AudioDevice> m_device;
    };
}
