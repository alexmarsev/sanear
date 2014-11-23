#pragma once

#include <comdef.h>
#include <ocidl.h>

namespace SaneAudioRenderer
{
    struct __declspec(uuid("AFA2A9DB-16FC-4B63-86CB-E38803D8BE7A"))
    ISettings : IUnknown
    {
        STDMETHOD_(BOOL, UseExclusiveMode)() = 0;
        STDMETHOD_(void, SetUseExclusiveMode)(BOOL) = 0;

        STDMETHOD_(BOOL, UseStereoCrossfeed)() = 0;
        STDMETHOD_(void, SetUseStereoCrossfeed)(BOOL) = 0;
    };
    _COM_SMARTPTR_TYPEDEF(ISettings, __uuidof(ISettings));

    struct __declspec(uuid("03481710-D73E-4674-839F-03EDE2D60ED8"))
    ISpecifyPropertyPages2 : ISpecifyPropertyPages
    {
        STDMETHOD(CreatePage)(const GUID& guid, IPropertyPage** ppPage) = 0;
    };
    _COM_SMARTPTR_TYPEDEF(ISpecifyPropertyPages2, __uuidof(ISpecifyPropertyPages2));
}
