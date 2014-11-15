#pragma once

#include <dshow.h>
#include <guiddef.h>

#include "Settings.h"

namespace SaneAudioRenderer
{
    class Factory final
    {
    public:

        Factory() = delete;

        static HRESULT CreateFilter(IBaseFilter** ppOut, ISettings* pSettings);
        static const GUID& GetFilterGuid();
    };
}
