#pragma once

#include "DspChunk.h"
#include "Interfaces.h"

#include <bs2bclass.h>

namespace SaneAudioRenderer
{
    class DspCrossfeed final
    {
    public:

        DspCrossfeed() = default;
        DspCrossfeed(const DspCrossfeed&) = delete;
        DspCrossfeed& operator=(const DspCrossfeed&) = delete;

        std::wstring Name() { return L"Crossfeed"; }

        void Initialize(ISettings* pSettings, uint32_t rate, uint32_t channels, DWORD mask);
        bool Active();

        void Process(DspChunk& chunk);
        void Finish(DspChunk& chunk);

    private:

        void UpdateSettings();

        bs2b_base m_bs2b;

        ISettingsPtr m_settings;
        UINT32 m_settingsSerial = 0;

        bool m_possible = false;
        bool m_active = false;
    };
}
