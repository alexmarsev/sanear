#pragma once

#include "DeviceManager.h"
//#include "DspCrossfeed.h"
//#include "DspGain.h"
//#include "DspLimiter.h"
#include "DspMatrix.h"
#include "DspRate.h"
#include "MyClock.h"

namespace SaneAudioRenderer
{
    class AudioRenderer final
        : CCritSec
    {
    public:

        AudioRenderer(IMyClock* pClock, CAMEvent& bufferFilled, HRESULT& result);
        AudioRenderer(const AudioRenderer&) = delete;
        AudioRenderer& operator=(const AudioRenderer&) = delete;
        ~AudioRenderer();

        bool Enqueue(IMediaSample* pSample, const AM_SAMPLE2_PROPERTIES& sampleProps);
        bool Finish(bool blockUntilEnd);

        void BeginFlush();
        void EndFlush();

        void SetFormat(const WAVEFORMATEX& inputFormat);

        void NewSegment();

        void Play(REFERENCE_TIME startTime);
        void Pause();
        void Stop();

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
        //DspGain m_dspGain;
        DspRate m_dspRate;
        //DspCrossfeed m_dspCrossfeed;
        //DspLimiter m_dspLimiter;

        CAMEvent& m_bufferFilled;
    };
}
