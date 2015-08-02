#include "pch.h"

#include "OuterFilter.h"

namespace
{
    // {DF557071-C9FD-433A-9627-81E0D3640ED9}
    const GUID filterGuid = {0xdf557071, 0xc9fd, 0x433a, {0x96, 0x27, 0x81, 0xe0, 0xd3, 0x64, 0xe, 0xd9}};

    const WCHAR filterName[] = L"sanear";

    const AMOVIESETUP_MEDIATYPE pinTypes[] = {
        {&MEDIATYPE_Audio, &CLSID_NULL},
    };

    const AMOVIESETUP_PIN setupPin = {
        L"", TRUE, FALSE, FALSE, FALSE, &CLSID_NULL, nullptr, _countof(pinTypes), pinTypes,
    };

    const AMOVIESETUP_FILTER setupFilter = {
        &filterGuid, filterName, MERIT_DO_NOT_USE, 1, &setupPin
    };
}

CUnknown* WINAPI CreateFilterInstance(LPUNKNOWN, HRESULT*);

CFactoryTemplate g_Templates[] = {
    {filterName, &filterGuid, CreateFilterInstance},
};

int g_cTemplates = _countof(g_Templates);


STDAPI RegisterAllServers(LPCWSTR szFileName, BOOL bRegister);

namespace
{
    struct CoFreeUnusedLibrariesHelper
    {
        ~CoFreeUnusedLibrariesHelper() { CoFreeUnusedLibraries(); };
    };

    HRESULT DllRegisterServer(bool reg)
    {
        wchar_t filename[MAX_PATH];
        if (!GetModuleFileName(g_hInst, filename, MAX_PATH))
            return AmGetLastErrorToHResult();

        if (reg)
            ReturnIfFailed(RegisterAllServers(filename, TRUE));

        {
            SaneAudioRenderer::CoInitializeHelper coInitializeHelper(COINIT_APARTMENTTHREADED);
            CoFreeUnusedLibrariesHelper coFreeUnusedLibrariesHelper;

            IFilterMapper2Ptr filterMapper;
            ReturnIfFailed(CoCreateInstance(CLSID_FilterMapper2, nullptr,
                                            CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&filterMapper)));
            {
                HRESULT result;

                result = filterMapper->UnregisterFilter(nullptr, nullptr, *setupFilter.clsID);

                if (FAILED(result))
                    ReturnIfNotEquals(result, 0x80070002);

                result = filterMapper->UnregisterFilter(&CLSID_AudioRendererCategory, nullptr, *setupFilter.clsID);

                if (FAILED(result))
                    ReturnIfNotEquals(result, 0x80070002);
            }

            if (reg)
            {
                const REGFILTER2 rf = {
                    1,
                    setupFilter.dwMerit,
                    setupFilter.nPins,
                    setupFilter.lpPin,
                };

                ReturnIfFailed(filterMapper->RegisterFilter(*setupFilter.clsID, setupFilter.strName,
                                                            nullptr, &CLSID_AudioRendererCategory, nullptr, &rf));

                ReturnIfFailed(filterMapper->RegisterFilter(*setupFilter.clsID, setupFilter.strName,
                                                            nullptr, nullptr, nullptr, &rf));
            }
        }

        if (!reg)
            ReturnIfFailed(RegisterAllServers(filename, FALSE));

        return S_OK;
    }
}

CUnknown* WINAPI CreateFilterInstance(IUnknown* pUnknown, HRESULT* pResult)
{
    CheckPointer(pResult, nullptr);

    auto pFilter = new(std::nothrow) SaneAudioRenderer::OuterFilter(pUnknown, filterGuid);

    if (!pFilter)
        *pResult = E_OUTOFMEMORY;

    return pFilter;
}

STDAPI DllRegisterServer()
{
    if (!IsWindowsVistaOrGreater())
        return E_FAIL;

    return DllRegisterServer(true);
}

STDAPI DllUnregisterServer()
{
    return DllRegisterServer(false);
}

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL WINAPI DllMain(HINSTANCE hDllHandle, DWORD dwReason, LPVOID pReserved)
{
	return DllEntryPoint(hDllHandle, dwReason, pReserved);
}
