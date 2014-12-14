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

    STDMETHODIMP Settings::SetOuputDevice(LPCWSTR pDeviceName, BOOL bExclusive)
    {
        CAutoLock lock(this);

        m_exclusive = bExclusive;

        m_serial++;
        return S_OK;
    }

    STDMETHODIMP Settings::GetOuputDevice(LPWSTR* ppDeviceName, BOOL* pbExclusive)
    {
        CAutoLock lock(this);

        if (ppDeviceName)
            *ppDeviceName = nullptr;

        if (pbExclusive)
            *pbExclusive = m_exclusive;

        return S_OK;
    }

    STDMETHODIMP_(void) Settings::SetAllowBitstreaming(BOOL bAllowBitstreaming)
    {
        CAutoLock lock(this);

        m_allowBitstreaming = bAllowBitstreaming;

        m_serial++;
    }

    STDMETHODIMP_(void) Settings::GetAllowBitstreaming(BOOL* pbAllowBitstreaming)
    {
        CAutoLock lock(this);

        if (pbAllowBitstreaming)
            *pbAllowBitstreaming = m_allowBitstreaming;
    }

    STDMETHODIMP_(void) Settings::SetSharedModePeakLimiterEnabled(BOOL bEnable)
    {
        CAutoLock lock(this);

        m_sharedModePeakLimiterEnabled = bEnable;

        m_serial++;
    }

    STDMETHODIMP_(void) Settings::GetSharedModePeakLimiterEnabled(BOOL* pbEnabled)
    {
        CAutoLock lock(this);

        if (pbEnabled)
            *pbEnabled = m_sharedModePeakLimiterEnabled;
    }

    STDMETHODIMP_(void) Settings::SetCrossfeedEnabled(BOOL bEnable)
    {
        CAutoLock lock(this);

        m_crossfeedEnabled = bEnable;

        m_serial++;
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

        m_crossfeedCutoffFrequency = uCutoffFrequency;
        m_crossfeedLevel = uCrossfeedLevel;

        m_serial++;
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
