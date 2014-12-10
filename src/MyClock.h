#pragma once

namespace SaneAudioRenderer
{
    // This is an internal interface, and can be changed anytime.
    struct __declspec(uuid("B6E42D80-6CAB-4D9B-91F4-F6FE5C4C64E6"))
    IMyClock : IUnknown
    {
        STDMETHOD_(void, SlaveClockToAudio)(IAudioClock* pAudioClock, int64_t audioStart) = 0;
        STDMETHOD_(void, UnslaveClockFromAudio)() = 0;
        STDMETHOD(GetAudioClockTime)(REFERENCE_TIME* pAudioTime, REFERENCE_TIME* pCounterTime) = 0;
        STDMETHOD(GetAudioClockStartTime)(REFERENCE_TIME* pStartTime) = 0;
    };
    _COM_SMARTPTR_TYPEDEF(IMyClock, __uuidof(IMyClock));

    class MyClock final
        : public CBaseReferenceClock
        , public IMyClock
    {
    public:

        MyClock(HRESULT& result);
        MyClock(const MyClock&) = delete;
        MyClock& operator=(const MyClock&) = delete;

        DECLARE_IUNKNOWN

        STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

        REFERENCE_TIME GetPrivateTime() override;

        STDMETHODIMP_(void) SlaveClockToAudio(IAudioClock* pAudioClock, int64_t audioStart) override;
        STDMETHODIMP_(void) UnslaveClockFromAudio() override;
        STDMETHODIMP GetAudioClockTime(REFERENCE_TIME* pAudioTime, REFERENCE_TIME* pCounterTime) override;
        STDMETHODIMP GetAudioClockStartTime(REFERENCE_TIME* pStartTime) override;

    private:

        const int64_t m_performanceFrequency;
        IAudioClockPtr m_audioClock;
        int64_t m_audioStart = 0;
        int64_t m_counterOffset = 0;
    };
}
