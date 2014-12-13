#pragma once

#include "DeviceManager.h"
#include "DspBalance.h"
#include "DspCrossfeed.h"
#include "DspDither.h"
#include "DspLimiter.h"
#include "DspMatrix.h"
#include "DspRate.h"
#include "DspTempo.h"
#include "DspVolume.h"
#include "Interfaces.h"
#include "MyClock.h"

namespace SaneAudioRenderer
{
    class AudioRenderer final
        : public CCritSec
    {
    public:

        AudioRenderer(ISettings* pSettings, IMyClock* pClock, CAMEvent& bufferFilled, HRESULT& result);
        AudioRenderer(const AudioRenderer&) = delete;
        AudioRenderer& operator=(const AudioRenderer&) = delete;
        ~AudioRenderer();

        void SetClock(IReferenceClock* pClock);
        bool OnExternalClock();

        bool Enqueue(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps);
        bool Finish(bool blockUntilEnd);

        void BeginFlush();
        void EndFlush();

        bool CheckFormat(const WAVEFORMATEX& inputFormat);
        void SetFormat(const WAVEFORMATEX& inputFormat);

        void NewSegment(double rate);

        void Play(REFERENCE_TIME startTime);
        void Pause();
        void Stop();

        float GetVolume() const { return m_volume; }
        void SetVolume(float volume) { m_volume = volume; }
        float GetBalance() const { return m_balance; }
        void SetBalance(float balance) { m_balance = balance; }

        std::unique_ptr<WAVEFORMATEXTENSIBLE> GetInputFormat();
        std::unique_ptr<AudioDevice> GetDeviceFormat();
        std::vector<std::wstring> GetActiveProcessors();

    private:

        DspChunk PreProcessFirstSamples(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps);

        void StartDevice();
        void ClearDevice();

        void InitializeProcessors();

        bool Push(DspChunk& chunk);

        DeviceManager m_deviceManager;
        AudioDevice m_device;
        bool m_deviceInitialized = false;

        FILTER_STATE m_state = State_Stopped;

        IMyClockPtr m_myClock;
        IReferenceClockPtr m_graphClock;
        bool m_externalClock = false;
        REFERENCE_TIME m_correctedWithRateDsp = 0;

        WAVEFORMATEXTENSIBLE m_inputFormat;
        bool m_inputFormatInitialized = false;

        REFERENCE_TIME m_startOffset = 0;
        REFERENCE_TIME m_startTime = 0;

        REFERENCE_TIME m_receivedFramesTimeInPreviousFormats = 0;
        uint64_t m_receivedFrames = 0;
        REFERENCE_TIME m_firstSampleStart = 0;
        REFERENCE_TIME m_lastSampleEnd = 0;

        CAMEvent m_flush;

        uint64_t m_pushedFrames = 0;

        DspMatrix m_dspMatrix;
        DspRate m_dspRate;
        DspTempo m_dspTempo;
        DspCrossfeed m_dspCrossfeed;
        DspVolume m_dspVolume;
        DspBalance m_dspBalance;
        DspLimiter m_dspLimiter;
        DspDither m_dspDither;

        CAMEvent& m_bufferFilled;

        ISettingsPtr m_settings;

        std::atomic<float> m_volume = 1.0f;
        std::atomic<float> m_balance = 0.0f;
        double m_rate = 1.0;
    };
}
