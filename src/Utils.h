#pragma once

#define ReturnIfFailed(x) { HRESULT hr = (x); if (FAILED(hr)) return hr; }
#define ReturnIfNotEquals(r, x) { HRESULT hr = (x); if (hr != r) return hr; }

namespace SaneAudioRenderer
{
    // One second in 100ns units.
    const int64_t OneSecond = 10000000;

    // One millisecond in 100ns units.
    const int64_t OneMillisecond = OneSecond / 1000;

    typedef std::shared_ptr<const std::wstring> SharedString;

    typedef std::shared_ptr<const WAVEFORMATEX> SharedWaveFormat;

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

    struct CoTaskMemFreeDeleter
    {
        void operator()(void* p)
        {
            CoTaskMemFree(p);
        }
    };

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

    inline std::wstring GetHexString(uint32_t number)
    {
        std::array<wchar_t, 11> temp;
        return swprintf(temp.data(), temp.size(), L"0x%X", number) > 0 ? temp.data() : L"";
    }

    inline SharedWaveFormat CopyWaveFormat(const WAVEFORMATEX& format)
    {
        size_t size = sizeof(WAVEFORMATEX) + format.cbSize;
        void* pBuffer = _aligned_malloc(size, 4);
        if (!pBuffer) throw std::bad_alloc();
        memcpy(pBuffer, &format, size);
        return SharedWaveFormat(reinterpret_cast<WAVEFORMATEX*>(pBuffer), AlignedFreeDeleter());
    }

    inline void DebugOutForward(std::wostringstream&) {}

    template <typename T0, typename... T>
    inline void DebugOutForward(std::wostringstream& stream, T0&& arg0, T&&... args)
    {
        stream << " " << arg0;
        DebugOutForward(stream, std::forward<T>(args)...);
    }

    template <typename... T>
    inline void DebugOut(T&&... args)
    {
        #ifndef NDEBUG
        try
        {
            std::wostringstream stream;
            stream << "sanear:";
            DebugOutForward(stream, std::forward<T>(args)...);
            stream << "\n";
            OutputDebugString(stream.str().c_str());
        }
        catch (...)
        {
            OutputDebugString(L"sanear: caught exception while formatting debug message");
        }
        #endif
    }

    template <class T>
    inline const char* ClassName(T* p)
    {
        const char* str = strrchr(typeid(*p).name(), ':');
        return str ? str + 1 : "";
    }

    template <typename>
    class WinapiFunc;
    template <typename ReturnType, typename...Args>
    class WinapiFunc<ReturnType WINAPI(Args...)> final
    {
    public:
        typedef ReturnType(WINAPI* Func)(Args...);
        WinapiFunc(LPCWSTR dll, LPCSTR func) { m_lib = LoadLibrary(dll); m_func = (Func)GetProcAddress(m_lib, func); }
        WinapiFunc(const WinapiFunc&) = delete;
        WinapiFunc& operator=(const WinapiFunc&) = delete;
        ~WinapiFunc() { FreeLibrary(m_lib); }
        explicit operator bool() const { return !!m_func; }
        ReturnType operator()(Args...args) const { return m_func(args...); }
    private:
        HMODULE m_lib = NULL;
        Func m_func = nullptr;
    };
}
