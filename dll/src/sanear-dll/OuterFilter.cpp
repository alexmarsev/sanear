#include "pch.h"
#include "OuterFilter.h"

#include "../../../src/Factory.h"

namespace SaneAudioRenderer
{
    namespace
    {
        const auto DeviceId = L"DeviceId";
        const auto DeviceExclusive = L"DeviceExclusive";
        const auto DeviceBufferDuration = L"DeviceBufferDuration";
        const auto AllowBitstreaming = L"AllowBitstreaming";
        const auto CrossfeedEnabled = L"CrossfeedEnabled";
        const auto CrossfeedCutoffFrequency = L"CrossfeedCutoffFrequency";
        const auto CrossfeedLevel = L"CrossfeedLevel";
    }

    OuterFilter::OuterFilter(IUnknown* pUnknown, const GUID& guid)
        : CUnknown(L"SaneAudioRenderer::OuterFilter", pUnknown)
        , m_guid(guid)
    {
    }

    OuterFilter::~OuterFilter()
    {
        BOOL boolValue;
        WCHAR* stringValue;
        UINT32 uintValue1;
        UINT32 uintValue2;

        if (SUCCEEDED(m_settings->GetOuputDevice(&stringValue, &boolValue, &uintValue1)))
        {
            std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> holder(stringValue);
            m_registryKey.SetString(DeviceId, stringValue);
            m_registryKey.SetUint(DeviceExclusive, boolValue);
            m_registryKey.SetUint(DeviceBufferDuration, uintValue1);
        }

        m_settings->GetAllowBitstreaming(&boolValue);
        m_registryKey.SetUint(AllowBitstreaming, boolValue);

        m_settings->GetCrossfeedEnabled(&boolValue);
        m_registryKey.SetUint(CrossfeedEnabled, boolValue);

        m_settings->GetCrossfeedSettings(&uintValue1, &uintValue2);
        m_registryKey.SetUint(CrossfeedCutoffFrequency, uintValue1);
        m_registryKey.SetUint(CrossfeedLevel, uintValue2);

        //Unregister Notifications
        IMMDeviceEnumeratorPtr pDeviceEnumerator;
        if (SUCCEEDED(pDeviceEnumerator.CreateInstance(__uuidof(MMDeviceEnumerator))))
        {
            pDeviceEnumerator->UnregisterEndpointNotificationCallback(m_notification);
        }		
    }

    STDMETHODIMP OuterFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        if (!m_initialized)
            ReturnIfFailed(Init());

        if (riid == IID_IUnknown)
            return CUnknown::NonDelegatingQueryInterface(riid, ppv);

        if (riid == IID_ISpecifyPropertyPages)
            return m_innerFilter->QueryInterface(__uuidof(ISpecifyPropertyPages2), ppv);

        return m_innerFilter->QueryInterface(riid, ppv);
    }

    HRESULT OuterFilter::Init()
    {
        assert(!m_initialized);

        ReturnIfFailed(Factory::CreateSettings(&m_settings))
        ReturnIfFailed(Factory::CreateFilterAggregated(GetOwner(), m_guid, m_settings, &m_innerFilter));
        ReturnIfFailed(m_registryKey.Open(HKEY_CURRENT_USER, L"Software\\sanear"));
        ReturnIfFailed(m_trayWindow.Init(m_settings));
        ReturnIfFailed(Factory::CreateNotificationClient(m_settings, &m_notification));

        //Register Notifications
        IMMDeviceEnumeratorPtr pDeviceEnumerator;
        ReturnIfFailed(pDeviceEnumerator.CreateInstance(__uuidof(MMDeviceEnumerator)));

        pDeviceEnumerator->RegisterEndpointNotificationCallback(m_notification);

        m_initialized = true;

        std::vector<wchar_t> stringValue;
        uint32_t uintValue1;
        uint32_t uintValue2;

        if (m_registryKey.GetString(DeviceId, stringValue) &&
            m_registryKey.GetUint(DeviceExclusive, uintValue1) &&
            m_registryKey.GetUint(DeviceBufferDuration, uintValue2))
        {
            m_settings->SetOuputDevice(stringValue.data(), uintValue1, uintValue2);
        }

        if (m_registryKey.GetUint(AllowBitstreaming, uintValue1))
            m_settings->SetAllowBitstreaming(uintValue1);

        if (m_registryKey.GetUint(CrossfeedEnabled, uintValue1))
            m_settings->SetCrossfeedEnabled(uintValue1);

        if (m_registryKey.GetUint(CrossfeedCutoffFrequency, uintValue1) &&
            m_registryKey.GetUint(CrossfeedLevel, uintValue2))
        {
            m_settings->SetCrossfeedSettings(uintValue1, uintValue2);
        }

        return S_OK;
    }
}
