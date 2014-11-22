#include "pch.h"
#include "Factory.h"

#include "MyFilter.h"
#include "Settings.h"

namespace SaneAudioRenderer
{
    HRESULT Factory::CreateSettings(ISettings** ppOut)
    {
        CheckPointer(ppOut, E_POINTER);

        *ppOut = nullptr;

        auto pSettings = new(std::nothrow) Settings();

        if (!pSettings)
            return E_OUTOFMEMORY;

        pSettings->AddRef();

        HRESULT result = pSettings->QueryInterface(IID_PPV_ARGS(ppOut));

        pSettings->Release();

        return result;
    }

    HRESULT Factory::CreateFilter(ISettings* pSettings, IBaseFilter** ppOut)
    {
        CheckPointer(ppOut, E_POINTER);
        CheckPointer(pSettings, E_POINTER);

        *ppOut = nullptr;

        HRESULT result = S_OK;

        auto pFilter = new(std::nothrow) MyFilter(pSettings, result);

        if (!pFilter)
            return E_OUTOFMEMORY;

        pFilter->AddRef();

        if (SUCCEEDED(result))
            result = pFilter->QueryInterface(IID_PPV_ARGS(ppOut));

        pFilter->Release();

        return result;
    }

    const GUID& Factory::GetFilterGuid()
    {
        static const GUID guid = {0x2AE00773, 0x819A, 0x40FB, {0xA5, 0x54, 0x54, 0x82, 0x7E, 0x11, 0x63, 0x59}};
        return guid;
    }
}
