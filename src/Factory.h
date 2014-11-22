#pragma once

#include <dshow.h>
#include <guiddef.h>

#include "Interfaces.h"

namespace SaneAudioRenderer
{
    class Factory final
    {
    public:

        Factory() = delete;

        static HRESULT CreateSettings(ISettings** ppOut);

        static HRESULT CreateFilter(ISettings* pSettings, IBaseFilter** ppOut);
        static const GUID& GetFilterGuid();
    };
}
