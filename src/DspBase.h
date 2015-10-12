#pragma once

#include "DspChunk.h"
#include "Interfaces.h"

namespace SaneAudioRenderer
{
    class DspBase
    {
    public:

        virtual ~DspBase() = default;

        virtual std::wstring Name() = 0;

        virtual bool Active() = 0;

        virtual void Process(DspChunk& chunk) = 0;
        virtual void Finish(DspChunk& chunk) = 0;

    protected:

        void SetSettings(ISettings* pSettings)
        {
            assert(pSettings);

            m_settings = pSettings;
            m_settingsSerial = m_settings->GetSerial();

            SettingsUpdated();
        }

        bool CheckSettings()
        {
            assert(m_settings);

            UINT32 newSerial = m_settings->GetSerial();

            if (m_settingsSerial == newSerial)
                return false;

            m_settingsSerial = newSerial;
            SettingsUpdated();

            return true;
        }

        virtual void SettingsUpdated() {}; // Override this.

        ISettingsPtr m_settings;

    private:

        UINT32 m_settingsSerial = 0;
    };
}
