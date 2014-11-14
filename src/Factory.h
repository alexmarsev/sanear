#pragma once

#include <guiddef.h>

namespace SaneAudioRenderer
{
    class Factory final
    {
    public:

        Factory() = delete;

        static HRESULT CreateFilter(IBaseFilter** ppOut);
        static const GUID& GetFilterGuid();
    };
}
