#pragma once

#include "DspFormat.h"

namespace SaneAudioRenderer
{
    struct AudioDevice final
    {
        IAudioClientPtr       audioClient;
        WAVEFORMATEXTENSIBLE  format;
        uint32_t              bufferDuration;
        IAudioRenderClientPtr audioRenderClient;
        IAudioClockPtr        audioClock;
        DspFormat             dspFormat;
        bool                  exclusive;
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

    private:

        LRESULT OnCreateDevice();

        DWORD ThreadProc();
        LRESULT WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        bool m_queuedDestroy = false;
        bool m_queuedCreate = false;

        HANDLE m_hThread;
        HWND m_hWindow = NULL;
        std::promise<bool> m_windowInitialized;;

        AudioDevice m_device;
        WAVEFORMATEXTENSIBLE m_format;
        bool m_exclusive;
    };
}
