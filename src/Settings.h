#pragma once

#include "Interfaces.h"

namespace SaneAudioRenderer
{
    class Settings final
        : public CUnknown
        , public ISettings
        , private CCritSec
    {
    public:

        DECLARE_IUNKNOWN

        Settings();
        Settings(const Settings&) = delete;
        Settings& operator=(const Settings&) = delete;

        STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

        STDMETHODIMP_(UINT32) GetSerial() override;

        STDMETHODIMP SetOuputDevice(LPCWSTR pDeviceName, BOOL bExclusive) override;
        STDMETHODIMP GetOuputDevice(LPWSTR* ppDeviceName, BOOL* pbExclusive) override;

        STDMETHODIMP_(void) SetAllowBitstreaming(BOOL bAllowBitstreaming) override;
        STDMETHODIMP_(void) GetAllowBitstreaming(BOOL* pbAllowBitstreaming) override;

        STDMETHODIMP_(void) SetSharedModePeakLimiterEnabled(BOOL bEnable) override;
        STDMETHODIMP_(void) GetSharedModePeakLimiterEnabled(BOOL* pbEnabled) override;

        STDMETHODIMP_(void) SetCrossfeedEnabled(BOOL bEnable) override;
        STDMETHODIMP_(void) GetCrossfeedEnabled(BOOL* pbEnabled) override;

        STDMETHODIMP SetCrossfeedSettings(UINT32 uCutoffFrequency, UINT32 uCrossfeedLevel) override;
        STDMETHODIMP_(void) GetCrossfeedSettings(UINT32* puCutoffFrequency, UINT32* puCrossfeedLevel) override;

    private:

        std::atomic<UINT32> m_serial = 0;

        std::wstring m_device;
        BOOL m_exclusive = FALSE;

        BOOL m_allowBitstreaming = TRUE;

        BOOL m_sharedModePeakLimiterEnabled = FALSE;

        BOOL m_crossfeedEnabled = FALSE;
        UINT32 m_crossfeedCutoffFrequency = CROSSFEED_CUTOFF_FREQ_CMOY;
        UINT32 m_crossfeedLevel = CROSSFEED_LEVEL_CMOY;
    };
}
