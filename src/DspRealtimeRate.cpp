#include "pch.h"
#include "DspRealtimeRate.h"

namespace SaneAudioRenderer
{
    void DspRealtimeRate::Initialize(bool realtime, uint32_t inputRate, uint32_t outputRate, uint32_t channels)
    {
        m_active = realtime;
        m_inputRate = inputRate;
        m_outputRate = outputRate;
        m_channels = channels;
        m_adjustTime = 0;
        m_inputFrames = 0;
        m_outputFrames = 0;

        if (realtime)
        {
            m_resampler.setup((double)outputRate / inputRate, channels,
                              std::min(inputRate, outputRate) > 44100 ? 32 : 48);

            // Insert silence to align input

            DspChunk silence(DspFormat::Float, channels, m_resampler.inpsize() / 2 - 1, outputRate);
            ZeroMemory(silence.GetData(), silence.GetSize());
            DspChunk temp(DspFormat::Float, channels, 1, outputRate);

            m_resampler.inp_count = (uint32_t)silence.GetFrameCount();
            m_resampler.out_count = 1;
            m_resampler.inp_data = (float*)silence.GetData();
            m_resampler.out_data = (float*)temp.GetData();

            m_resampler.process();
            assert(m_resampler.inp_count == 0);
            assert(m_resampler.out_count == 1);
        }
    }

    bool DspRealtimeRate::Active()
    {
        return m_active;
    }

    void DspRealtimeRate::Process(DspChunk& chunk)
    {
        if (!m_active || chunk.IsEmpty())
            return;

        assert(chunk.GetRate() == m_inputRate);
        assert(chunk.GetChannelCount() == m_channels);

        DspChunk::ToFloat(chunk);

        {
            REFERENCE_TIME adjustTime = m_adjustTime;

            if (m_inputFrames > m_resampler.inpsize() / 2)
            {
                int64_t adjustedFrames = llMulDiv(m_inputFrames - m_resampler.inpsize() / 2,
                                                  m_outputRate, m_inputRate, 0) - m_outputFrames;

                adjustTime += llMulDiv(adjustedFrames, OneSecond, m_outputRate, 0);
            }

            double ratio = (double)m_outputRate / m_inputRate;
            double adjustRatio = 1.0 + (double)adjustTime / OneSecond / ratio;

            // Stay within 5 cents.
            adjustRatio = std::min(1.003, std::max(0.997, adjustRatio));

            m_resampler.set_rratio(adjustRatio);
        }

        uint32_t outputFrames = (uint32_t)(2 * (uint64_t)chunk.GetFrameCount() * m_outputRate / m_inputRate);
        DspChunk output(DspFormat::Float, m_channels, outputFrames, m_outputRate);

        m_resampler.inp_count = (uint32_t)chunk.GetFrameCount();
        m_resampler.out_count = outputFrames;
        m_resampler.inp_data = (float*)chunk.GetData();
        m_resampler.out_data = (float*)output.GetData();

        m_resampler.process();

        assert(m_resampler.inp_count == 0);
        output.ShrinkTail(outputFrames - m_resampler.out_count);

        m_inputFrames += chunk.GetFrameCount();
        m_outputFrames += output.GetFrameCount();

        chunk = std::move(output);
    }

    void DspRealtimeRate::Finish(DspChunk& chunk)
    {
        if (!m_active)
            return;

        // Insert silence to align output
        if (!chunk.IsEmpty())
        {
            DspChunk::ToFloat(chunk);

            assert(chunk.GetRate() == m_inputRate);
            assert(chunk.GetChannelCount() == m_channels);

            DspChunk temp(DspFormat::Float, m_channels, chunk.GetFrameCount() + m_resampler.inpsize() / 2, m_inputRate);
            memcpy(temp.GetData(), chunk.GetData(), chunk.GetSize());
            ZeroMemory(temp.GetData() + chunk.GetSize(), temp.GetSize() - chunk.GetSize());

            chunk = std::move(temp);
        }
        else
        {
            chunk = DspChunk(DspFormat::Float, m_channels, m_resampler.inpsize() / 2, m_inputRate);
            ZeroMemory(chunk.GetData(), chunk.GetSize());
        }

        Process(chunk);
    }

    void DspRealtimeRate::Adjust(REFERENCE_TIME time)
    {
        m_adjustTime += time;
    }
}
