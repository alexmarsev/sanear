#include "pch.h"
#include "MMNotificationClient.h"

namespace SaneAudioRenderer
{
    HRESULT CMMNotificationClient::QueryInterface(const IID& riid, void** ppvInterface)
    {
        if (IID_IUnknown == riid)
        {
            AddRef();
            *ppvInterface = static_cast<IUnknown*>(this);
        }
        else if (__uuidof(IMMNotificationClient) == riid)
        {
            AddRef();
            *ppvInterface = static_cast<IMMNotificationClient*>(this);
        }
        else
        {
            *ppvInterface = nullptr;
            return E_NOINTERFACE;
        }
        return S_OK;
    }

    ULONG CMMNotificationClient::AddRef()
    {
        return InterlockedIncrement(&m_cRef);
    }

    ULONG CMMNotificationClient::Release()
    {
        ULONG ulRef = InterlockedDecrement(&m_cRef);
        if (0 == ulRef)
        {
            delete this;
        }
        return ulRef;
    }

    HRESULT CMMNotificationClient::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
    {
        return S_OK;
    }

    HRESULT CMMNotificationClient::OnDeviceAdded(LPCWSTR pwstrDeviceId)
    {
        return S_OK;
    }

    HRESULT CMMNotificationClient::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
    {
        return S_OK;
    }

    HRESULT CMMNotificationClient::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
    {
        //Not interested in Recording devices
        if (flow == eCapture)
            return S_FALSE;
        //Not interested in Communications devices, only Multimedia and Console
        if (role == ::eCommunications)
            return S_FALSE;

        LPWSTR pDeviceId = nullptr;
        BOOL exclusive;
        UINT32 buffer;
        m_settings->GetOuputDevice(&pDeviceId, &exclusive, &buffer);
        std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> holder(pDeviceId);

        if (!pDeviceId || pwstrDefaultDeviceId != pDeviceId)
        {
            m_settings->SetOuputDevice(pwstrDefaultDeviceId, exclusive, buffer);
            return S_OK;
        }
        return S_FALSE;
    }

    HRESULT CMMNotificationClient::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
    {
        return S_OK;
    }

    HRESULT CMMNotificationClient::Init(ISettings* pSettings)
    {
        CheckPointer(pSettings, E_POINTER);
        ReturnIfFailed(pSettings->QueryInterface(IID_PPV_ARGS(&m_settings)));
        return S_OK;
    }

    CMMNotificationClient::CMMNotificationClient() : m_cRef(0)
    {
    }


    CMMNotificationClient::~CMMNotificationClient()
    {
    }
}
