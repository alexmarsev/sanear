#pragma once

#include "../../../src/pch.h"

#include <VersionHelpers.h>

namespace SaneAudioRenderer
{
    template <class T, DWORD(T::*ThreadProc)() = &T::ThreadProc>
    unsigned CALLBACK StaticThreadProc(LPVOID p)
    {
        return (static_cast<T*>(p)->*ThreadProc)();
    }

    template <class T, LRESULT(T::*WindowProc)(HWND, UINT, WPARAM, LPARAM) = &T::WindowProc>
    LRESULT CALLBACK StaticWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (LONG_PTR userData = GetWindowLongPtr(hWnd, GWLP_USERDATA))
            return (reinterpret_cast<T*>(userData)->*WindowProc)(hWnd, msg, wParam, lParam);

        if (msg == WM_NCCREATE)
        {
            CREATESTRUCT* pCreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
            return (static_cast<T*>(pCreateStruct->lpCreateParams)->*WindowProc)(hWnd, msg, wParam, lParam);
        }

        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    inline void RunMessageLoop()
    {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

_COM_SMARTPTR_TYPEDEF(IFilterMapper2, __uuidof(IFilterMapper2));
