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
        : private CCritSec
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

        LRESULT OnCheckBitstreamFormat();
        LRESULT OnCreateDevice();

        DWORD ThreadProc();
        LRESULT WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        // MSDN states:
        // "In Windows 8, the first use of IAudioClient to access the audio device should be on the STA thread.
        //  Calls from an MTA thread may result in undefined behavior."
        // We abide.
        ApartmentInvokeHelper m_staHelper;

        bool m_queuedCheckBitstream = false;
        bool m_queuedDestroy = false;
        bool m_queuedCreateDevice = false;

        HANDLE m_hThread = NULL;
        HWND m_hWindow = NULL;
        std::promise<bool> m_windowInitialized;

        std::shared_ptr<AudioDevice> m_device;

        SharedWaveFormat m_checkBitstreamFormat;
        ISettings* m_checkBitstreamSettings = nullptr;

        SharedWaveFormat m_createDeviceFormat;
        ISettings* m_createDeviceSettings = nullptr;
    };
}
