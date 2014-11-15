#include "pch.h"
#include "MyFilter.h"

#include "Factory.h"
#include "MyPin.h"

namespace SaneAudioRenderer
{
    namespace
    {
        class MyBasicAudio final
            : public CBasicAudio
        {
        public:

            MyBasicAudio(MyPin& myPin) : CBasicAudio(TEXT("Basic Audio"), nullptr), m_myPin(myPin) {}
            MyBasicAudio(const MyBasicAudio&) = delete;
            MyBasicAudio& operator=(const MyBasicAudio&) = delete;

            STDMETHODIMP put_Volume(long volume) override
            {
                if (volume < -10000 || volume > 0) return E_FAIL;
                float f = (volume == 0) ? 1.0f : pow(10.0f, (float)volume / 2000);
                m_myPin.SetVolume(f);
                return S_OK;
            }

            STDMETHODIMP get_Volume(long* pVolume) override
            {
                CheckPointer(pVolume, E_POINTER);
                float f = m_myPin.GetVolume();
                *pVolume = (f == 1.0f) ? 0 : (long)(log10(f) * 2000);
                assert(*pVolume <= 0 && *pVolume >= -10000);
                return S_OK;
            }

            STDMETHODIMP put_Balance(long) override
            {
                return E_NOTIMPL;
            }

            STDMETHODIMP get_Balance(long* pBalance) override
            {
                CheckPointer(pBalance, E_POINTER);
                *pBalance = 0;
                return S_OK;
            }

        private:

            MyPin& m_myPin;
        };
    }

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
                m_basicAudio = new MyBasicAudio(*m_pin);

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
            return m_basicAudio->QueryInterface(riid, ppv);

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
