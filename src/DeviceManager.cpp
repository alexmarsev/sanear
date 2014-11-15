#include "pch.h"
#include "DeviceManager.h"

namespace SaneAudioRenderer
{
    namespace
    {
        const auto WindowClass = TEXT("SaneAudioRenderer::DeviceManager");
        const auto WindowTitle = TEXT("");

        enum
        {
            WM_CREATE_DEVICE = WM_USER + 100,
        };

        template <class T>
        bool IsLastInstance(T& smartPointer)
        {
            bool ret = (smartPointer.GetInterfacePtr()->AddRef() == 2);
            smartPointer.GetInterfacePtr()->Release();
            return ret;
        }
    }

    DeviceManager::DeviceManager(HRESULT& result)
    {
        if (FAILED(result))
            return;

        m_hThread = (HANDLE)_beginthreadex(nullptr, 0, StaticThreadProc<DeviceManager>, this, 0, nullptr);

        if (m_hThread == NULL || !m_windowInitialized.get_future().get())
            result = E_FAIL;
    }

    DeviceManager::~DeviceManager()
    {
        PostMessage(m_hWindow, WM_DESTROY, 0, 0);
        WaitForSingleObject(m_hThread, INFINITE);
        CloseHandle(m_hThread);
    }

    bool DeviceManager::CreateDevice(AudioDevice& device)
    {
        device = {};
        bool ret = (SendMessage(m_hWindow, WM_CREATE_DEVICE, 0, 0) == 0);
        device = m_device;
        return ret;
    }

    LRESULT DeviceManager::OnCreateDevice()
    {
        ReleaseDevice();
        try
        {
            IMMDeviceEnumeratorPtr enumerator;
            ThrowIfFailed(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                           CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator)));

            IMMDevicePtr device;
            ThrowIfFailed(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));
            ThrowIfFailed(device->Activate(__uuidof(IAudioClient),
                                           CLSCTX_INPROC_SERVER, nullptr, (void**)&m_device.audioClient));

            WAVEFORMATEX* pFormat;
            ThrowIfFailed(m_device.audioClient->GetMixFormat(&pFormat));
            m_device.format = pFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE ?
                                  *reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pFormat) :
                                  WAVEFORMATEXTENSIBLE{*pFormat};
            CoTaskMemFree(pFormat);

            m_device.bufferDuration = 1000;

            ThrowIfFailed(m_device.audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
                                                           MILLISECONDS_TO_100NS_UNITS(m_device.bufferDuration),
                                                           0, &m_device.format.Format, nullptr));

            ThrowIfFailed(m_device.audioClient->GetStreamLatency(&m_device.streamLatency));

            ThrowIfFailed(m_device.audioClient->GetService(IID_PPV_ARGS(&m_device.audioRenderClient)));

            ThrowIfFailed(m_device.audioClient->GetService(IID_PPV_ARGS(&m_device.audioClock)));

            return 0;
        }
        catch (HRESULT)
        {
            ReleaseDevice();
            return 1;
        }
    }

    void DeviceManager::ReleaseDevice()
    {
        auto areLastInstances = [this]
        {
            if (m_device.audioClock && !IsLastInstance(m_device.audioClock))
                return false;

            m_device.audioClock = nullptr;

            if (m_device.audioRenderClient && !IsLastInstance(m_device.audioRenderClient))
                return false;

            m_device.audioRenderClient = nullptr;

            if (m_device.audioClient && !IsLastInstance(m_device.audioClient))
                return false;

            return true;
        };
        assert(areLastInstances());

        m_device = {};
    }

    DWORD DeviceManager::ThreadProc()
    {
        CoInitializeHelper coInitializeHelper(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        HINSTANCE hInstance = GetModuleHandle(nullptr);

        WNDCLASSEX windowClass{sizeof(windowClass), 0, StaticWindowProc<DeviceManager>, 0, 0, hInstance,
                               NULL, NULL, NULL, nullptr, WindowClass, NULL};

        RegisterClassEx(&windowClass);

        m_hWindow = CreateWindowEx(0, WindowClass, WindowTitle, 0, 0, 0, 0, 0, 0, NULL, hInstance, this);

        if (m_hWindow != NULL && coInitializeHelper.Initialized())
        {
            m_windowInitialized.set_value(true);
            RunMessageLoop();
            ReleaseDevice();
        }
        else
        {
            m_windowInitialized.set_value(false);
        }

        return 0;
    }

    LRESULT DeviceManager::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            case WM_CREATE_DEVICE:
                return OnCreateDevice();

            default:
                return DefWindowProc(hWnd, msg, wParam, lParam);
        }
    }
}
