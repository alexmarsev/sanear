#include "pch.h"
#include "DeviceManager.h"

#include "DspMatrix.h"

namespace SaneAudioRenderer
{
    namespace
    {
        template <class T>
        bool IsLastInstance(T& smartPointer)
        {
            bool ret = (smartPointer.GetInterfacePtr()->AddRef() == 2);
            smartPointer.GetInterfacePtr()->Release();
            return ret;
        }

        WAVEFORMATEX BuildWaveFormat(WORD formatTag, uint32_t formatBits, uint32_t rate, uint32_t channelCount)
        {
            WAVEFORMATEX ret;

            ret.wFormatTag      = formatTag;
            ret.nChannels       = channelCount;
            ret.nSamplesPerSec  = rate;
            ret.nAvgBytesPerSec = formatBits / 8 * channelCount * rate;
            ret.nBlockAlign     = formatBits / 8 * channelCount;
            ret.wBitsPerSample  = formatBits;
            ret.cbSize          = (formatTag == WAVE_FORMAT_EXTENSIBLE) ? 22 : 0;

            return ret;
        }

        WAVEFORMATEXTENSIBLE BuildWaveFormatExt(GUID formatGuid, uint32_t formatBits, WORD formatExtProps,
                                                uint32_t rate, uint32_t channelCount, DWORD channelMask)
        {
            WAVEFORMATEXTENSIBLE ret;

            ret.Format                      = BuildWaveFormat(WAVE_FORMAT_EXTENSIBLE, formatBits, rate, channelCount);
            ret.Samples.wValidBitsPerSample = formatExtProps;
            ret.dwChannelMask               = channelMask;
            ret.SubFormat                   = formatGuid;

            return ret;
        }

        std::shared_ptr<std::wstring> GetDevicePropertyString(IPropertyStore* pStore, REFPROPERTYKEY key)
        {
            assert(pStore);

            PROPVARIANT prop;
            PropVariantInit(&prop);
            ThrowIfFailed(pStore->GetValue(key, &prop));
            auto ret = std::make_shared<std::wstring>(prop.pwszVal);
            PropVariantClear(&prop);

            return ret;
        }

        void CreateAudioClient(AudioDevice& audioDevice)
        {
            IMMDeviceEnumeratorPtr enumerator;
            ThrowIfFailed(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                           CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator)));

            IMMDevicePtr device;
            IPropertyStorePtr devicePropertyStore;

            if (!audioDevice.friendlyName || audioDevice.friendlyName->empty())
            {
                audioDevice.default = true;
                ThrowIfFailed(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));
                ThrowIfFailed(device->OpenPropertyStore(STGM_READ, &devicePropertyStore));
                audioDevice.friendlyName = GetDevicePropertyString(devicePropertyStore, PKEY_Device_FriendlyName);
            }
            else
            {
                auto targetName = audioDevice.friendlyName;
                audioDevice.default = false;

                IMMDeviceCollectionPtr collection;
                ThrowIfFailed(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection));

                UINT count = 0;
                ThrowIfFailed(collection->GetCount(&count));

                for (UINT i = 0; i < count; i++)
                {
                    ThrowIfFailed(collection->Item(i, &device));
                    ThrowIfFailed(device->OpenPropertyStore(STGM_READ, &devicePropertyStore));
                    audioDevice.friendlyName = GetDevicePropertyString(devicePropertyStore, PKEY_Device_FriendlyName);

                    if (*targetName == *audioDevice.friendlyName)
                        break;

                    device = nullptr;
                    devicePropertyStore = nullptr;
                    audioDevice.friendlyName = nullptr;
                }
            }

            if (!device)
                return;

            audioDevice.adapterName = GetDevicePropertyString(devicePropertyStore, PKEY_DeviceInterface_FriendlyName);
            audioDevice.endpointName = GetDevicePropertyString(devicePropertyStore, PKEY_Device_DeviceDesc);

            ThrowIfFailed(device->Activate(__uuidof(IAudioClient),
                                           CLSCTX_INPROC_SERVER, nullptr, (void**)&audioDevice.audioClient));
        }
    }

    DeviceManager::DeviceManager(HRESULT& result)
        : m_staHelper(result)
    {
        if (FAILED(result))
            return;

        try
        {
            if (static_cast<HANDLE>(m_wake) == NULL ||
                static_cast<HANDLE>(m_done) == NULL)
            {
                throw E_OUTOFMEMORY;
            }

            m_thread = std::thread(
                [this]
                {
                    CoInitializeHelper coInitializeHelper(COINIT_MULTITHREADED);

                    while (!m_exit)
                    {
                        m_wake.Wait();

                        if (m_function)
                        {
                            m_result = m_function();
                            m_function = nullptr;
                            m_done.Set();
                        }
                    }
                }
            );
        }
        catch (HRESULT ex)
        {
            result = ex;
        }
        catch (std::system_error&)
        {
            result = E_FAIL;
        }
    }

    DeviceManager::~DeviceManager()
    {
        if (m_device)
            ReleaseDevice();

        m_exit = true;
        m_wake.Set();

        if (m_thread.joinable())
            m_thread.join();
    }

    bool DeviceManager::BitstreamFormatSupported(SharedWaveFormat format, ISettings* pSettings)
    {
        assert(format);
        assert(pSettings);

        m_format = format;
        m_settings = pSettings;

        m_function = [this] { return OnCheckBitstreamFormat(); };
        m_wake.Set();
        m_done.Wait();

        bool ret = SUCCEEDED(m_result);

        m_format = nullptr;
        m_settings = nullptr;

        return ret;
    }

    SharedAudioDevice DeviceManager::CreateDevice(SharedWaveFormat format, ISettings* pSettings)
    {
        assert(format);
        assert(pSettings);

        m_format = format;
        m_settings = pSettings;

        m_function = [this] { return OnCreateDevice(); };
        m_wake.Set();
        m_done.Wait();

        SharedAudioDevice ret = SUCCEEDED(m_result) ? m_device : nullptr;

        m_format = nullptr;
        m_settings = nullptr;

        return ret;
    }

    void DeviceManager::ReleaseDevice()
    {
        m_function = [this] { return OnReleaseDevice(); };
        m_wake.Set();
        m_done.Wait();
    }

    HRESULT DeviceManager::OnCheckBitstreamFormat()
    {
        assert(m_format);
        assert(m_settings);

        try
        {
            AudioDevice device = {};

            {
                LPWSTR pDeviceName = nullptr;
                ThrowIfFailed(m_settings->GetOuputDevice(&pDeviceName, nullptr, nullptr));

                std::unique_ptr<wchar_t, CoTaskMemFreeDeleter> deviceName;
                deviceName.reset(pDeviceName);

                device.friendlyName = std::make_shared<std::wstring>(deviceName.get());
            }

            CreateAudioClient(device);

            if (!device.audioClient)
                return 1;

            auto staInvoke = [&](IAudioClient* pAudioClient)
            {
                return device.audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                                             &(*m_format), nullptr);
            };
            ThrowIfFailed(m_staHelper.Invoke<IAudioClient>(m_device->audioClient, staInvoke));

            return S_OK;
        }
        catch (HRESULT ex)
        {
            return ex;
        }
    }

    LRESULT DeviceManager::OnCreateDevice()
    {
        assert(m_format);
        assert(m_settings);

        OnReleaseDevice();
        try
        {
            assert(!m_device);
            m_device = std::make_shared<AudioDevice>();

            {
                LPWSTR pDeviceName = nullptr;
                BOOL exclusive;
                UINT32 buffer;
                ThrowIfFailed(m_settings->GetOuputDevice(&pDeviceName, &exclusive, &buffer));

                std::unique_ptr<wchar_t, CoTaskMemFreeDeleter> deviceName;
                deviceName.reset(pDeviceName);

                m_device->friendlyName = std::make_shared<std::wstring>(deviceName.get());
                m_device->exclusive = !!exclusive;
                m_device->bufferDuration = buffer;
            }

            CreateAudioClient(*m_device);

            if (!m_device->audioClient)
                return 1;

            WAVEFORMATEX* pFormat;
            ThrowIfFailed(m_device->audioClient->GetMixFormat(&pFormat));
            SharedWaveFormat mixFormat(pFormat, CoTaskMemFreeDeleter());

            m_device->bitstream = (DspFormatFromWaveFormat(*m_format) == DspFormat::Unknown);

            if (m_device->bitstream)
            {
                // Exclusive bitstreaming.
                if (!m_device->exclusive)
                    return 1;

                m_device->dspFormat = DspFormat::Unknown;
                m_device->waveFormat = m_format;
            }
            else if (m_device->exclusive)
            {
                // Exclusive.
                auto inputRate = m_format->nSamplesPerSec;
                auto mixRate = mixFormat->nSamplesPerSec;
                auto mixChannels = mixFormat->nChannels;
                auto mixMask = DspMatrix::GetChannelMask(*mixFormat);

                auto priorities = make_array(
                    BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 32, 32, inputRate, mixChannels, mixMask),
                    BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM,        32, 32, inputRate, mixChannels, mixMask),
                    BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM,        24, 24, inputRate, mixChannels, mixMask),
                    BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM,        32, 24, inputRate, mixChannels, mixMask),
                    BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM,        16, 16, inputRate, mixChannels, mixMask),

                    BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 32, 32, mixRate, mixChannels, mixMask),
                    BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM,        32, 32, mixRate, mixChannels, mixMask),
                    BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM,        24, 24, mixRate, mixChannels, mixMask),
                    BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM,        32, 24, mixRate, mixChannels, mixMask),
                    BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM,        16, 16, mixRate, mixChannels, mixMask),

                    WAVEFORMATEXTENSIBLE{BuildWaveFormat(WAVE_FORMAT_PCM, 16, inputRate, mixChannels)},
                    WAVEFORMATEXTENSIBLE{BuildWaveFormat(WAVE_FORMAT_PCM, 16, mixRate,   mixChannels)}
                );

                for (const auto& f : priorities)
                {
                    assert(DspFormatFromWaveFormat(f.Format) != DspFormat::Unknown);

                    if (SUCCEEDED(m_device->audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &f.Format, nullptr)))
                    {
                        m_device->dspFormat = DspFormatFromWaveFormat(f.Format);
                        m_device->waveFormat = CopyWaveFormat(f.Format);
                        break;
                    }
                }
            }
            else
            {
                // Shared.
                m_device->dspFormat = DspFormat::Float;
                m_device->waveFormat = mixFormat;
            }

            auto staInvoke = [this](IAudioClient* pAudioClient)
            {
                return pAudioClient->Initialize(m_device->exclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED,
                                                0, MILLISECONDS_TO_100NS_UNITS(m_device->bufferDuration),
                                                0, &(*m_device->waveFormat), nullptr);
            };
            ThrowIfFailed(m_staHelper.Invoke<IAudioClient>(m_device->audioClient, staInvoke));

            ThrowIfFailed(m_device->audioClient->GetService(IID_PPV_ARGS(&m_device->audioRenderClient)));

            ThrowIfFailed(m_device->audioClient->GetService(IID_PPV_ARGS(&m_device->audioClock)));

            return S_OK;
        }
        catch (std::bad_alloc&)
        {
            OnReleaseDevice();
            return E_OUTOFMEMORY;
        }
        catch (HRESULT ex)
        {
            OnReleaseDevice();
            return ex;
        }
    }

    HRESULT DeviceManager::OnReleaseDevice()
    {
        if (!m_device)
            return S_OK;

        auto areLastInstances = [this]
        {
            if (!m_device.unique())
                return false;

            if (m_device->audioClock && !IsLastInstance(m_device->audioClock))
                return false;

            m_device->audioClock = nullptr;

            if (m_device->audioRenderClient && !IsLastInstance(m_device->audioRenderClient))
                return false;

            m_device->audioRenderClient = nullptr;

            if (m_device->audioClient && !IsLastInstance(m_device->audioClient))
                return false;

            return true;
        };
        assert(areLastInstances());

        m_device = nullptr;

        return S_OK;
    }
}
