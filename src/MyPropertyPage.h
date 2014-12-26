#pragma once

namespace SaneAudioRenderer
{
    struct AudioDevice;

    class _declspec(uuid("7EEEDEC8-8B8E-4220-AF12-08BC0CE844F0"))
    MyPropertyPage final
        : public CUnknown
        , public IPropertyPage
    {
    public:

        MyPropertyPage(SharedWaveFormat inputFormat, const AudioDevice* pDeviceFormat,
                       std::vector<std::wstring> processors, bool externalClock);
        MyPropertyPage(const MyPropertyPage&) = delete;
        MyPropertyPage& operator=(const MyPropertyPage&) = delete;

        DECLARE_IUNKNOWN

        STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

        STDMETHODIMP SetPageSite(IPropertyPageSite* pPageSite) override;
        STDMETHODIMP Activate(HWND hParent, LPCRECT pRect, BOOL bModal) override;
        STDMETHODIMP Deactivate() override;
        STDMETHODIMP GetPageInfo(PROPPAGEINFO* pPageInfo) override;
        STDMETHODIMP SetObjects(ULONG, IUnknown**) override { return S_OK; }
        STDMETHODIMP Show(UINT cmdShow) override;
        STDMETHODIMP Move(LPCRECT pRect) override;
        STDMETHODIMP IsPageDirty() override { return S_FALSE; }
        STDMETHODIMP Apply() override { return S_OK; }
        STDMETHODIMP Help(LPCOLESTR) override { return E_NOTIMPL; }
        STDMETHODIMP TranslateAccelerator(MSG*) override { return E_NOTIMPL; }

    private:

        std::vector<char> m_dialogData;
        IPropertyPageSitePtr m_pageSite;
        HWND m_hWindow = NULL;
    };
}
