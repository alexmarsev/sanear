#include "pch.h"
#include "DspLimiter.h"

namespace SaneAudioRenderer
{
    namespace
    {
        inline bool OverflowingLess(uint32_t a, uint32_t b)
        {
            return a - b > UINT32_MAX / 2;
        }

        inline float f_x(const std::pair<uint32_t, float>& left, const std::pair<uint32_t, float>& right)
        {
            assert(OverflowingLess(left.first, right.first));
            return (right.second - left.second) / (right.first - left.first);
        }

        inline float f(const std::pair<uint32_t, float>& left, float x, uint32_t pos)
        {
            return x * (pos - left.first) + left.second;
        }

        inline float f(const std::pair<uint32_t, float>& left, const std::pair<uint32_t, float>& right, uint32_t pos)
        {
            return f(left, f_x(left, right), pos);
        }
    }

    void DspLimiter::Initialize(ISettings* pSettings, uint32_t rate, bool exclusive)
    {
        assert(pSettings);
        m_settings = pSettings;
        UpdateSettings();

        m_exclusive = exclusive;

        m_attackFrames  = (uint32_t)std::round(rate / 2000.0f);
        m_releaseFrames = (uint32_t)std::round(rate / 2000.0f);
        m_windowFrames  = (uint32_t)std::round(rate / 100.0f);

        m_buffer.clear();
        m_bufferFrameCount = 0;
        m_bufferFirstFrame = 0;

        m_peaks = {};
    }

    bool DspLimiter::Active()
    {
        return !m_buffer.empty();
    }

    void DspLimiter::Process(DspChunk& chunk)
    {
        if (chunk.IsEmpty())
            return;

        if (m_settingsSerial != m_settings->GetSerial())
            UpdateSettings();

        if (chunk.GetFormat() == DspFormat::Float &&
            (m_exclusive || m_enabledShared))
        {
            DspChunk::ToFloat(chunk);

            m_bufferFrameCount += chunk.GetFrameCount();
            m_buffer.push_back(std::move(chunk));
            assert(chunk.IsEmpty());

            AnalyzeLastChunk();

            size_t bufferFrontFrames = m_buffer.front().GetFrameCount();
            if (m_bufferFrameCount - bufferFrontFrames >= m_windowFrames)
            {
                ModifyFirstChunk();

                m_bufferFrameCount -= bufferFrontFrames;
                m_bufferFirstFrame += bufferFrontFrames;
                chunk = std::move(m_buffer.front());
                m_buffer.pop_front();
            }
        }
        else if (!m_buffer.empty())
        {
            DspChunk::ToFloat(chunk);

            m_bufferFrameCount += chunk.GetFrameCount();
            m_buffer.push_back(std::move(chunk));
            assert(chunk.IsEmpty());

            DspChunk output;
            Finish(output);
            assert(!output.IsEmpty());
            assert(m_buffer.empty());

            chunk = std::move(output);
        }
    }

    void DspLimiter::Finish(DspChunk& chunk)
    {
        Process(chunk);

        if (!m_buffer.empty())
        {
            assert(chunk.IsEmpty() || chunk.GetFormat() == DspFormat::Float);

            DspChunk output(DspFormat::Float, m_buffer.front().GetChannelCount(),
                            chunk.GetFrameCount() + m_bufferFrameCount, m_buffer.front().GetRate());

            size_t offset = chunk.GetSize();

            if (!chunk.IsEmpty())
                memcpy(output.GetData(), chunk.GetConstData(), offset);

            while (!m_buffer.empty())
            {
                ModifyFirstChunk();

                const auto& front = m_buffer.front();
                assert(front.GetFormat() == DspFormat::Float);
                memcpy(output.GetData() + offset, front.GetConstData(), front.GetSize());
                offset += front.GetSize();
                m_bufferFrameCount -= front.GetFrameCount();
                m_bufferFirstFrame += front.GetFrameCount();

                m_buffer.pop_front();
            }

            assert(m_bufferFrameCount == 0);

            chunk = std::move(output);
        }
    }

    void DspLimiter::AnalyzeLastChunk()
    {
        const DspChunk& chunk = m_buffer.back();
        assert(chunk.GetFormat() == DspFormat::Float);

        const uint64_t chunkFirstFrame = m_bufferFirstFrame + m_bufferFrameCount - chunk.GetFrameCount();
        const uint32_t channels = chunk.GetChannelCount();

        auto data = reinterpret_cast<const float*>(chunk.GetConstData());
        for (size_t i = 0, n = chunk.GetSampleCount(); i < n; i++)
        {
            const float sample = std::fabs(data[i]);

            if (sample > m_limit)
            {
                const uint32_t channel = (uint32_t)(i % channels);
                const uint32_t peakFrame32 = (uint32_t)(chunkFirstFrame + i / channels);
                auto& channelPeaks = m_peaks[channel];

                if (channelPeaks.empty())
                {
                    //DbgOutString((std::wstring(L"start ") + std::to_wstring(peakFrame) + L" " +
                    //                                        std::to_wstring(sample) + L"\n").c_str());
                    channelPeaks.emplace_back(peakFrame32 - m_attackFrames, m_limit);
                    channelPeaks.emplace_back(peakFrame32, sample);
                    channelPeaks.emplace_back(peakFrame32 + m_releaseFrames, m_limit);
                }
                else
                {
                    assert(channelPeaks.size() > 1);
                    assert(channelPeaks.back().second == m_limit);
                    auto back = channelPeaks.rbegin();
                    auto nextToBack = back + 1;
                    if (OverflowingLess(back->first, peakFrame32) || f(*nextToBack, *back, peakFrame32) < sample)
                    {
                        channelPeaks.pop_back();
                        back = channelPeaks.rbegin();
                        nextToBack = back + 1;

                        while (nextToBack != channelPeaks.rend())
                        {
                            if (sample >= back->second &&
                                f(*nextToBack, {peakFrame32, sample}, back->first) > back->second)
                            {
                                //DbgOutString((std::wstring(L"drop ") + std::to_wstring(back->first) + L" " +
                                //                                       std::to_wstring(back->second) + L"\n").c_str());
                                channelPeaks.pop_back();
                                back = channelPeaks.rbegin();
                                nextToBack = back + 1;
                                continue;
                            }
                            break;
                        }

                        {
                            //DbgOutString((std::wstring(L"add ") + std::to_wstring(peakFrame) + L" " +
                            //                                      std::to_wstring(sample) + L"\n").c_str());
                            channelPeaks.emplace_back(peakFrame32, sample);
                            channelPeaks.emplace_back(peakFrame32 + m_releaseFrames, m_limit);
                        }
                    }
                    else
                    {
                        //DbgOutString((std::wstring(L"consume ") + std::to_wstring(peakFrame) + L" " +
                        //                                          std::to_wstring(sample) + L"\n").c_str());
                    }
                }
            }
        }
    }

    void DspLimiter::ModifyFirstChunk()
    {
        DspChunk& chunk = m_buffer.front();
        const uint32_t chunkFirstFrame32 = (uint32_t)m_bufferFirstFrame;

        for (size_t channel = 0, channels = chunk.GetChannelCount(); channel < channels; channel++)
        {
            auto& channelPeaks = m_peaks[channel];

            if (!channelPeaks.empty())
            {
                assert(channelPeaks.size() > 1);
                auto left = channelPeaks[0];
                auto right = channelPeaks[1];
                float x = f_x(left, right);

                const size_t frameOffset = OverflowingLess(left.first, chunkFirstFrame32) ?
                                               0 : (left.first - chunkFirstFrame32);

                auto data = reinterpret_cast<float*>(chunk.GetData());
                for (size_t i = frameOffset * channels + channel, n = chunk.GetSampleCount(); i < n; i += channels)
                {
                    const uint32_t frame32 = (uint32_t)(chunkFirstFrame32 + i / channels);

                    float& sample = data[i];
                    sample = sample / f(left, x, frame32) * m_limit;
                    assert(std::fabs(sample) <= m_limit);

                    if (!OverflowingLess(frame32, right.first))
                    {
                        assert(right.first == frame32);
                        channelPeaks.pop_front();
                        if (channelPeaks.size() == 1)
                        {
                            //DbgOutString(L"clear\n");
                            channelPeaks.clear();
                            break;
                        }
                        else
                        {
                            left = channelPeaks[0];
                            right = channelPeaks[1];
                            x = f_x(left, right);
                        }
                    }
                }
            }
        }
    }

    void DspLimiter::UpdateSettings()
    {
        m_settingsSerial = m_settings->GetSerial();

        BOOL enabledShared;
        m_settings->GetSharedModePeakLimiterEnabled(&enabledShared);
        m_enabledShared = !!enabledShared;
    }
}
