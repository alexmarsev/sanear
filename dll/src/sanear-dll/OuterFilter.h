#include "pch.h"

#include "../../../src/Factory.h"

namespace SaneAudioRenderer
{
    class OuterFilter final
        : public CUnknown
    {
    public:

        OuterFilter(IUnknown* pUnknown, const GUID& guid);
        OuterFilter(const OuterFilter&) = delete;
        OuterFilter& operator=(const OuterFilter&) = delete;

        DECLARE_IUNKNOWN

        STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

    private:

        HRESULT Init();

        const GUID& m_guid;
        bool m_initialized = false;
        ISettingsPtr m_settings;
        IUnknownPtr m_innerFilter;
    };
}
