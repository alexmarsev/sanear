#include "pch.h"
#include "MyFilter.h"

#include "Factory.h"
#include "MyBasicAudio.h"
#include "MyPin.h"

namespace SaneAudioRenderer
{
    MyFilter::MyFilter(ISettings* pSettings, HRESULT& result)
        : CBaseFilter("Audio Renderer", nullptr, this, Factory::GetFilterGuid())
    {
        assert(result == S_OK);

        try
        {
            if (SUCCEEDED(result))
                m_clock = new MyClock(result);

            if (SUCCEEDED(result))
                m_pin = std::make_unique<MyPin>(this, pSettings, m_clock, result);

            if (SUCCEEDED(result))
                result = CreatePosPassThru(nullptr, FALSE, m_pin.get(), &m_seeking);
        }
        catch (std::bad_alloc&)
        {
            result = E_OUTOFMEMORY;
        }
    }

    STDMETHODIMP MyFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        if (riid == IID_IReferenceClock || riid == IID_IReferenceClockTimerControl)
            return m_clock->QueryInterface(riid, ppv);

        if (riid == IID_IBasicAudio)
            return m_pin->QueryInterface(riid, ppv);

        if (riid == IID_IMediaSeeking)
            return m_seeking->QueryInterface(riid, ppv);

        return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
    }

    int MyFilter::GetPinCount()
    {
        return 1;
    }

    CBasePin* MyFilter::GetPin(int n)
    {
        return (n == 0) ? m_pin.get() : nullptr;
    }

    STDMETHODIMP MyFilter::Stop()
    {
        return ChangeState<State_Stopped>(std::bind(&MyPin::Inactive, m_pin.get()));
    }

    STDMETHODIMP MyFilter::Pause()
    {
        return ChangeState<State_Paused>(std::bind(&MyPin::Active, m_pin.get()));
    }

    STDMETHODIMP MyFilter::Run(REFERENCE_TIME startTime)
    {
        return ChangeState<State_Running>(std::bind(&MyPin::Run, m_pin.get(), startTime));
    }

    STDMETHODIMP MyFilter::GetState(DWORD timeoutMilliseconds, FILTER_STATE* pState)
    {
        CheckPointer(pState, E_POINTER);

        CAutoLock objectLock(this);

        *pState = m_State;

        if (!m_pin->StateTransitionFinished(timeoutMilliseconds))
            return VFW_S_STATE_INTERMEDIATE;

        return S_OK;
    }

    template <FILTER_STATE NewState, typename PinFunction>
    STDMETHODIMP MyFilter::ChangeState(PinFunction pinFunction)
    {
        CAutoLock objectLock(this);

        if (m_State != NewState)
        {
            ReturnIfFailed(pinFunction());
            m_State = NewState;
        }

        if (!m_pin->StateTransitionFinished(0))
            return S_FALSE;

        return S_OK;
    }
}
