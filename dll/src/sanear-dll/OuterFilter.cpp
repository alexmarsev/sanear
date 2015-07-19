#include "pch.h"
#include "OuterFilter.h"

namespace SaneAudioRenderer
{
    OuterFilter::OuterFilter(IUnknown* pUnknown, const GUID& guid)
        : CUnknown(L"SaneAudioRenderer::OuterFilter", pUnknown)
        , m_guid(guid)
    {
    }

    STDMETHODIMP OuterFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        if (!m_initialized)
            ReturnIfFailed(Init());

        if (riid == IID_IUnknown)
            return CUnknown::NonDelegatingQueryInterface(riid, ppv);

        return m_innerFilter->QueryInterface(riid, ppv);
    }

    HRESULT OuterFilter::Init()
    {
        assert(!m_initialized);

        ReturnIfFailed(Factory::CreateSettings(&m_settings))
        ReturnIfFailed(Factory::CreateFilterAggregated(GetOwner(), m_guid, m_settings, &m_innerFilter));

        m_initialized = true;

        return S_OK;
    }
}
