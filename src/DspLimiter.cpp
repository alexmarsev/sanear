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

    void DspLimiter::Initialize(uint32_t rate, bool exclusive)
    {
        m_limit = (exclusive ? 1.0f : 0.98f);
        m_attackFrames = rate / 1700;
        m_releaseFrames = rate / 70;
        m_windowFrames = m_attackFrames + m_releaseFrames;

        m_buffer.clear();
        m_bufferFrameCount = 0;
        m_bufferFirstFrame = 0;

        m_peaks.clear();
    }

    bool DspLimiter::Active()
    {
        return !m_buffer.empty();
    }

    void DspLimiter::Process(DspChunk& chunk)
    {
        if (chunk.IsEmpty())
            return;

        if (m_limit != 1.0f || chunk.GetFormat() == DspFormat::Float)
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
        assert(m_limit > 0.0f);

        const DspChunk& chunk = m_buffer.back();
        assert(chunk.GetFormat() == DspFormat::Float);

        const uint64_t chunkFirstFrame = m_bufferFirstFrame + m_bufferFrameCount - chunk.GetFrameCount();
        const uint32_t channels = chunk.GetChannelCount();

        auto data = reinterpret_cast<const float*>(chunk.GetConstData());
        for (size_t frame = 0, frameCount = chunk.GetFrameCount(); frame < frameCount; frame++)
        {
            float sample = 0.0f;
            for (size_t i = 0; i < channels; i++)
                sample = std::max(sample, std::fabs(data[frame * channels + i]));

            if (sample > m_limit)
            {
                const uint32_t peakFrame32 = (uint32_t)(chunkFirstFrame + frame);
                if (m_peaks.empty())
                {
                    m_peaks.emplace_back(chunkFirstFrame + frame > m_attackFrames ? peakFrame32 - m_attackFrames : 0, m_limit);
                    m_peaks.emplace_back(peakFrame32, sample);
                    m_peaks.emplace_back(peakFrame32 + m_releaseFrames, m_limit);
                    //DbgOutString((std::wstring(L"start ") + std::to_wstring(peakFrame) + L" " +
                    //                                        std::to_wstring(sample) + L"\n").c_str());
                }
                else
                {
                    assert(m_peaks.size() > 1);
                    assert(m_peaks.back().second == m_limit);
                    auto back = m_peaks.rbegin();
                    auto nextToBack = back + 1;
                    if (OverflowingLess(back->first, peakFrame32) || f(*nextToBack, *back, peakFrame32) < sample)
                    {
                        m_peaks.pop_back();
                        back = m_peaks.rbegin();
                        nextToBack = back + 1;

                        while (nextToBack != m_peaks.rend())
                        {
                            if (sample >= back->second &&
                                f(*nextToBack, {peakFrame32, sample}, back->first) > back->second)
                            {
                                //DbgOutString((std::wstring(L"drop ") + std::to_wstring(back->first) + L" " +
                                //                                       std::to_wstring(back->second) + L"\n").c_str());
                                m_peaks.pop_back();
                                back = m_peaks.rbegin();
                                nextToBack = back + 1;
                                continue;
                            }
                            break;
                        }

                        {
                            //DbgOutString((std::wstring(L"add ") + std::to_wstring(peakFrame) + L" " +
                            //                                      std::to_wstring(sample) + L"\n").c_str());
                            m_peaks.emplace_back(peakFrame32, sample);
                            m_peaks.emplace_back(peakFrame32 + m_releaseFrames, m_limit);
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
        if (!m_peaks.empty())
        {
            DspChunk& chunk = m_buffer.front();
            assert(chunk.GetFormat() == DspFormat::Float);

            const uint32_t chunkFirstFrame32 = (uint32_t)m_bufferFirstFrame;
            const uint32_t channels = chunk.GetChannelCount();

            assert(m_peaks.size() > 1);
            auto left = m_peaks[0];
            auto right = m_peaks[1];
            float x = f_x(left, right);

            const size_t firstFrameOffset = OverflowingLess(left.first, chunkFirstFrame32) ?
                                                0 : (left.first - chunkFirstFrame32);

            auto data = reinterpret_cast<float*>(chunk.GetData());
            for (size_t i = firstFrameOffset, frameCount = chunk.GetFrameCount(); i < frameCount; i++)
            {
                const uint32_t frame32 = (uint32_t)(chunkFirstFrame32 + i);
                const float divisor = f(left, x, frame32);

                for (size_t channel = 0; channel < channels; channel++)
                {
                    float& sample = data[i * channels + channel];
                    sample = sample / divisor * m_limit;
                    assert(std::fabs(sample) <= m_limit);
                }

                if (!OverflowingLess(frame32, right.first))
                {
                    assert(right.first == frame32);
                    m_peaks.pop_front();
                    if (m_peaks.size() == 1)
                    {
                        //DbgOutString(L"clear\n");
                        m_peaks.clear();
                        break;
                    }
                    else
                    {
                        left = m_peaks[0];
                        right = m_peaks[1];
                        x = f_x(left, right);
                    }
                }
            }
        }
    }
}
