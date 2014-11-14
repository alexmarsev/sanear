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
}
