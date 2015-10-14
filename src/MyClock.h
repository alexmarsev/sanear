#pragma once

namespace SaneAudioRenderer
{
    class AudioRenderer;

    class MyClock final
        : public CBaseReferenceClock
    {
    public:

        MyClock(IUnknown* pUnknown, const std::unique_ptr<AudioRenderer>& renderer, HRESULT& result);
        MyClock(const MyClock&) = delete;
        MyClock& operator=(const MyClock&) = delete;

        DECLARE_IUNKNOWN

        STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

        REFERENCE_TIME GetPrivateTime() override;

        void SlaveClockToAudio(IAudioClock* pAudioClock, int64_t audioStart);
        void UnslaveClockFromAudio();
        void OffsetSlavedClock(REFERENCE_TIME offsetTime);
        HRESULT GetAudioClockTime(REFERENCE_TIME* pAudioTime, REFERENCE_TIME* pCounterTime);
        HRESULT GetAudioClockStartTime(REFERENCE_TIME* pStartTime);

    private:

        const std::unique_ptr<AudioRenderer>& m_renderer;

        const int64_t m_performanceFrequency;
        IAudioClockPtr m_audioClock;
        int64_t m_audioStart = 0;
        int64_t m_audioOffset = 0;
        int64_t m_counterOffset = 0;
    };
}
