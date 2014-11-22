#pragma once

#include "Interfaces.h"

namespace SaneAudioRenderer
{
    class Settings final
        : public CUnknown
        , public ISettings
    {
    public:

        DECLARE_IUNKNOWN

        Settings();
        Settings(const Settings&) = delete;
        Settings& operator=(const Settings&) = delete;

        STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

        STDMETHODIMP_(BOOL) UseExclusiveMode() override { return m_useExclusiveMode; }
        STDMETHODIMP_(void) SetUseExclusiveMode(BOOL value) override { m_useExclusiveMode = value; }

        STDMETHODIMP_(BOOL) UseStereoCrossfeed() override { return m_useStereoCrossfeed; }
        STDMETHODIMP_(void) SetUseStereoCrossfeed(BOOL value) override { m_useStereoCrossfeed = value; }

    private:

        std::atomic<BOOL> m_useExclusiveMode = FALSE;
        std::atomic<BOOL> m_useStereoCrossfeed = FALSE;
    };
}
