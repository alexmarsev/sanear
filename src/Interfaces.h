#pragma once

#include <comdef.h>
#include <ocidl.h>

namespace SaneAudioRenderer
{
    struct __declspec(uuid("AFA2A9DB-16FC-4B63-86CB-E38803D8BE7A"))
    ISettings : IUnknown
    {
        STDMETHOD_(UINT32, GetSerial)() = 0;

        enum
        {
            OUTPUT_DEVICE_DEFAULT_BUFFER_MS = 200,
        };
        STDMETHOD(SetOuputDevice)(LPCWSTR pDeviceName, BOOL bExclusive, UINT32 uBufferMS) = 0;
        STDMETHOD(GetOuputDevice)(LPWSTR* ppDeviceName, BOOL* pbExclusive, UINT32* puBufferMS) = 0;

        STDMETHOD_(void, SetAllowBitstreaming)(BOOL bAllowBitstreaming) = 0;
        STDMETHOD_(void, GetAllowBitstreaming)(BOOL* pbAllowBitstreaming) = 0;

        STDMETHOD_(void, SetSharedModePeakLimiterEnabled)(BOOL bEnable) = 0;
        STDMETHOD_(void, GetSharedModePeakLimiterEnabled)(BOOL* pbEnabled) = 0;

        STDMETHOD_(void, SetCrossfeedEnabled)(BOOL bEnable) = 0;
        STDMETHOD_(void, GetCrossfeedEnabled)(BOOL* pbEnabled) = 0;

        enum
        {
            CROSSFEED_CUTOFF_FREQ_MIN = 300,
            CROSSFEED_CUTOFF_FREQ_MAX = 2000,
            CROSSFEED_CUTOFF_FREQ_CMOY = 700,
            CROSSFEED_CUTOFF_FREQ_JMEIER = 650,
            CROSSFEED_LEVEL_MIN = 10,
            CROSSFEED_LEVEL_MAX = 150,
            CROSSFEED_LEVEL_CMOY = 60,
            CROSSFEED_LEVEL_JMEIER = 95,
        };
        STDMETHOD(SetCrossfeedSettings)(UINT32 uCutoffFrequency, UINT32 uCrossfeedLevel) = 0;
        STDMETHOD_(void, GetCrossfeedSettings)(UINT32* puCutoffFrequency, UINT32* puCrossfeedLevel) = 0;
    };
    _COM_SMARTPTR_TYPEDEF(ISettings, __uuidof(ISettings));

    struct __declspec(uuid("03481710-D73E-4674-839F-03EDE2D60ED8"))
    ISpecifyPropertyPages2 : ISpecifyPropertyPages
    {
        STDMETHOD(CreatePage)(const GUID& guid, IPropertyPage** ppPage) = 0;
    };
    _COM_SMARTPTR_TYPEDEF(ISpecifyPropertyPages2, __uuidof(ISpecifyPropertyPages2));
}
