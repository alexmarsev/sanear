#pragma once

#define ReturnIfFailed(x) { HRESULT hr = (x); if (FAILED(hr)) return hr; }
#define ReturnIfNotEquals(r, x) { HRESULT hr = (x); if (hr != r) return hr; }

namespace SaneAudioRenderer
{
    // One second in 100ns units.
    const int64_t OneSecond = 10000000;

    inline void ThrowIfFailed(HRESULT result)
    {
        if (FAILED(result))
            throw result;
    }

    inline int64_t GetPerformanceFrequency()
    {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart;
    }

    inline int64_t GetPerformanceCounter()
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return counter.QuadPart;
    }

    struct AlignedFreeDeleter
    {
        void operator()(void* p)
        {
            _aligned_free(p);
        }
    };

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

    class CoInitializeHelper final
    {
    public:
        explicit CoInitializeHelper(DWORD appartment) : m_initialized(SUCCEEDED(CoInitializeEx(nullptr, appartment))) {}
        CoInitializeHelper(const CoInitializeHelper&) = delete;
        CoInitializeHelper& operator=(const CoInitializeHelper&) = delete;
        ~CoInitializeHelper() { if (m_initialized) CoUninitialize(); }
        bool Initialized() const { return m_initialized; }
    private:
        const bool m_initialized;
    };

    class TimePeriodHelper final
    {
    public:
        explicit TimePeriodHelper(UINT period) : m_period(period) { timeBeginPeriod(m_period); }
        TimePeriodHelper(const TimePeriodHelper&) = delete;
        TimePeriodHelper& operator=(const TimePeriodHelper&) = delete;
        ~TimePeriodHelper() { timeEndPeriod(m_period); }
    private:
        const UINT m_period;
    };
}
