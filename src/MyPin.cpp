#include "pch.h"
#include "MyPin.h"

namespace SaneAudioRenderer
{
    MyPin::MyPin(CBaseFilter* pFilter, IMyClock* pClock, HRESULT& result)
        : CBaseInputPin("Audio Renderer Input Pin", pFilter, this, &result, TEXT("Input0"))
        , m_bufferFilled(TRUE/*manual reset*/)
        , m_renderer(pClock, m_bufferFilled, result)
    {
        if (FAILED(result))
            return;

        if (static_cast<HANDLE>(m_bufferFilled) == NULL)
        {
            result = E_OUTOFMEMORY;
        }
    }

    HRESULT MyPin::CheckMediaType(const CMediaType* pmt)
    {
        CheckPointer(pmt, E_POINTER);

        if (*pmt->Type() == MEDIATYPE_Audio &&
            *pmt->FormatType() == FORMAT_WaveFormatEx)
        {
            WAVEFORMATEX* pFormat = (WAVEFORMATEX*)pmt->Format();

            if (*pmt->Subtype() == MEDIASUBTYPE_IEEE_FLOAT)
            {
                switch (pFormat->wBitsPerSample)
                {
                    case 32:
                    case 64:
                        return S_OK;
                }
            }

            if (*pmt->Subtype() == MEDIASUBTYPE_PCM)
            {
                switch (pFormat->wBitsPerSample)
                {
                    case 8:
                    case 16:
                    case 24:
                    case 32:
                        return S_OK;
                }
            }
        }

        return S_FALSE;
    }

    HRESULT MyPin::SetMediaType(const CMediaType* pmt)
    {
        ReturnIfFailed(CBaseInputPin::SetMediaType(pmt));

        WAVEFORMATEX* pFormat = (WAVEFORMATEX*)pmt->Format();
        m_renderer.SetFormat(*pFormat);

        return S_OK;
    }

    STDMETHODIMP MyPin::NewSegment(REFERENCE_TIME startTime, REFERENCE_TIME stopTime, double rate)
    {
        ReturnIfFailed(CBaseInputPin::NewSegment(startTime, stopTime, rate));

        m_renderer.NewSegment();

        return S_OK;
    }

    STDMETHODIMP MyPin::Receive(IMediaSample* pSample)
    {
        CAutoLock receiveLock(&m_receiveMutex);

        {
            CAutoLock objectLock(this);

            if (m_state == State_Stopped)
                return VFW_E_WRONG_STATE;

            ReturnIfNotEquals(S_OK, CBaseInputPin::Receive(pSample));
        }

        if (m_eosUp)
            return S_FALSE;

        if (m_SampleProps.dwSampleFlags & AM_SAMPLE_TYPECHANGED)
        {
            m_renderer.Finish(false);
            ReturnIfFailed(SetMediaType(static_cast<CMediaType*>(m_SampleProps.pMediaType)));
        }

        return m_renderer.Enqueue(pSample, m_SampleProps) ? S_OK : S_FALSE;
    }

    STDMETHODIMP MyPin::EndOfStream()
    {
        CAutoLock receiveLock(&m_receiveMutex);

        {
            CAutoLock objectLock(this);

            if (m_state == State_Stopped)
                return VFW_E_WRONG_STATE;

            if (m_bFlushing)
                return S_FALSE;
        }

        m_eosUp = true;
        // We ask audio renderer to block until all samples are played.
        // Returns 'false' in case of interruption.
        m_eosDown = m_renderer.Finish(true);

        if (m_eosDown)
        {
            m_pFilter->NotifyEvent(EC_COMPLETE, S_OK, (LONG_PTR)m_pFilter);
            m_bufferFilled.Set();
        }

        return S_OK;
    }

    STDMETHODIMP MyPin::BeginFlush()
    {
        // Parent method locks the object before modifying it, all is good.
        CBaseInputPin::BeginFlush();

        m_renderer.BeginFlush();
        // Barrier for any present Receive() and EndOfStream() calls.
        // Subsequent ones will be rejected because m_bFlushing == TRUE.
        CAutoLock receiveLock(&m_receiveMutex);
        m_eosUp = false;
        m_eosDown = false;
        m_renderer.EndFlush();

        return S_OK;
    }

    STDMETHODIMP MyPin::EndFlush()
    {
        CAutoLock objectLock(this);

        CBaseInputPin::EndFlush();

        return S_OK;
    }

    HRESULT MyPin::Active()
    {
        CAutoLock objectLock(this);

        assert(m_state != State_Paused);
        m_state = State_Paused;

        if (IsConnected())
        {
            m_renderer.Pause();
        }
        else
        {
            m_eosUp = true;
            m_eosDown = true;
        }

        return S_OK;
    }

    HRESULT MyPin::Run(REFERENCE_TIME startTime)
    {
        CAutoLock objectLock(this);

        assert(m_state == State_Paused);
        m_state = State_Running;

        if (m_eosDown)
        {
            m_pFilter->NotifyEvent(EC_COMPLETE, S_OK, (LONG_PTR)m_pFilter);
        }
        else if (IsConnected())
        {
            m_renderer.Play(startTime);
        }

        return S_OK;
    }

    HRESULT MyPin::Inactive()
    {
        {
            CAutoLock objectLock(this);

            assert(m_state != State_Stopped);
            m_state = State_Stopped;

            CBaseInputPin::Inactive();
        }

        m_renderer.BeginFlush();
        // Barrier for any present Receive() and EndOfStream() calls.
        // Subsequent ones will be rejected because m_state == State_Stopped.
        CAutoLock receiveLock(&m_receiveMutex);
        m_eosUp = false;
        m_eosDown = false;
        m_renderer.Stop();
        m_renderer.EndFlush();

        return S_OK;
    }

    bool MyPin::StateTransitionFinished(uint32_t timeoutMilliseconds)
    {
        CAutoLock objectLock(this);

        if (!IsConnected() || m_state == State_Stopped)
            return true;

        return !!m_bufferFilled.Wait(timeoutMilliseconds);
    }
}
