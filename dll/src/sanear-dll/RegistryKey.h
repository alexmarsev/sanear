#pragma once

namespace SaneAudioRenderer
{
    class RegistryKey final
    {
    public:

        RegistryKey() = default;
        ~RegistryKey();
        RegistryKey(const RegistryKey&) = delete;
        RegistryKey& operator=(const RegistryKey&) = delete;

        HRESULT Open(const wchar_t* key);
        void Close();

        bool SetString(const wchar_t* name, const wchar_t* value);
        bool GetString(const wchar_t* name, std::vector<wchar_t>& value);

        bool SetUint(const wchar_t* name, uint32_t value);
        bool RegistryKey::GetUint(const wchar_t* name, uint32_t& value);

    private:

        HKEY m_hKey = NULL;
    };
}
