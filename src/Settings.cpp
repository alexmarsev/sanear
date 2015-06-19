#include "pch.h"
#include "Settings.h"

namespace SaneAudioRenderer
{
    Settings::Settings()
        : CUnknown("Audio Renderer Settings", nullptr)
    {
    }

    STDMETHODIMP Settings::NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        return (riid == __uuidof(ISettings)) ?
                   GetInterface(static_cast<ISettings*>(this), ppv) :
                   CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }

    STDMETHODIMP_(UINT32) Settings::GetSerial()
    {
        return m_serial;
    }

    STDMETHODIMP Settings::SetOuputDevice(LPCWSTR pDeviceName, BOOL bExclusive, UINT32 uBufferMS)
    {
        if (uBufferMS < OUTPUT_DEVICE_BUFFER_MIN_MS || uBufferMS > OUTPUT_DEVICE_BUFFER_MAX_MS)
            return E_INVALIDARG;

        CAutoLock lock(this);

        if (m_exclusive != bExclusive ||
            m_buffer != uBufferMS ||
            (pDeviceName && wcscmp(pDeviceName, m_device.c_str())) ||
            (!pDeviceName && !m_device.empty()))
        {
            try
            {
                m_device = pDeviceName ? pDeviceName : L"";
                m_exclusive = bExclusive;
                m_buffer = uBufferMS;
                m_serial++;
            }
            catch (std::bad_alloc&)
            {
                return E_OUTOFMEMORY;
            }
        }

        return S_OK;
    }

    STDMETHODIMP Settings::GetOuputDevice(LPWSTR* ppDeviceName, BOOL* pbExclusive, UINT32* puBufferMS)
    {
        CAutoLock lock(this);

        if (pbExclusive)
            *pbExclusive = m_exclusive;

        if (ppDeviceName)
        {
            size_t size = sizeof(wchar_t) * (m_device.length() + 1);

            *ppDeviceName = static_cast<LPWSTR>(CoTaskMemAlloc(size));

            if (!*ppDeviceName)
                return E_OUTOFMEMORY;

            memcpy(*ppDeviceName, m_device.c_str(), size);
        }

        if (puBufferMS)
            *puBufferMS = m_buffer;

        return S_OK;
    }

    STDMETHODIMP_(void) Settings::SetAllowBitstreaming(BOOL bAllowBitstreaming)
    {
        CAutoLock lock(this);

        if (m_allowBitstreaming != bAllowBitstreaming)
        {
            m_allowBitstreaming = bAllowBitstreaming;
            m_serial++;
        }
    }

    STDMETHODIMP_(void) Settings::GetAllowBitstreaming(BOOL* pbAllowBitstreaming)
    {
        CAutoLock lock(this);

        if (pbAllowBitstreaming)
            *pbAllowBitstreaming = m_allowBitstreaming;
    }

    STDMETHODIMP_(void) Settings::SetCrossfeedEnabled(BOOL bEnable)
    {
        CAutoLock lock(this);

        if (m_crossfeedEnabled != bEnable)
        {
            m_crossfeedEnabled = bEnable;
            m_serial++;
        }
    }

    STDMETHODIMP_(void) Settings::GetCrossfeedEnabled(BOOL* pbEnabled)
    {
        CAutoLock lock(this);

        if (pbEnabled)
            *pbEnabled = m_crossfeedEnabled;
    }

    STDMETHODIMP Settings::SetCrossfeedSettings(UINT32 uCutoffFrequency, UINT32 uCrossfeedLevel)
    {
        if (uCutoffFrequency < CROSSFEED_CUTOFF_FREQ_MIN ||
            uCutoffFrequency > CROSSFEED_CUTOFF_FREQ_MAX ||
            uCrossfeedLevel < CROSSFEED_LEVEL_MIN ||
            uCrossfeedLevel > CROSSFEED_LEVEL_MAX)
        {
            return E_INVALIDARG;
        }

        CAutoLock lock(this);

        if (m_crossfeedCutoffFrequency != uCutoffFrequency ||
            m_crossfeedLevel != uCrossfeedLevel)
        {
            m_crossfeedCutoffFrequency = uCutoffFrequency;
            m_crossfeedLevel = uCrossfeedLevel;
            m_serial++;
        }

        return S_OK;
    }

    STDMETHODIMP_(void) Settings::GetCrossfeedSettings(UINT32* puCutoffFrequency, UINT32* puCrossfeedLevel)
    {
        CAutoLock lock(this);

        if (puCutoffFrequency)
            *puCutoffFrequency = m_crossfeedCutoffFrequency;

        if (puCrossfeedLevel)
            *puCrossfeedLevel = m_crossfeedLevel;
    }
}
