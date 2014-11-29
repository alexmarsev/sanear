#include "pch.h"
#include "MyClock.h"

namespace SaneAudioRenderer
{
    MyClock::MyClock(HRESULT& result)
        : CBaseReferenceClock(TEXT("Audio Renderer Clock"), nullptr, &result)
    {
    }

    STDMETHODIMP MyClock::NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        if (riid == __uuidof(IMyClock))
            return GetInterface(static_cast<IMyClock*>(this), ppv);

        return CBaseReferenceClock::NonDelegatingQueryInterface(riid, ppv);
    }

    REFERENCE_TIME MyClock::GetPrivateTime()
    {
        CAutoLock lock(this);

        static const int64_t performanceFrequency = GetPerformanceFrequency();

        if (m_audioClock)
        {
            uint64_t audioFrequency, audioPosition, audioTime;
            if (SUCCEEDED(m_audioClock->GetFrequency(&audioFrequency)) &&
                SUCCEEDED(m_audioClock->GetPosition(&audioPosition, &audioTime)))
            {
                int64_t counterTime = llMulDiv(GetPerformanceCounter(), OneSecond, performanceFrequency, 0);
                int64_t clockTime = llMulDiv(audioPosition, OneSecond, audioFrequency, 0) +
                                    m_audioStart + (audioPosition > 0 ? counterTime - audioTime : 0);
                m_counterOffset = clockTime - counterTime;
                return clockTime;
            }
        }

        return m_counterOffset + llMulDiv(GetPerformanceCounter(), OneSecond, performanceFrequency, 0);
    }

    STDMETHODIMP_(void) MyClock::SlaveClockToAudio(IAudioClock* pAudioClock, int64_t audioStart)
    {
        CAutoLock lock(this);
        m_audioClock = pAudioClock;
        m_audioStart = audioStart;
    }

    STDMETHODIMP_(void) MyClock::UnslaveClockFromAudio()
    {
        CAutoLock lock(this);
        m_audioClock = nullptr;
    }

    STDMETHODIMP_(REFERENCE_TIME) MyClock::GetTime()
    {
        return GetPrivateTime();
    }
}
