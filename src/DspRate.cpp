#include "pch.h"
#include "DspRate.h"

namespace SaneAudioRenderer
{
    DspRate::~DspRate()
    {
        DestroyBackend();
    }

    void DspRate::Initialize(uint32_t inputRate, uint32_t outputRate, uint32_t channels)
    {
        DestroyBackend();

        m_inputRate = inputRate;
        m_outputRate = outputRate;
        m_channels = channels;

        if (inputRate != outputRate)
        {
            soxr_io_spec_t ioSpec{SOXR_FLOAT32_I, SOXR_FLOAT32_I, 1.0, nullptr, 0};
            soxr_quality_spec_t quality = soxr_quality_spec(SOXR_VHQ, 0);
            m_soxr = soxr_create(inputRate, outputRate, channels, nullptr, &ioSpec, &quality, nullptr);
        }
    }

    void DspRate::Process(DspChunk& chunk)
    {
        if (m_soxr && !chunk.IsEmpty())
        {
            DspChunk::ToFloat(chunk);

            assert(chunk.GetFormat() == DspFormat::Float);
            assert(chunk.GetRate() == m_inputRate);
            assert(chunk.GetChannelCount() == m_channels);

            size_t outputFrames = (size_t)((uint64_t)chunk.GetFrameCount() * 2 * m_outputRate / chunk.GetRate());
            DspChunk output(DspFormat::Float, chunk.GetChannelCount(), outputFrames, m_outputRate);

            size_t inputDone = 0;
            size_t outputDone = 0;
            soxr_process(m_soxr, chunk.GetConstData(), chunk.GetFrameCount(), &inputDone,
                                 output.GetData(), output.GetFrameCount(), &outputDone);
            assert(inputDone == chunk.GetFrameCount());
            output.Shrink(outputDone);

            chunk = std::move(output);
        }
    }

    void DspRate::Finish(DspChunk& chunk)
    {
        Process(chunk);

        size_t delay = (size_t)soxr_delay(m_soxr);

        if (delay > 0)
        {
            DspChunk output(DspFormat::Float, m_channels, chunk.GetFrameCount() + delay, m_outputRate);

            if (!chunk.IsEmpty())
                memcpy(output.GetData(), chunk.GetConstData(), chunk.GetSize());

            size_t inputDone = 0;
            size_t outputDone = 0;
            soxr_process(m_soxr, nullptr, 0, &inputDone,
                                 output.GetData() + chunk.GetSize(), output.GetFrameCount() - chunk.GetFrameCount(), &outputDone);
            assert(outputDone == delay);
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
