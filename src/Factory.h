#pragma once

namespace SaneAudioRenderer
{
    class Factory final
    {
    public:

        Factory() = delete;

        static HRESULT CreateFilter(IBaseFilter** ppOut);
    };
}
