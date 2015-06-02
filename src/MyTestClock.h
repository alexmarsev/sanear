#pragma once

namespace SaneAudioRenderer
{
    class MyTestClock final
        : public CBaseReferenceClock
    {
    public:

        MyTestClock(HRESULT& result);
        MyTestClock(const MyTestClock&) = delete;
        MyTestClock& operator=(const MyTestClock&) = delete;

        REFERENCE_TIME GetPrivateTime() override;

    private:

        const int64_t m_performanceFrequency;
    };
}
