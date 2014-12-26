#pragma once

#include "DspFormat.h"
#include "Interfaces.h"

namespace SaneAudioRenderer
{
    struct AudioDevice final
    {
        std::shared_ptr<const std::wstring> friendlyName;
        std::shared_ptr<const std::wstring> adapterName;
        std::shared_ptr<const std::wstring> endpointName;
        UINT32                              settingsSerial;
        IAudioClientPtr                     audioClient;
        SharedWaveFormat                    waveFormat;
        uint32_t                            bufferDuration;
        IAudioRenderClientPtr               audioRenderClient;
        IAudioClockPtr                      audioClock;
        DspFormat                           dspFormat;
        bool                                exclusive;
        bool                                default;
    };

    class DeviceManager final
    {
    public:

        DeviceManager(HRESULT& result);
        DeviceManager(const DeviceManager&) = delete;
        DeviceManager& operator=(const DeviceManager&) = delete;
        ~DeviceManager();

        bool BitstreamFormatSupported(SharedWaveFormat format, ISettings* pSettings);
        bool CreateDevice(AudioDevice& device, SharedWaveFormat format, ISettings* pSettings);
        void ReleaseDevice();

    private:

        LRESULT OnCheckBitstreamFormat();
        LRESULT OnCreateDevice();

        DWORD ThreadProc();
        LRESULT WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        bool m_queuedCheckBitstream = false;
        bool m_queuedDestroy = false;
        bool m_queuedCreateDevice = false;

        HANDLE m_hThread = NULL;
        HWND m_hWindow = NULL;
        std::promise<bool> m_windowInitialized;

        AudioDevice m_device = {};

        SharedWaveFormat m_checkBitstreamFormat;
        ISettings* m_checkBitstreamSettings = nullptr;

        SharedWaveFormat m_createDeviceFormat;
        ISettings* m_createDeviceSettings = nullptr;
    };
}
