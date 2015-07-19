#include "pch.h"
#include "RegistryKey.h"

namespace SaneAudioRenderer
{
    HRESULT RegistryKey::Open(HKEY key, const wchar_t* subkey)
    {
        Close();

        return RegCreateKeyEx(key, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                              KEY_READ | KEY_WRITE, nullptr, &m_hKey, nullptr);
    }

    void RegistryKey::Close()
    {
        if (m_hKey != NULL)
        {
            RegCloseKey(m_hKey);
            m_hKey = NULL;
        }
    }

    RegistryKey::~RegistryKey()
    {
        Close();
    }

    bool RegistryKey::SetString(const wchar_t* key, const wchar_t* value)
    {
        const DWORD valueSize = (DWORD)(wcslen(value) + 1) * sizeof(wchar_t);
        return RegSetValueEx(m_hKey, key, 0, REG_SZ, (const BYTE*)value, valueSize) == ERROR_SUCCESS;
    }

    bool RegistryKey::GetString(const wchar_t* name, std::vector<wchar_t>& value)
    {
        DWORD valueSize;
        DWORD valuetype;

        if (RegQueryValueEx(m_hKey, name, 0, &valuetype, nullptr, &valueSize) != ERROR_SUCCESS)
            return false;

        try
        {
            value.resize(valueSize / sizeof(wchar_t));
        }
        catch (std::bad_alloc&)
        {
            return false;
        }

        if (RegQueryValueEx(m_hKey, name, 0, &valuetype, (BYTE*)value.data(), &valueSize) != ERROR_SUCCESS ||
            valuetype != REG_SZ)
        {
            return false;
        }

        return true;
    }

    bool RegistryKey::SetUint(const wchar_t* name, uint32_t value)
    {
        return RegSetValueEx(m_hKey, name, 0, REG_DWORD, (BYTE*)&value, sizeof(uint32_t)) == ERROR_SUCCESS;
    }

    bool RegistryKey::GetUint(const wchar_t* name, uint32_t& value)
    {
        DWORD valueSize = sizeof(uint32_t);
        DWORD valuetype;

        if (RegQueryValueEx(m_hKey, name, 0, &valuetype, (BYTE*)&value, &valueSize) != ERROR_SUCCESS ||
            valuetype != REG_DWORD)
        {
            return false;
        }

        return true;
    }
}
