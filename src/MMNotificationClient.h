#pragma once
#include <comdef.h>
#include "../../../src/Settings.h"

namespace SaneAudioRenderer
{
    _COM_SMARTPTR_TYPEDEF(IMMDeviceEnumerator, __uuidof(IMMDeviceEnumerator));
    _COM_SMARTPTR_TYPEDEF(IMMNotificationClient, __uuidof(IMMNotificationClient));

    class CMMNotificationClient : public IMMNotificationClient
    {
    public:
        HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) override;
        ULONG STDMETHODCALLTYPE AddRef() override;
        ULONG STDMETHODCALLTYPE Release() override;
        HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
        HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
        HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
        HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
        HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;
        HRESULT Init(ISettings* pSettings);
        CMMNotificationClient();
        virtual ~CMMNotificationClient();
    private:
        LONG m_cRef;
        ISettingsPtr m_settings;
    };
}
