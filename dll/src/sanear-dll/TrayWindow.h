#pragma once

#include "../../../src/Interfaces.h"

namespace SaneAudioRenderer
{
    class TrayWindow final
    {
    public:

        TrayWindow();
        ~TrayWindow();
        TrayWindow(const TrayWindow&) = delete;
        TrayWindow& operator=(const TrayWindow&) = delete;

        HRESULT Init(ISettings* pSettings);

    private:

        void Destroy();

        void AddIcon();
        void RemoveIcon();

        void AddMenu();
        void RemoveMenu();

        void OnTrayNotify(WPARAM wParam, LPARAM lParam);
        void OnCommand(WPARAM wParam, LPARAM lParam);

        DWORD ThreadProc();
        LRESULT WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        UINT m_taskbarCreatedMessage = 0;
        NOTIFYICONDATA m_nid;

        ISettingsPtr m_settings;
        HANDLE m_hThread = NULL;
        HWND m_hWindow = NULL;
        HMENU m_hMenu = NULL;
        std::promise<bool> m_windowCreated;
        std::vector<std::pair<std::wstring, std::wstring>> m_devices;
    };
}
