#pragma once

namespace SaneAudioRenderer
{
    class ApartmentInvokeHelper final
    {
    public:
        explicit ApartmentInvokeHelper(HRESULT& result)
        {
            if (FAILED(result))
                return;

            try
            {
                if (static_cast<HANDLE>(m_wake) == NULL ||
                    static_cast<HANDLE>(m_done) == NULL)
                {
                    throw E_OUTOFMEMORY;
                }

                m_thread = std::thread(
                    [this]
                    {
                        CoInitializeHelper coInitializeHelper(COINIT_APARTMENTTHREADED);

                        while (!m_exit)
                        {
                            auto handles = make_array(static_cast<HANDLE>(m_wake));
                            DWORD code = MsgWaitForMultipleObjectsEx(handles.size(), handles.data(), INFINITE,
                                                                     QS_ALLINPUT, MWMO_INPUTAVAILABLE);

                            if (code == WAIT_OBJECT_0 + handles.size())
                            {
                                MSG msg;
                                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                                {
                                    TranslateMessage(&msg);
                                    DispatchMessage(&msg);
                                }
                            }
                            else if (code == WAIT_OBJECT_0)
                            {
                                if (m_function)
                                {
                                    m_result = m_function();
                                    m_function = nullptr;
                                    m_done.Set();
                                }
                            }
                        }
                    }
                );
            }
            catch (HRESULT ex)
            {
                result = ex;
            }
            catch (std::system_error&)
            {
                result = E_FAIL;
            }
        }

        ApartmentInvokeHelper(const ApartmentInvokeHelper&) = delete;
        ApartmentInvokeHelper& operator=(const ApartmentInvokeHelper&) = delete;

        ~ApartmentInvokeHelper()
        {
            m_exit = true;
            m_wake.Set();

            if (m_thread.joinable())
                m_thread.join();
        }

        template <typename I, typename F>
        HRESULT Invoke(IUnknown* pObject, F invokeFunction)
        {
            if (!pObject)
                return E_INVALIDARG;

            {
                IGlobalInterfaceTablePtr table;
                ReturnIfFailed(CoCreateInstance(CLSID_StdGlobalInterfaceTable, nullptr,
                                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&table)));
                ReturnIfFailed(table->RegisterInterfaceInGlobal(pObject, __uuidof(I), &m_cookie));
            }

            {
                m_function = [&]
                {
                    IGlobalInterfaceTablePtr table;
                    ReturnIfFailed(CoCreateInstance(CLSID_StdGlobalInterfaceTable, nullptr,
                                                    CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&table)));

                    I* pInterface;
                    ReturnIfFailed(table->GetInterfaceFromGlobal(m_cookie, IID_PPV_ARGS(&pInterface)));

                    HRESULT result = invokeFunction(pInterface);

                    pInterface->Release();

                    table->RevokeInterfaceFromGlobal(m_cookie);
                    m_cookie = 0;

                    return result;
                };
            }

            m_wake.Set();
            m_done.Wait();

            return m_result;
        }

    private:

        std::thread m_thread;
        std::atomic<bool> m_exit = false;
        CAMEvent m_wake;
        CAMEvent m_done;

        std::function<HRESULT(void)> m_function;
        DWORD m_cookie = 0;
        HRESULT m_result = S_OK;
    };
}
