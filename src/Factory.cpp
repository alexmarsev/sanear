#include "pch.h"
#include "Factory.h"

#include "MyFilter.h"

namespace SaneAudioRenderer
{
    HRESULT Factory::CreateFilter(IBaseFilter** ppOut)
    {
        CheckPointer(ppOut, E_POINTER);

        *ppOut = nullptr;

        HRESULT result = S_OK;

        MyFilter* pRenderer = new(std::nothrow) MyFilter(result);

        if (!pRenderer)
            return E_OUTOFMEMORY;

        pRenderer->AddRef();

        if (SUCCEEDED(result))
            result = pRenderer->QueryInterface(IID_PPV_ARGS(ppOut));

        pRenderer->Release();

        return result;
    }

    const GUID& Factory::GetFilterGuid()
    {
        static const GUID guid = {0x2AE00773, 0x819A, 0x40FB, {0xA5, 0x54, 0x54, 0x82, 0x7E, 0x11, 0x63, 0x59}};
        return guid;
    }
}
