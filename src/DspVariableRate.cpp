#include "pch.h"
#include "DspVariableRate.h"

namespace SaneAudioRenderer
{
    void DspVariableRate::Initialize(bool variable, uint32_t inputRate, uint32_t outputRate, uint32_t channels)
    {
        m_active = variable;
        m_inputRate = inputRate;
        m_outputRate = outputRate;
        m_channels = channels;

        if (variable)
        {
            m_resampler.setup((double)outputRate / inputRate, channels, 32);
            DebugOut("DspVariableRate filter window is", m_resampler.inpsize(), "frames");
        }
    }

    bool DspVariableRate::Active()
    {
        return m_active;
    }

    void DspVariableRate::Process(DspChunk& chunk)
    {
        if (!m_active || chunk.IsEmpty())
            return;

        assert(chunk.GetRate() == m_inputRate);
        assert(chunk.GetChannelCount() == m_channels);

        DspChunk::ToFloat(chunk);

        // TODO: pad to align

        uint32_t outputFrames = (uint32_t)(2 * (uint64_t)chunk.GetFrameCount() * m_outputRate / m_inputRate);
        DspChunk output(DspFormat::Float, chunk.GetChannelCount(), outputFrames, m_outputRate);

        m_resampler.inp_count = (uint32_t)chunk.GetFrameCount();
        m_resampler.out_count = outputFrames;
        m_resampler.inp_data = (float*)chunk.GetData();
        m_resampler.out_data = (float*)output.GetData();

        m_resampler.process();

        assert(m_resampler.inp_count == 0);
        output.ShrinkTail(outputFrames - m_resampler.out_count);

        chunk = std::move(output);
    }

    void DspVariableRate::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }
}
