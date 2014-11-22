#include "pch.h"
#include "Settings.h"

namespace SaneAudioRenderer
{
    Settings::Settings()
        : CUnknown("Audio Renderer Settings", nullptr)
    {
    }

    STDMETHODIMP Settings::NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        return (riid == __uuidof(ISettings)) ?
                   GetInterface(static_cast<ISettings*>(this), ppv) :
                   CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}
