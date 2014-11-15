#pragma once

#include "MyClock.h"

namespace SaneAudioRenderer
{
    struct ISettings;
    class MyPin;

    class MyFilter final
        : public CCritSec
        , public CBaseFilter
    {
    public:

        explicit MyFilter(ISettings* pSettings, HRESULT& result);
        MyFilter(const MyFilter&) = delete;
        MyFilter& operator=(const MyFilter&) = delete;

        DECLARE_IUNKNOWN

        STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

        int GetPinCount() override;
        CBasePin* GetPin(int n) override;

        STDMETHODIMP Stop() override;
        STDMETHODIMP Pause() override;
        STDMETHODIMP Run(REFERENCE_TIME startTime) override;

        STDMETHODIMP GetState(DWORD timeoutMilliseconds, FILTER_STATE* pState) override;

    private:

        template <FILTER_STATE NewState, typename PinFunction>
        STDMETHODIMP ChangeState(PinFunction pinFunction);

        IMyClockPtr m_clock;
        IBasicAudioPtr m_basicAudio;
        std::unique_ptr<MyPin> m_pin;
        IUnknownPtr m_seeking;
    };
}
