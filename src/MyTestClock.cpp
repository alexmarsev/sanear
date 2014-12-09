#include "pch.h"
#include "MyTestClock.h"

namespace SaneAudioRenderer
{
    MyTestClock::MyTestClock(HRESULT& result)
        : CBaseReferenceClock(TEXT("Audio Renderer Test Clock"), nullptr, &result)
        , m_performanceFrequency(GetPerformanceFrequency())
    {
    }

    REFERENCE_TIME MyTestClock::GetPrivateTime()
    {
        return llMulDiv(GetPerformanceCounter(), OneSecond, m_performanceFrequency, 0);
    }
}
