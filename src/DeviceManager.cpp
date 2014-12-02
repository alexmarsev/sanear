#include "pch.h"
#include "DeviceManager.h"

#include "DspMatrix.h"

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


        WAVEFORMATEXTENSIBLE BuildFormat(GUID formatGuid, uint32_t formatBits, WORD formatExtProps,
                                         uint32_t rate, uint32_t channelCount, DWORD channelMask)
        {
            WAVEFORMATEXTENSIBLE ret;
            ret.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            ret.Format.nChannels = channelCount;
            ret.Format.nSamplesPerSec = rate;
            ret.Format.nAvgBytesPerSec = formatBits / 8 * rate * channelCount;
            ret.Format.nBlockAlign = formatBits / 8 * channelCount;
            ret.Format.wBitsPerSample = formatBits;
            ret.Format.cbSize = 22;
            ret.Samples.wValidBitsPerSample = formatExtProps;
            ret.dwChannelMask = channelMask;
            ret.SubFormat = formatGuid;
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
        m_queuedDestroy = true;
        PostMessage(m_hWindow, WM_DESTROY, 0, 0);
        WaitForSingleObject(m_hThread, INFINITE);
        CloseHandle(m_hThread);
    }

    bool DeviceManager::CreateDevice(AudioDevice& device, const WAVEFORMATEXTENSIBLE& format, bool exclusive)
    {
        device = {};
        m_format = format;
        m_exclusive = exclusive;
        m_queuedCreate = true;
        bool ret = (SendMessage(m_hWindow, WM_CREATE_DEVICE, 0, 0) == 0);
        device = m_device;
        return ret;
    }

    LRESULT DeviceManager::OnCreateDevice()
    {
        if (!m_queuedCreate)
            return 1;

        m_queuedCreate = false;

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
            WAVEFORMATEXTENSIBLE mixFormat = pFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE ?
                                                 *reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pFormat) :
                                                 WAVEFORMATEXTENSIBLE{*pFormat};
            CoTaskMemFree(pFormat);

            m_device.bufferDuration = 200;

            m_device.exclusive = m_exclusive;

            if (m_device.exclusive)
            {
                auto priorities = make_array(
                    std::make_pair(DspFormat::Float, BuildFormat(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 32, 32, m_format.Format.nSamplesPerSec, mixFormat.Format.nChannels, DspMatrix::GetChannelMask(mixFormat))),
                    std::make_pair(DspFormat::Pcm32, BuildFormat(KSDATAFORMAT_SUBTYPE_PCM,        32, 32, m_format.Format.nSamplesPerSec, mixFormat.Format.nChannels, DspMatrix::GetChannelMask(mixFormat))),
                    std::make_pair(DspFormat::Pcm24, BuildFormat(KSDATAFORMAT_SUBTYPE_PCM,        24, 24, m_format.Format.nSamplesPerSec, mixFormat.Format.nChannels, DspMatrix::GetChannelMask(mixFormat))),
                    std::make_pair(DspFormat::Pcm32, BuildFormat(KSDATAFORMAT_SUBTYPE_PCM,        32, 24, m_format.Format.nSamplesPerSec, mixFormat.Format.nChannels, DspMatrix::GetChannelMask(mixFormat))),
                    std::make_pair(DspFormat::Pcm16, BuildFormat(KSDATAFORMAT_SUBTYPE_PCM,        16, 16, m_format.Format.nSamplesPerSec, mixFormat.Format.nChannels, DspMatrix::GetChannelMask(mixFormat))),

                    std::make_pair(DspFormat::Float, BuildFormat(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 32, 32, mixFormat.Format.nSamplesPerSec, mixFormat.Format.nChannels, DspMatrix::GetChannelMask(mixFormat))),
                    std::make_pair(DspFormat::Pcm32, BuildFormat(KSDATAFORMAT_SUBTYPE_PCM,        32, 32, mixFormat.Format.nSamplesPerSec, mixFormat.Format.nChannels, DspMatrix::GetChannelMask(mixFormat))),
                    std::make_pair(DspFormat::Pcm24, BuildFormat(KSDATAFORMAT_SUBTYPE_PCM,        24, 24, mixFormat.Format.nSamplesPerSec, mixFormat.Format.nChannels, DspMatrix::GetChannelMask(mixFormat))),
                    std::make_pair(DspFormat::Pcm32, BuildFormat(KSDATAFORMAT_SUBTYPE_PCM,        32, 24, mixFormat.Format.nSamplesPerSec, mixFormat.Format.nChannels, DspMatrix::GetChannelMask(mixFormat))),
                    std::make_pair(DspFormat::Pcm16, BuildFormat(KSDATAFORMAT_SUBTYPE_PCM,        16, 16, mixFormat.Format.nSamplesPerSec, mixFormat.Format.nChannels, DspMatrix::GetChannelMask(mixFormat))),

                    std::make_pair(DspFormat::Float, mixFormat)
                );

                for (const auto& f : priorities)
                {
                    if (SUCCEEDED(m_device.audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &f.second.Format, nullptr)))
                    {
                        m_device.dspFormat = f.first;
                        m_device.format = f.second;
                        break;
                    }
                }
            }
            else
            {
                m_device.dspFormat = DspFormat::Float;
                m_device.format = mixFormat;
            }

            ThrowIfFailed(m_device.audioClient->Initialize(m_device.exclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED,
                                                           0, MILLISECONDS_TO_100NS_UNITS(m_device.bufferDuration),
                                                           0, &m_device.format.Format, nullptr));

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

        if (RegisterClassEx(&windowClass))
        {
            m_hWindow = CreateWindowEx(0, WindowClass, WindowTitle, 0, 0, 0, 0, 0, 0, NULL, hInstance, this);
            UnregisterClass(WindowClass, hInstance);
        }
        else
        {
            m_hWindow = NULL;
        }

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
                if (m_queuedDestroy)
                    PostQuitMessage(0);
                return 0;

            case WM_CREATE_DEVICE:
                return OnCreateDevice();

            default:
                return DefWindowProc(hWnd, msg, wParam, lParam);
        }
    }
}
