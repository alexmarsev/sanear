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

        m_variable = variable;
        m_inputRate = inputRate;
        m_outputRate = outputRate;
        m_channels = channels;

        if (variable || inputRate != outputRate)
        {
            auto ioSpec = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
            auto qualitySpec = variable ? soxr_quality_spec(SOXR_HQ, SOXR_VR) : soxr_quality_spec(SOXR_VHQ, 0);
            m_soxr = soxr_create(variable ? inputRate * (m_maxVariableRateMultiplier + 0.1) : inputRate, outputRate,
                                 channels, nullptr, &ioSpec, &qualitySpec, nullptr);
        }
    }

    bool DspRate::Active()
    {
        return !!m_soxr;
    }

    void DspRate::Process(DspChunk& chunk)
    {
        if (m_soxr && !chunk.IsEmpty())
        {
            DspChunk::ToFloat(chunk);
            assert(chunk.GetRate() == m_inputRate);
            assert(chunk.GetChannelCount() == m_channels);

            size_t outputFrames = 2 * chunk.GetFrameCount() * m_outputRate / m_inputRate;

            if (m_variable)
            {
                double multiplier = 1.0;

                if (m_delta != 0)
                {
                    REFERENCE_TIME chunkTime = OneSecond * chunk.GetFrameCount() / m_inputRate;
                    multiplier = (double)chunkTime / (chunkTime - std::min(m_delta, chunkTime - 1));
                    multiplier = std::max(1 / m_maxVariableRateMultiplier,
                                          std::min(m_maxVariableRateMultiplier, multiplier));
                    m_delta -= (REFERENCE_TIME)(chunkTime - chunkTime / multiplier);
                }

                assert(multiplier > 0.0);
                soxr_set_io_ratio(m_soxr, m_inputRate * multiplier / m_outputRate, 0);
                if (multiplier < 1.0)
                    outputFrames = (size_t)(outputFrames / multiplier);
            }

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
        if (m_soxr)
        {
            Process(chunk);

            for (;;)
            {
                DspChunk output(DspFormat::Float, m_channels, chunk.GetFrameCount() + m_outputRate, m_outputRate);

                if (!chunk.IsEmpty())
                {
                    assert(output.GetFormat() == chunk.GetFormat());
                    assert(output.GetFrameSize() == chunk.GetFrameSize());
                    memcpy(output.GetData(), chunk.GetConstData(), chunk.GetSize());
                }

                size_t inputDone = 0;
                size_t outputDo = output.GetFrameCount() - chunk.GetFrameCount();
                size_t outputDone = 0;
                soxr_process(m_soxr, nullptr, 0, &inputDone, output.GetData() + chunk.GetSize(), outputDo, &outputDone);
                output.Shrink(outputDone);

                chunk = std::move(output);

                if (outputDone < outputDo)
                    break;
            }
        }
    }

    void DspRate::Adjust(REFERENCE_TIME delta)
    {
        m_delta += delta;
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
