#include "pch.h"
#include "MyFilter.h"

#include "AudioRenderer.h"
#include "Factory.h"
#include "MyBasicAudio.h"
#include "MyPin.h"
#include "MyPropertyPage.h"

namespace SaneAudioRenderer
{
    MyFilter::MyFilter(ISettings* pSettings, HRESULT& result)
        : CBaseFilter("Audio Renderer", nullptr, this, Factory::GetFilterGuid())
        , m_bufferFilled(TRUE/*manual reset*/)
    {
        assert(result == S_OK);

        try
        {
            if (SUCCEEDED(result))
                m_clock = new MyClock(result);

            if (SUCCEEDED(result))
                m_renderer = std::make_unique<AudioRenderer>(pSettings, m_clock, m_bufferFilled, result);

            if (SUCCEEDED(result))
                m_basicAudio = new MyBasicAudio(*m_renderer);

            if (SUCCEEDED(result))
                m_pin = std::make_unique<MyPin>(*m_renderer, this, m_bufferFilled, result);

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

        if (riid == __uuidof(ISpecifyPropertyPages2))
            return GetInterface(static_cast<ISpecifyPropertyPages2*>(this), ppv);

        if (riid == IID_ISpecifyPropertyPages)
            return GetInterface(static_cast<ISpecifyPropertyPages*>(this), ppv);

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

    STDMETHODIMP MyFilter::SetSyncSource(IReferenceClock* pClock)
    {
        CAutoLock objectLock(this);

        CBaseFilter::SetSyncSource(pClock);

        try
        {
            m_renderer->SetClock(pClock);
        }
        catch (HRESULT)
        {
            return E_UNEXPECTED;
        }

        return S_OK;
    }

    STDMETHODIMP MyFilter::GetPages(CAUUID* pPages)
    {
        CheckPointer(pPages, E_POINTER);

        pPages->cElems = 1;
        pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID));
        CheckPointer(pPages->pElems, E_OUTOFMEMORY);
        *pPages->pElems = __uuidof(MyPropertyPage);

        return S_OK;
    }

    STDMETHODIMP MyFilter::CreatePage(const GUID& guid, IPropertyPage** ppPage)
    {
        CheckPointer(ppPage, E_POINTER);

        if (guid != __uuidof(MyPropertyPage))
            return E_UNEXPECTED;

        MyPropertyPage* pPage;

        try
        {
            CAutoLock rendererLock(m_renderer.get());

            auto inputFormat = m_renderer->GetInputFormat();
            auto devicetFormat = m_renderer->GetDeviceFormat();

            pPage = new MyPropertyPage(inputFormat.get(), devicetFormat.get(),
                                       m_renderer->GetActiveProcessors(), m_renderer->OnExternalClock());
        }
        catch (std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }

        pPage->AddRef();

        HRESULT result = pPage->QueryInterface(IID_PPV_ARGS(ppPage));

        pPage->Release();

        return result;
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
