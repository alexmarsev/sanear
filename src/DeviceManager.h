#pragma once

#include "DspFormat.h"

namespace SaneAudioRenderer
{
    struct AudioDevice final
    {
        std::shared_ptr<std::wstring> adapterName;
        std::shared_ptr<std::wstring> endpointName;
        IAudioClientPtr               audioClient;
        WAVEFORMATEXTENSIBLE          format; // TODO: move all WAVEFORMATEX variables to pointers
        uint32_t                      bufferDuration;
        IAudioRenderClientPtr         audioRenderClient;
        IAudioClockPtr                audioClock;
        DspFormat                     dspFormat;
        bool                          exclusive;
    };

    class DeviceManager final
    {
    public:

        DeviceManager(HRESULT& result);
        DeviceManager(const DeviceManager&) = delete;
        DeviceManager& operator=(const DeviceManager&) = delete;
        ~DeviceManager();

        bool CreateDevice(AudioDevice& device, const WAVEFORMATEXTENSIBLE& format, bool exclusive);
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
        std::promise<bool> m_windowInitialized;;

        AudioDevice m_device = {};
        WAVEFORMATEXTENSIBLE m_format;
        bool m_exclusive = false;
        WAVEFORMATEXTENSIBLE m_checkBitstreamFormat;
    };
}
