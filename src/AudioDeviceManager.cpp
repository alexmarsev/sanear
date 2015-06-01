#include "pch.h"
#include "AudioDeviceManager.h"

#include "DspMatrix.h"

namespace SaneAudioRenderer
{
    namespace
    {
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

        SharedString GetDevicePropertyString(IPropertyStore* pStore, REFPROPERTYKEY key)
        {
            assert(pStore);

            PROPVARIANT prop;
            PropVariantInit(&prop);
            ThrowIfFailed(pStore->GetValue(key, &prop));
            auto ret = std::make_shared<std::wstring>(prop.pwszVal);
            PropVariantClear(&prop);

            return ret;
        }

        void CreateAudioClient(AudioDeviceBackend& backend)
        {
            IMMDeviceEnumeratorPtr enumerator;
            ThrowIfFailed(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                           CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator)));

            IMMDevicePtr device;
            IPropertyStorePtr devicePropertyStore;

            if (!backend.friendlyName || backend.friendlyName->empty())
            {
                backend.default = true;
                ThrowIfFailed(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));
                ThrowIfFailed(device->OpenPropertyStore(STGM_READ, &devicePropertyStore));
                backend.friendlyName = GetDevicePropertyString(devicePropertyStore, PKEY_Device_FriendlyName);
            }
            else
            {
                auto targetName = backend.friendlyName;
                backend.default = false;

                IMMDeviceCollectionPtr collection;
                ThrowIfFailed(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection));

                UINT count = 0;
                ThrowIfFailed(collection->GetCount(&count));

                for (UINT i = 0; i < count; i++)
                {
                    ThrowIfFailed(collection->Item(i, &device));
                    ThrowIfFailed(device->OpenPropertyStore(STGM_READ, &devicePropertyStore));
                    backend.friendlyName = GetDevicePropertyString(devicePropertyStore, PKEY_Device_FriendlyName);

                    if (*targetName == *backend.friendlyName)
                        break;

                    device = nullptr;
                    devicePropertyStore = nullptr;
                    backend.friendlyName = nullptr;
                }
            }

            if (!device)
                return;

            backend.adapterName = GetDevicePropertyString(devicePropertyStore, PKEY_DeviceInterface_FriendlyName);
            backend.endpointName = GetDevicePropertyString(devicePropertyStore, PKEY_Device_DeviceDesc);

            ThrowIfFailed(device->Activate(__uuidof(IAudioClient),
                                           CLSCTX_INPROC_SERVER, nullptr, (void**)&backend.audioClient));
        }

        HRESULT CheckBitstreamFormat(SharedWaveFormat format, ISettings* pSettings)
        {
            assert(format);
            assert(pSettings);

            try
            {
                AudioDeviceBackend device = {};

                {
                    LPWSTR pDeviceName = nullptr;
                    ThrowIfFailed(pSettings->GetOuputDevice(&pDeviceName, nullptr, nullptr));

                    std::unique_ptr<wchar_t, CoTaskMemFreeDeleter> deviceName;
                    deviceName.reset(pDeviceName);

                    device.friendlyName = std::make_shared<std::wstring>(deviceName.get());
                }

                CreateAudioClient(device);

                if (!device.audioClient)
                    return 1;

                ThrowIfFailed(device.audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &(*format), nullptr));

                return S_OK;
            }
            catch (HRESULT ex)
            {
                return ex;
            }
        }

        HRESULT CreateAudioDeviceBackend(SharedWaveFormat format, bool live, ISettings* pSettings,
                                         std::shared_ptr<AudioDeviceBackend>& backend)
        {
            assert(format);
            assert(pSettings);

            try
            {
                backend = std::make_shared<AudioDeviceBackend>();

                {
                    LPWSTR pDeviceName = nullptr;
                    BOOL exclusive;
                    UINT32 buffer;
                    ThrowIfFailed(pSettings->GetOuputDevice(&pDeviceName, &exclusive, &buffer));

                    std::unique_ptr<wchar_t, CoTaskMemFreeDeleter> deviceName;
                    deviceName.reset(pDeviceName);

                    backend->friendlyName = std::make_shared<std::wstring>(deviceName.get());
                    backend->exclusive = !!exclusive;
                    backend->live = live;
                    backend->bufferDuration = buffer;
                }

                CreateAudioClient(*backend);

                if (!backend->audioClient)
                    return 1;

                WAVEFORMATEX* pFormat;
                ThrowIfFailed(backend->audioClient->GetMixFormat(&pFormat));
                SharedWaveFormat mixFormat(pFormat, CoTaskMemFreeDeleter());

                backend->bitstream = (DspFormatFromWaveFormat(*format) == DspFormat::Unknown);

                if (backend->bitstream)
                {
                    // Exclusive bitstreaming.
                    if (!backend->exclusive)
                        return 1;

                    backend->dspFormat = DspFormat::Unknown;
                    backend->waveFormat = format;
                }
                else if (backend->exclusive)
                {
                    // Exclusive.
                    auto inputRate = format->nSamplesPerSec;
                    auto mixRate = mixFormat->nSamplesPerSec;
                    auto mixChannels = mixFormat->nChannels;
                    auto mixMask = DspMatrix::GetChannelMask(*mixFormat);

                    auto priorities = make_array(
                        BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 32, 32, inputRate, mixChannels, mixMask),
                        BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 32, 32, inputRate, mixChannels, mixMask),
                        BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 24, 24, inputRate, mixChannels, mixMask),
                        BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 32, 24, inputRate, mixChannels, mixMask),
                        BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 16, 16, inputRate, mixChannels, mixMask),

                        BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 32, 32, mixRate, mixChannels, mixMask),
                        BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 32, 32, mixRate, mixChannels, mixMask),
                        BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 24, 24, mixRate, mixChannels, mixMask),
                        BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 32, 24, mixRate, mixChannels, mixMask),
                        BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 16, 16, mixRate, mixChannels, mixMask),

                        WAVEFORMATEXTENSIBLE{ BuildWaveFormat(WAVE_FORMAT_PCM, 16, inputRate, mixChannels) },
                        WAVEFORMATEXTENSIBLE{ BuildWaveFormat(WAVE_FORMAT_PCM, 16, mixRate, mixChannels) }
                    );

                    for (const auto& f : priorities)
                    {
                        assert(DspFormatFromWaveFormat(f.Format) != DspFormat::Unknown);

                        if (SUCCEEDED(backend->audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &f.Format, nullptr)))
                        {
                            backend->dspFormat = DspFormatFromWaveFormat(f.Format);
                            backend->waveFormat = CopyWaveFormat(f.Format);
                            break;
                        }
                    }
                }
                else
                {
                    // Shared.
                    backend->dspFormat = DspFormat::Float;
                    backend->waveFormat = mixFormat;
                }

                ThrowIfFailed(backend->audioClient->Initialize(
                                         backend->exclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED,
                                         AUDCLNT_STREAMFLAGS_NOPERSIST,
                                         MILLISECONDS_TO_100NS_UNITS(backend->bufferDuration),
                                         0, &(*backend->waveFormat), nullptr));

                ThrowIfFailed(backend->audioClient->GetService(IID_PPV_ARGS(&backend->audioRenderClient)));

                ThrowIfFailed(backend->audioClient->GetService(IID_PPV_ARGS(&backend->audioClock)));

                return S_OK;
            }
            catch (std::bad_alloc&)
            {
                backend = nullptr;
                return E_OUTOFMEMORY;
            }
            catch (HRESULT ex)
            {
                backend = nullptr;
                return ex;
            }
        }
    }

    AudioDeviceManager::AudioDeviceManager(HRESULT& result)
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

    AudioDeviceManager::~AudioDeviceManager()
    {
        m_exit = true;
        m_wake.Set();

        if (m_thread.joinable())
            m_thread.join();
    }

    bool AudioDeviceManager::BitstreamFormatSupported(SharedWaveFormat format, ISettings* pSettings)
    {
        assert(format);
        assert(pSettings);

        m_function = [&] { return CheckBitstreamFormat(format, pSettings); };
        m_wake.Set();
        m_done.Wait();

        return SUCCEEDED(m_result);
    }

    std::unique_ptr<AudioDevice> AudioDeviceManager::CreateDevice(SharedWaveFormat format, bool live, ISettings* pSettings)
    {
        assert(format);
        assert(pSettings);

        std::shared_ptr<AudioDeviceBackend> backend;

        m_function = [&] { return CreateAudioDeviceBackend(format, live, pSettings, backend); };
        m_wake.Set();
        m_done.Wait();

        if (FAILED(m_result))
            return nullptr;

        try
        {
            return std::unique_ptr<AudioDevice>(new AudioDevice(backend));
        }
        catch (std::bad_alloc&)
        {
            return nullptr;
        }
    }
}
