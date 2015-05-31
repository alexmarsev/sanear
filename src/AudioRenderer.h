#pragma once

#include "AudioDeviceManager.h"
#include "AudioDevice.h"
#include "DspBalance.h"
#include "DspCrossfeed.h"
#include "DspDither.h"
#include "DspLimiter.h"
#include "DspMatrix.h"
#include "DspRate.h"
#include "DspTempo.h"
#include "DspVariableRate.h"
#include "DspVolume.h"
#include "Interfaces.h"
#include "MyClock.h"
#include "SampleCorrection.h"

namespace SaneAudioRenderer
{
    class AudioRenderer final
        : public CCritSec
    {
    public:

        AudioRenderer(ISettings* pSettings, IMyClock* pClock, HRESULT& result);
        AudioRenderer(const AudioRenderer&) = delete;
        AudioRenderer& operator=(const AudioRenderer&) = delete;
        ~AudioRenderer();

        bool Enqueue(IMediaSample* pSample, AM_SAMPLE2_PROPERTIES& sampleProps, CAMEvent* pFilledEvent);
        bool Finish(bool blockUntilEnd, CAMEvent* pFilledEvent);

        void BeginFlush();
        void EndFlush();

        bool CheckFormat(SharedWaveFormat inputFormat);
        void SetFormat(SharedWaveFormat inputFormat, bool live);

        void NewSegment(double rate);

        void Play(REFERENCE_TIME startTime);
        void Pause();
        void Stop();

        float GetVolume() const { return m_volume; }
        void SetVolume(float volume) { m_volume = volume; }
        float GetBalance() const { return m_balance; }
        void SetBalance(float balance) { m_balance = balance; }

        SharedWaveFormat GetInputFormat();
        AudioDevice const* GetAudioDevice();
        std::vector<std::wstring> GetActiveProcessors();

    private:

        void CheckDeviceSettings();
        void StartDevice();
        void CreateDevice();
        void ClearDevice();

        void ApplyClockCorrection();

        HRESULT GetGraphTime(REFERENCE_TIME& time);

        void InitializeProcessors();

        template <typename F>
        void EnumerateProcessors(F f)
        {
            f(&m_dspMatrix);
            f(&m_dspRate);
            f(&m_dspVariableRate);
            f(&m_dspTempo);
            f(&m_dspCrossfeed);
            f(&m_dspVolume);
            f(&m_dspBalance);
            f(&m_dspLimiter);
            f(&m_dspDither);
        }

        bool Push(DspChunk& chunk, CAMEvent* pFilledEvent);

        AudioDeviceManager m_deviceManager;
        std::unique_ptr<AudioDevice> m_device;

        FILTER_STATE m_state = State_Stopped;

        SampleCorrection m_sampleCorrection;

        IMyClockPtr m_myClock;
        IReferenceClockPtr m_myGraphClock;

        SharedWaveFormat m_inputFormat;
        bool m_live = false;

        REFERENCE_TIME m_startClockOffset = 0;
        REFERENCE_TIME m_startTime = 0;

        CAMEvent m_flush;

        DspMatrix m_dspMatrix;
        DspRate m_dspRate;
        DspVariableRate m_dspVariableRate;
        DspTempo m_dspTempo;
        DspCrossfeed m_dspCrossfeed;
        DspVolume m_dspVolume;
        DspBalance m_dspBalance;
        DspLimiter m_dspLimiter;
        DspDither m_dspDither;

        ISettingsPtr m_settings;
        UINT32 m_deviceSettingsSerial = 0;

        std::atomic<float> m_volume = 1.0f;
        std::atomic<float> m_balance = 0.0f;
        double m_rate = 1.0;
    };
}
