#pragma once

#include "DeviceManager.h"
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
        : CCritSec
    {
    public:

        AudioRenderer(ISettings* pSettings, IMyClock* pClock, CAMEvent& bufferFilled, HRESULT& result);
        AudioRenderer(const AudioRenderer&) = delete;
        AudioRenderer& operator=(const AudioRenderer&) = delete;
        ~AudioRenderer();

        bool Enqueue(IMediaSample* pSample, const AM_SAMPLE2_PROPERTIES& sampleProps);
        bool Finish(bool blockUntilEnd);

        void BeginFlush();
        void EndFlush();

        void SetFormat(const WAVEFORMATEX& inputFormat);

        void NewSegment(double rate);

        void Play(REFERENCE_TIME startTime);
        void Pause();
        void Stop();

        float GetVolume() const { return m_volume; }
        void SetVolume(float volume) { m_volume = volume; }

    private:

        void InitializeProcessors();

        bool Push(DspChunk& chunk);

        DeviceManager m_deviceManager;
        AudioDevice m_device;

        FILTER_STATE m_state = State_Stopped;

        IMyClockPtr m_graphClock;

        WAVEFORMATEXTENSIBLE m_inputFormat;
        bool m_inputFormatInitialized = false;

        REFERENCE_TIME m_lastSampleEnd = 0;

        CAMEvent m_flush;

        uint64_t m_pushedFrames = 0;

        DspMatrix m_dspMatrix;
        DspRate m_dspRate;
        DspTempo m_dspTempo;
        DspCrossfeed m_dspCrossfeed;
        DspVolume m_dspVolume;
        DspLimiter m_dspLimiter;
        DspDither m_dspDither;

        CAMEvent& m_bufferFilled;

        ISettingsPtr m_settings;

        std::atomic<float> m_volume = 1.0f;
        double m_rate = 1.0;
    };
}
