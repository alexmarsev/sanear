#include "pch.h"
#include "MyPropertyPage.h"

#include "AudioDevice.h"
#include "DspMatrix.h"

namespace SaneAudioRenderer
{
    namespace
    {
        void Write(std::vector<char>& out, void* src, size_t size)
        {
            assert(src);
            assert(size > 0);
            out.insert(out.end(), size, 0);
            memcpy(&out[out.size() - size], src, size);
        }

        template <typename T>
        void Write(std::vector<char>& out, T t)
        {
            Write(out, &t, sizeof(T));
        }

        void WriteString(std::vector<char>& out, const std::wstring& str)
        {
            Write(out, (void*)str.c_str(), sizeof(wchar_t) * (str.length() + 1));
        }

        void WriteDialogHeader(std::vector<char>& out, const std::wstring& font, WORD fontSize, short w, short h)
        {
            assert(out.empty());

            Write<DWORD>(out, DS_SETFONT | DS_FIXEDSYS | WS_CHILD);
            Write<DWORD>(out, 0);
            Write<WORD>(out, 0);
            Write<short>(out, 0);
            Write<short>(out, 0);
            Write<short>(out, w);
            Write<short>(out, h);
            Write<WORD>(out, 0);
            Write<WORD>(out, 0);
            WriteString(out, L"");
            Write<WORD>(out, fontSize);
            WriteString(out, font);
        }

        void WriteDialogItem(std::vector<char>& out, DWORD style, DWORD control, short x, short y, short w, short h,
                             const std::wstring& text)
        {
            assert(!out.empty());

            if (out.size() % 4)
                out.insert(out.end(), out.size() % 4, 0);

            Write<DWORD>(out, style | WS_CHILD | WS_VISIBLE);
            Write<DWORD>(out, 0);
            Write<short>(out, x);
            Write<short>(out, y);
            Write<short>(out, w);
            Write<short>(out, h);
            Write<WORD>(out, 0);
            Write<DWORD>(out, control);
            WriteString(out, text);
            Write<WORD>(out, 0);

            *(WORD*)(&out[8]) += 1;
        }

        std::wstring GetFormatString(const WAVEFORMATEX& format)
        {
            DspFormat dspFormat = DspFormatFromWaveFormat(format);

            const WAVEFORMATEXTENSIBLE* pFormatExt =
                (format.wFormatTag == WAVE_FORMAT_EXTENSIBLE) ?
                    reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(&format) : nullptr;

            bool pcm24in32 = (dspFormat == DspFormat::Pcm32 &&
                              pFormatExt && pFormatExt->Samples.wValidBitsPerSample == 24);

            switch (dspFormat)
            {
                case DspFormat::Pcm8:
                    return L"PCM-8";

                case DspFormat::Pcm16:
                    return L"PCM-16";

                case DspFormat::Pcm24:
                    return L"PCM-24";

                case DspFormat::Pcm32:
                    return pcm24in32 ? L"PCM-24 (Padded)" : L"PCM-32";

                case DspFormat::Float:
                    return L"Float";

                case DspFormat::Double:
                    return L"Double";
            }

            assert(dspFormat == DspFormat::Unknown);

            if (format.wFormatTag == WAVE_FORMAT_DOLBY_AC3_SPDIF)
                return L"AC3/DTS";

            if (pFormatExt)
            {
                if (pFormatExt->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DTS_HD)
                    return L"DTS-HD";

                if (pFormatExt->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_MLP)
                    return L"TrueHD";

                if (pFormatExt->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS)
                    return L"EAC3";

                if (pFormatExt->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_WMA_PRO)
                    return L"WMA Pro";
            }

            return L"Unknown";
        }
    }

    std::vector<char> MyPropertyPage::CreateDialogData(SharedWaveFormat inputFormat, const AudioDevice* pDevice,
                                                       std::vector<std::wstring> processors, bool externalClock, bool live)
    {
        std::wstring adapterField = (pDevice && pDevice->GetAdapterName()) ? *pDevice->GetAdapterName() : L"-";

        std::wstring endpointField = (pDevice && pDevice->GetEndpointName()) ? *pDevice->GetEndpointName() : L"-";

        std::wstring exclusiveField = (pDevice ? (pDevice->IsExclusive() ? L"Yes" : L"No") : L"-");

        std::wstring bufferField = (pDevice ? std::to_wstring(pDevice->GetBufferDuration()) + L"ms" : L"-");

        const bool bitstreaming = (inputFormat && DspFormatFromWaveFormat(*inputFormat) == DspFormat::Unknown);

        std::wstring bitstreamingField = (inputFormat ? (bitstreaming ? L"Yes" : L"No") : L"-");

        std::wstring slavingField = live ? L"Live Source" : externalClock ? L"Graph Clock" : L"Audio Device";

        std::wstring channelsInputField = (inputFormat && !bitstreaming) ? std::to_wstring(inputFormat->nChannels) +
                                              L" (" + GetHexString(DspMatrix::GetChannelMask(*inputFormat)) + L")" : L"-";
        std::wstring channelsDeviceField = (pDevice && !bitstreaming) ? std::to_wstring(pDevice->GetWaveFormat()->nChannels) +
                                              L" (" + GetHexString(DspMatrix::GetChannelMask(*pDevice->GetWaveFormat())) + L")" : L"-";
        std::wstring channelsField = (channelsInputField == channelsDeviceField) ?
                                         channelsInputField : channelsInputField + L" -> " + channelsDeviceField;

        std::wstring formatInputField = (inputFormat ? GetFormatString(*inputFormat) : L"-");
        std::wstring formatDeviceField = (pDevice ? GetFormatString(*pDevice->GetWaveFormat()) : L"-");
        std::wstring formatField = (formatInputField == formatDeviceField) ?
                                       formatInputField : formatInputField + L" -> " + formatDeviceField;

        std::wstring rateInputField = (inputFormat && !bitstreaming) ? std::to_wstring(inputFormat->nSamplesPerSec) : L"-";
        std::wstring rateDeviceField = (pDevice && !bitstreaming) ? std::to_wstring(pDevice->GetWaveFormat()->nSamplesPerSec) : L"-";
        std::wstring rateField = (rateInputField == rateDeviceField) ?
                                      rateInputField : rateInputField + L" -> " + rateDeviceField;

        std::wstring processorsField;
        for (const auto& s : processors)
        {
            if (!processorsField.empty())
                processorsField += L", ";

            processorsField += s;
        }
        if (processorsField.empty())
            processorsField = L"-";

        std::vector<char> dialogData;

        WriteDialogHeader(dialogData, L"MS Shell Dlg", 8, 220, 160);
        WriteDialogItem(dialogData, BS_GROUPBOX, 0x0080FFFF, 5, 5, 210, 150, L"Renderer Status");
        WriteDialogItem(dialogData, BS_TEXT | SS_RIGHT, 0x0082FFFF, 10, 20,  60,  8, L"Adapter:");
        WriteDialogItem(dialogData, BS_TEXT | SS_LEFT,  0x0082FFFF, 73, 20,  130, 8, adapterField);
        WriteDialogItem(dialogData, BS_TEXT | SS_RIGHT, 0x0082FFFF, 10, 32,  60,  8, L"Endpoint:");
        WriteDialogItem(dialogData, BS_TEXT | SS_LEFT,  0x0082FFFF, 73, 32,  130, 8, endpointField);
        WriteDialogItem(dialogData, BS_TEXT | SS_RIGHT, 0x0082FFFF, 10, 44,  60,  8, L"Exclusive:");
        WriteDialogItem(dialogData, BS_TEXT | SS_LEFT,  0x0082FFFF, 73, 44,  130, 8, exclusiveField);
        WriteDialogItem(dialogData, BS_TEXT | SS_RIGHT, 0x0082FFFF, 10, 56,  60,  8, L"Buffer:");
        WriteDialogItem(dialogData, BS_TEXT | SS_LEFT,  0x0082FFFF, 73, 56,  130, 8, bufferField);
        WriteDialogItem(dialogData, BS_TEXT | SS_RIGHT, 0x0082FFFF, 10, 68,  60,  8, L"Bitstreaming:");
        WriteDialogItem(dialogData, BS_TEXT | SS_LEFT,  0x0082FFFF, 73, 68,  130, 8, bitstreamingField);
        WriteDialogItem(dialogData, BS_TEXT | SS_RIGHT, 0x0082FFFF, 10, 80,  60,  8, L"Slaving:");
        WriteDialogItem(dialogData, BS_TEXT | SS_LEFT,  0x0082FFFF, 73, 80,  130, 8, slavingField);
        WriteDialogItem(dialogData, BS_TEXT | SS_RIGHT, 0x0082FFFF, 10, 92,  60,  8, L"Format:");
        WriteDialogItem(dialogData, BS_TEXT | SS_LEFT,  0x0082FFFF, 73, 92,  130, 8, formatField);
        WriteDialogItem(dialogData, BS_TEXT | SS_RIGHT, 0x0082FFFF, 10, 104, 60,  8, L"Channels:");
        WriteDialogItem(dialogData, BS_TEXT | SS_LEFT,  0x0082FFFF, 73, 104, 130, 8, channelsField);
        WriteDialogItem(dialogData, BS_TEXT | SS_RIGHT, 0x0082FFFF, 10, 116, 60,  8, L"Rate:");
        WriteDialogItem(dialogData, BS_TEXT | SS_LEFT,  0x0082FFFF, 73, 116, 130, 8, rateField);
        WriteDialogItem(dialogData, BS_TEXT | SS_RIGHT, 0x0082FFFF, 10, 128, 60,  8, L"Processors:");
        WriteDialogItem(dialogData, BS_TEXT | SS_LEFT,  0x0082FFFF, 73, 128, 130, 24, processorsField);

        return dialogData;
    }

    MyPropertyPage::MyPropertyPage()
        : CUnknown(L"SaneAudioRenderer::MyPropertyPage", nullptr)
    {
        m_dialogData = CreateDialogData(nullptr, nullptr, {}, false, false);
    }

    STDMETHODIMP MyPropertyPage::NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        return (riid == __uuidof(IPropertyPage)) ?
                   GetInterface(static_cast<IPropertyPage*>(this), ppv) :
                   CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }

    STDMETHODIMP MyPropertyPage::SetPageSite(IPropertyPageSite* pPageSite)
    {
        if (!m_pageSite && !pPageSite)
            return E_UNEXPECTED;

        m_pageSite = nullptr;
        CheckPointer(pPageSite, S_OK);

        return pPageSite->QueryInterface(IID_PPV_ARGS(&m_pageSite));
    }

    STDMETHODIMP MyPropertyPage::Activate(HWND hParent, LPCRECT pRect, BOOL bModal)
    {
        CheckPointer(pRect, E_POINTER);

        if (m_hWindow)
            return E_UNEXPECTED;

        m_hWindow = CreateDialogIndirect(GetModuleHandle(nullptr), (LPCDLGTEMPLATE)m_dialogData.data(), hParent, nullptr);

        if (!m_hWindow)
            return E_OUTOFMEMORY;

        Move(pRect);

        return S_OK;
    }

    STDMETHODIMP MyPropertyPage::Deactivate()
    {
        DestroyWindow(m_hWindow);
        m_hWindow = NULL;
        return S_OK;
    }

    STDMETHODIMP MyPropertyPage::GetPageInfo(PROPPAGEINFO* pPageInfo)
    {
        CheckPointer(pPageInfo, E_POINTER);

        pPageInfo->cb = sizeof(PROPPAGEINFO);

        const wchar_t title[] = L"Status";
        pPageInfo->pszTitle = (LPOLESTR)CoTaskMemAlloc(sizeof(title));
        CheckPointer(pPageInfo->pszTitle, E_OUTOFMEMORY);
        memcpy(pPageInfo->pszTitle, title, sizeof(title));

        pPageInfo->size = {0, 0};
        pPageInfo->pszDocString = nullptr;
        pPageInfo->pszHelpFile = nullptr;
        pPageInfo->dwHelpContext = 0;

        // This is how GetDialogSize() from DirectShow Base Classes does it.
        HWND hWindow = CreateDialogIndirect(GetModuleHandle(nullptr), (LPCDLGTEMPLATE)m_dialogData.data(), GetDesktopWindow(), nullptr);
        if (hWindow)
        {
            RECT rect;
            if (GetWindowRect(hWindow, &rect))
            {
                pPageInfo->size.cx = rect.right - rect.left;
                pPageInfo->size.cy = rect.bottom - rect.top;
            }

            DestroyWindow(hWindow);
        }

        return S_OK;
    }

    STDMETHODIMP MyPropertyPage::SetObjects(ULONG nObjects, IUnknown** ppUnk)
    {
        if (nObjects != 1)
            return E_UNEXPECTED;

        CheckPointer(ppUnk, E_POINTER);
        CheckPointer(ppUnk[0], E_POINTER);

        IStatusPageDataPtr data;
        ReturnIfFailed(ppUnk[0]->QueryInterface(IID_PPV_ARGS(&data)));

        ReturnIfFailed(data->GetPageData(m_dialogData));

        return S_OK;
    }

    STDMETHODIMP MyPropertyPage::Show(UINT cmdShow)
    {
        ShowWindow(m_hWindow, cmdShow);
        return S_OK;
    }

    STDMETHODIMP MyPropertyPage::Move(LPCRECT pRect)
    {
        MoveWindow(m_hWindow, pRect->left, pRect->top, pRect->right - pRect->left, pRect->bottom - pRect->top, TRUE);
        return S_OK;
    }
}
