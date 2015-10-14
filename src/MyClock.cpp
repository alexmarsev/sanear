#include "pch.h"
#include "MyClock.h"

#include "AudioRenderer.h"

namespace SaneAudioRenderer
{
    MyClock::MyClock(IUnknown* pUnknown, const std::unique_ptr<AudioRenderer>& renderer, HRESULT& result)
        : CBaseReferenceClock(L"SaneAudioRenderer::MyClock", pUnknown, &result)
        , m_renderer(renderer)
        , m_performanceFrequency(GetPerformanceFrequency())
    {
    }

    STDMETHODIMP MyClock::NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        return CBaseReferenceClock::NonDelegatingQueryInterface(riid, ppv);
    }

    REFERENCE_TIME MyClock::GetPrivateTime()
    {
        CAutoLock lock(this);

        REFERENCE_TIME audioClockTime, audioClockCounterTime;
        if (SUCCEEDED(GetAudioClockTime(&audioClockTime, &audioClockCounterTime)))
        {
            m_counterOffset = audioClockTime - audioClockCounterTime;
            return audioClockTime;
        }

        return m_counterOffset + llMulDiv(GetPerformanceCounter(), OneSecond, m_performanceFrequency, 0);
    }

    void MyClock::SlaveClockToAudio(IAudioClock* pAudioClock, int64_t audioStart)
    {
        CAutoLock lock(this);

        m_audioClock = pAudioClock;
        m_audioStart = audioStart;
        m_audioOffset = 0;
    }

    void MyClock::UnslaveClockFromAudio()
    {
        CAutoLock lock(this);

        m_audioClock = nullptr;
    }

    void MyClock::OffsetSlavedClock(REFERENCE_TIME offsetTime)
    {
        CAutoLock lock(this);

        m_audioOffset += offsetTime;
    }

    HRESULT MyClock::GetAudioClockTime(REFERENCE_TIME* pAudioTime, REFERENCE_TIME* pCounterTime)
    {
        CheckPointer(pAudioTime, E_POINTER);

        CAutoLock lock(this);

        if (m_audioClock)
        {
            uint64_t audioFrequency, audioPosition, audioTime;
            if (SUCCEEDED(m_audioClock->GetFrequency(&audioFrequency)) &&
                SUCCEEDED(m_audioClock->GetPosition(&audioPosition, &audioTime)))
            {
                int64_t counterTime = llMulDiv(GetPerformanceCounter(), OneSecond, m_performanceFrequency, 0);
                int64_t clockTime = llMulDiv(audioPosition, OneSecond, audioFrequency, 0) +
                                    m_audioStart + (audioPosition > 0 ? m_audioOffset + counterTime - audioTime : 0);

                *pAudioTime = clockTime;

                if (pCounterTime)
                    *pCounterTime = counterTime;

                return S_OK;
            }
        }

        return E_FAIL;
    }

    HRESULT MyClock::GetAudioClockStartTime(REFERENCE_TIME* pStartTime)
    {
        CheckPointer(pStartTime, E_POINTER);

        CAutoLock lock(this);

        if (m_audioClock)
        {
            *pStartTime = m_audioStart;
            return S_OK;
        }

        return E_FAIL;
    }
}
