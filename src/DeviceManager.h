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
        WAVEFORMATEXTENSIBLE                format; // TODO: move all WAVEFORMATEX variables to pointers
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

        bool CreateDevice(AudioDevice& device, const WAVEFORMATEXTENSIBLE& format, ISettings* pSettings);
        void ReleaseDevice();
        bool BitstreamFormatSupported(const WAVEFORMATEXTENSIBLE& format);

    private:

        LRESULT OnCreateDevice();
        LRESULT OnCheckBitstreamFormat();

        DWORD ThreadProc();
        LRESULT WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        bool m_queuedDestroy = false;
        bool m_queuedCreate = false;
        bool m_queuedCheckBitstream = false;

        HANDLE m_hThread = NULL;
        HWND m_hWindow = NULL;
        std::promise<bool> m_windowInitialized;

        AudioDevice m_device = {};
        WAVEFORMATEXTENSIBLE m_format;
        ISettings* m_pSettings = nullptr;
        WAVEFORMATEXTENSIBLE m_checkBitstreamFormat;
    };
}
