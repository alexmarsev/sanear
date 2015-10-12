#pragma once

#include "DspBase.h"

namespace SaneAudioRenderer
{
    class AudioRenderer;

    class DspBalance final
        : public DspBase
    {
    public:

        DspBalance(AudioRenderer& renderer) : m_renderer(renderer) {}
        DspBalance(const DspBalance&) = delete;
        DspBalance& operator=(const DspBalance&) = delete;

        void Initialize(ISettings* pSettings);

        bool Active() override;

        std::wstring Name() override { return L"Balance"; }

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

    protected:

        void SettingsUpdated() override;

    private:

        const AudioRenderer& m_renderer;
        bool m_extraPrecision = false;
    };
}
