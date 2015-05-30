#include "pch.h"
#include "DspRate.h"

namespace SaneAudioRenderer
{
    DspRate::~DspRate()
    {
        DestroyBackend();
    }

    void DspRate::Initialize(bool variable, uint32_t inputRate, uint32_t outputRate, uint32_t channels)
    {
        DestroyBackend();

        m_inputRate = inputRate;
        m_outputRate = outputRate;
        m_channels = channels;

        if (!variable && inputRate != outputRate)
        {
            auto ioSpec = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
            auto qualitySpec = soxr_quality_spec(SOXR_HQ, 0);
            m_soxr = soxr_create(inputRate, outputRate, channels, nullptr, &ioSpec, &qualitySpec, nullptr);
        }
    }

    bool DspRate::Active()
    {
        return !!m_soxr;
    }

    void DspRate::Process(DspChunk& chunk)
    {
        if (!m_soxr || chunk.IsEmpty())
            return;

        assert(chunk.GetRate() == m_inputRate);
        assert(chunk.GetChannelCount() == m_channels);

        DspChunk::ToFloat(chunk);

        size_t outputFrames = (size_t)(2 * (uint64_t)chunk.GetFrameCount() * m_outputRate / m_inputRate);
        DspChunk output(DspFormat::Float, chunk.GetChannelCount(), outputFrames, m_outputRate);

        size_t inputDone = 0;
        size_t outputDone = 0;
        soxr_process(m_soxr, chunk.GetConstData(), chunk.GetFrameCount(), &inputDone,
                             output.GetData(), output.GetFrameCount(), &outputDone);
        assert(inputDone == chunk.GetFrameCount());
        output.ShrinkTail(outputDone);

        chunk = std::move(output);
    }

    void DspRate::Finish(DspChunk& chunk)
    {
        if (!m_soxr)
            return;

        Process(chunk);

        for (;;)
        {
            DspChunk output(DspFormat::Float, m_channels, chunk.GetFrameCount() + m_outputRate, m_outputRate);

            if (!chunk.IsEmpty())
                memcpy(output.GetData(), chunk.GetConstData(), chunk.GetSize());

            size_t inputDone = 0;
            size_t outputDo = output.GetFrameCount() - chunk.GetFrameCount();
            size_t outputDone = 0;
            soxr_process(m_soxr, nullptr, 0, &inputDone, output.GetData() + chunk.GetSize(), outputDo, &outputDone);
            output.ShrinkTail(outputDone);

            chunk = std::move(output);

            if (outputDone < outputDo)
                break;
        }
    }

    void DspRate::DestroyBackend()
    {
        if (m_soxr)
        {
            soxr_delete(m_soxr);
            m_soxr = nullptr;
        }
    }
}
