#include "pch.h"
#include "TrayWindow.h"

#include "resource.h"

namespace SaneAudioRenderer
{
    namespace
    {
        const auto WindowClass = L"SaneAudioRenderer::TrayWindow";
        const auto WindowTitle = L"";

        enum
        {
            WM_TRAYNOTIFY = WM_USER + 100,
        };

        enum Item
        {
            ExclusiveMode = 10,
            AllowBitstreaming,
            EnableCrossfeed,
            CrossfeedCMoy,   // used in CheckMenuRadioItem()
            CrossfeedJMeier, // used in CheckMenuRadioItem()
            DefaultDevice,   // needs to be last
        };

        std::vector<std::pair<std::wstring, std::wstring>> GetDevices()
        {
            std::vector<std::pair<std::wstring, std::wstring>> devices;

            IMMDeviceEnumeratorPtr enumerator;
            IMMDeviceCollectionPtr collection;
            UINT count = 0;

            if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator))) &&
                SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED, &collection)) &&
                SUCCEEDED(collection->GetCount(&count)))
            {
                for (UINT i = 0; i < count; i++)
                {
                    IMMDevicePtr device;
                    LPWSTR pDeviceId;
                    IPropertyStorePtr devicePropertyStore;
                    PROPVARIANT friendlyName; // TODO: make wrapper class this, use it also in AudioDeviceManager
                    PropVariantInit(&friendlyName);

                    if (SUCCEEDED(collection->Item(i, &device)) &&
                        SUCCEEDED(device->GetId(&pDeviceId)) &&
                        SUCCEEDED(device->OpenPropertyStore(STGM_READ, &devicePropertyStore)) &&
                        SUCCEEDED(devicePropertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName)))
                    {
                        std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> holder(pDeviceId);
                        devices.emplace_back(friendlyName.pwszVal, pDeviceId);
                        PropVariantClear(&friendlyName);
                    }
                }
            }

            return devices;
        }
    }

    TrayWindow::TrayWindow()
    {
        m_nid = {sizeof(m_nid)};
    }

    TrayWindow::~TrayWindow()
    {
        Destroy();
    }

    HRESULT TrayWindow::Init(ISettings* pSettings)
    {
        CheckPointer(pSettings, E_POINTER);

        assert(m_hThread == NULL);
        Destroy();

        ReturnIfFailed(pSettings->QueryInterface(IID_PPV_ARGS(&m_settings)));

        m_hThread = (HANDLE)_beginthreadex(nullptr, 0, StaticThreadProc<TrayWindow>, this, 0, nullptr);

        if (m_hThread == NULL || !m_windowCreated.get_future().get())
            return E_FAIL;

        return S_OK;
    }

    DWORD TrayWindow::ThreadProc()
    {
        CoInitializeHelper coInitializeHelper(COINIT_MULTITHREADED);

        if (!coInitializeHelper.Initialized())
        {
            m_windowCreated.set_value(false);
            return 0;
        }

        WNDCLASSEX windowClass = {
            sizeof(windowClass), 0, StaticWindowProc<TrayWindow>, 0, 0, g_hInst,
            NULL, NULL, NULL, nullptr, WindowClass, NULL
        };

        RegisterClassEx(&windowClass);

        m_hWindow = CreateWindowEx(0, WindowClass, WindowTitle, 0, 0, 0, 0, 0, 0, NULL, g_hInst, this);

        if (m_hWindow == NULL)
        {
            m_windowCreated.set_value(false);
            return 0;
        }

        m_windowCreated.set_value(true);

        m_taskbarCreatedMessage = RegisterWindowMessage(L"TaskbarCreated");

        AddIcon();

        RunMessageLoop();

        return 0;
    }

    LRESULT TrayWindow::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == m_taskbarCreatedMessage)
        {
            AddIcon();
            return 0;
        }

        switch (msg)
        {
            case WM_COMMAND:
                OnCommand(wParam, lParam);
                return 0;

            case WM_TRAYNOTIFY:
                OnTrayNotify(wParam, lParam);
                return 0;

            case WM_DESTROY:
                RemoveMenu();
                RemoveIcon();
                PostQuitMessage(0);
                return 0;
        }

        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    void TrayWindow::Destroy()
    {
        if (m_hThread != NULL)
        {
            PostMessage(m_hWindow, WM_DESTROY, 0, 0);
            WaitForSingleObject(m_hThread, INFINITE);
            CloseHandle(m_hThread);
            m_hThread = NULL;
        }
    }

    void TrayWindow::AddIcon()
    {
        m_nid.hWnd = m_hWindow;
        m_nid.uVersion = NOTIFYICON_VERSION_4;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        LoadIconMetric(g_hInst, MAKEINTRESOURCE(IDI_ICON1), LIM_SMALL, &m_nid.hIcon);
        m_nid.uCallbackMessage = WM_TRAYNOTIFY;
        lstrcpy(m_nid.szTip, L"Sanear Audio Renderer");

        Shell_NotifyIcon(NIM_ADD, &m_nid);
        Shell_NotifyIcon(NIM_SETVERSION, &m_nid);
    }

    void TrayWindow::RemoveIcon()
    {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
    }

    void TrayWindow::AddMenu()
    {
        RemoveMenu();

        m_hMenu = CreateMenu();
        HMENU hMenu = CreateMenu();

        BOOL allowBitstreaming;
        m_settings->GetAllowBitstreaming(&allowBitstreaming);

        BOOL crossfeedEnabled;
        m_settings->GetCrossfeedEnabled(&crossfeedEnabled);

        UINT32 crosfeedCutoff;
        UINT32 crosfeedLevel;
        m_settings->GetCrossfeedSettings(&crosfeedCutoff, &crosfeedLevel);

        LPWSTR pDeviceId = nullptr;
        BOOL exclusive;
        m_settings->GetOuputDevice(&pDeviceId, &exclusive, nullptr);
        std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> holder(pDeviceId); // TODO: write specialized wrapper for this

        try
        {
            m_devices = GetDevices();
        }
        catch (std::bad_alloc&)
        {
            m_devices.clear();
        }

        MENUITEMINFO separator = {sizeof(MENUITEMINFO)};
        separator.fMask = MIIM_TYPE;
        separator.fType = MFT_SEPARATOR;

        MENUITEMINFO check = {sizeof(MENUITEMINFO)};
        check.fMask = MIIM_STRING | MIIM_ID | MIIM_CHECKMARKS | MIIM_STATE;

        MENUITEMINFO submenu = {sizeof(MENUITEMINFO)};
        submenu.fMask = MIIM_SUBMENU;

        check.wID = Item::AllowBitstreaming;
        check.dwTypeData = L"Allow bitstreaming (in exclusive WASAPI mode)";
        check.fState = (allowBitstreaming ? MFS_CHECKED : MFS_UNCHECKED) | (exclusive ? MFS_ENABLED : MFS_DISABLED);
        InsertMenuItem(hMenu, 0, TRUE, &check);

        check.wID = Item::ExclusiveMode;
        check.dwTypeData = L"Exclusive WASAPI mode";
        check.fState = (exclusive ? MFS_CHECKED : MFS_UNCHECKED);
        InsertMenuItem(hMenu, 0, TRUE, &check);

        InsertMenuItem(hMenu, 0, TRUE, &separator);

        check.wID = Item::CrossfeedJMeier;
        check.dwTypeData = L"J.Meier-like preset";
        check.fState = (crossfeedEnabled ? MFS_ENABLED : MFS_DISABLED);
        InsertMenuItem(hMenu, 0, TRUE, &check);

        check.wID = Item::CrossfeedCMoy;
        check.dwTypeData = L"C.Moy-like preset";
        check.fState = (crossfeedEnabled ? MFS_ENABLED : MFS_DISABLED);
        InsertMenuItem(hMenu, 0, TRUE, &check);

        if (crosfeedCutoff == ISettings::CROSSFEED_CUTOFF_FREQ_CMOY &&
            crosfeedLevel == ISettings::CROSSFEED_LEVEL_CMOY)
        {
            CheckMenuRadioItem(hMenu, Item::CrossfeedCMoy, Item::CrossfeedJMeier, Item::CrossfeedCMoy, MF_BYCOMMAND);
        }
        else if (crosfeedCutoff == ISettings::CROSSFEED_CUTOFF_FREQ_JMEIER &&
                 crosfeedLevel == ISettings::CROSSFEED_LEVEL_JMEIER)
        {
            CheckMenuRadioItem(hMenu, Item::CrossfeedCMoy, Item::CrossfeedJMeier, Item::CrossfeedJMeier, MF_BYCOMMAND);
        }

        check.wID = Item::EnableCrossfeed;
        check.dwTypeData = L"Enable stereo crossfeed (for headphones)";
        check.fState = (crossfeedEnabled ? MFS_CHECKED : MFS_UNCHECKED);
        InsertMenuItem(hMenu, 0, TRUE, &check);

        InsertMenuItem(hMenu, 0, TRUE, &separator);

        UINT selectedDevice = Item::DefaultDevice;

        for (size_t i = 0, n = m_devices.size(); i < n; i++)
        {
            const auto& device = m_devices[n - i - 1];

            check.wID = Item::DefaultDevice + (UINT)(n - i);
            check.dwTypeData = (LPWSTR)device.first.c_str();
            check.fState = MFS_ENABLED;
            InsertMenuItem(hMenu, 0, TRUE, &check);

            if (pDeviceId && device.second == pDeviceId)
                selectedDevice = check.wID;
        }

        check.wID = Item::DefaultDevice;
        check.dwTypeData = L"Default Device";
        check.fState = MFS_ENABLED;
        InsertMenuItem(hMenu, 0, TRUE, &check);

        CheckMenuRadioItem(hMenu, Item::DefaultDevice, Item::DefaultDevice + (UINT)m_devices.size(), selectedDevice, MF_BYCOMMAND);

        submenu.hSubMenu = hMenu;
        InsertMenuItem(m_hMenu, 0, TRUE, &submenu);
    }

    void TrayWindow::RemoveMenu()
    {
        if (m_hMenu)
        {
            EndMenu();
            DestroyMenu(m_hMenu);
            m_hMenu = NULL;
        }
    }

    void TrayWindow::OnTrayNotify(WPARAM wParam, LPARAM lParam)
    {
        switch (LOWORD(lParam))
        {
            case NIN_KEYSELECT:
            case NIN_SELECT:
            case WM_CONTEXTMENU:
                AddMenu();
                SetForegroundWindow(m_hWindow);
                TrackPopupMenuEx(GetSubMenu(m_hMenu, 0), TPM_LEFTALIGN | TPM_BOTTOMALIGN, LOWORD(wParam), HIWORD(wParam), m_hWindow, NULL);
                break;
        }
    }

    void TrayWindow::OnCommand(WPARAM wParam, LPARAM lParam)
    {
        switch (wParam)
        {
            case Item::ExclusiveMode:
            {
                LPWSTR pDeviceId = nullptr;
                BOOL exclusive;
                UINT32 buffer;
                m_settings->GetOuputDevice(&pDeviceId, &exclusive, &buffer);
                std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> holder(pDeviceId);
                m_settings->SetOuputDevice(pDeviceId, !exclusive, buffer);
                break;
            }

            case Item::AllowBitstreaming:
            {
                BOOL value;
                m_settings->GetAllowBitstreaming(&value);
                m_settings->SetAllowBitstreaming(!value);
                break;
            }

            case Item::EnableCrossfeed:
            {
                BOOL value;
                m_settings->GetCrossfeedEnabled(&value);
                m_settings->SetCrossfeedEnabled(!value);
                break;
            }

            case Item::CrossfeedCMoy:
            {
                m_settings->SetCrossfeedSettings(ISettings::CROSSFEED_CUTOFF_FREQ_CMOY, ISettings::CROSSFEED_LEVEL_CMOY);
                break;
            }

            case Item::CrossfeedJMeier:
            {
                m_settings->SetCrossfeedSettings(ISettings::CROSSFEED_CUTOFF_FREQ_JMEIER, ISettings::CROSSFEED_LEVEL_JMEIER);
                break;
            }

            case Item::DefaultDevice:
            {
                LPWSTR pDeviceId = nullptr;
                BOOL exclusive;
                UINT32 buffer;
                m_settings->GetOuputDevice(&pDeviceId, &exclusive, &buffer);
                std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> holder(pDeviceId);

                if (pDeviceId && *pDeviceId)
                    m_settings->SetOuputDevice(nullptr, exclusive, buffer);

                break;
            }

            default:
            {
                if (wParam <= Item::DefaultDevice || wParam > Item::DefaultDevice + m_devices.size())
                    break;

                const auto& selection = m_devices[wParam - Item::DefaultDevice - 1].second;

                LPWSTR pDeviceId = nullptr;
                BOOL exclusive;
                UINT32 buffer;
                m_settings->GetOuputDevice(&pDeviceId, &exclusive, &buffer);
                std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> holder(pDeviceId);

                if (!pDeviceId || selection != pDeviceId)
                    m_settings->SetOuputDevice(selection.c_str(), exclusive, buffer);
            }
        }
    }
}
