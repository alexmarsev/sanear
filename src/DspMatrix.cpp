#include "pch.h"
#include "DspMatrix.h"

namespace SaneAudioRenderer
{
    namespace
    {
        const std::map<DWORD, size_t> ChannelMap =
        {
            {SPEAKER_FRONT_LEFT, 0},
            {SPEAKER_FRONT_RIGHT, 1},
            {SPEAKER_FRONT_CENTER, 2},
            {SPEAKER_LOW_FREQUENCY, 3},
            {SPEAKER_BACK_LEFT, 4},
            {SPEAKER_BACK_RIGHT, 5},
            {SPEAKER_FRONT_LEFT_OF_CENTER, 6},
            {SPEAKER_FRONT_RIGHT_OF_CENTER, 7},
            {SPEAKER_BACK_CENTER, 8},
            {SPEAKER_SIDE_LEFT, 9},
            {SPEAKER_SIDE_RIGHT, 10},
            {SPEAKER_TOP_CENTER, 11},
            {SPEAKER_TOP_FRONT_LEFT, 12},
            {SPEAKER_TOP_FRONT_CENTER, 13},
            {SPEAKER_TOP_FRONT_RIGHT, 14},
            {SPEAKER_TOP_BACK_LEFT, 15},
            {SPEAKER_TOP_BACK_CENTER, 16},
            {SPEAKER_TOP_BACK_RIGHT, 17},
        };

        std::array<float, 18 * 18> BuildFullMatrix(DWORD inputMask, DWORD outputMask)
        {
            std::array<float, 18 * 18> matrix{};

            for (auto& p : ChannelMap)
            {
                if (inputMask & p.first)
                    matrix[18 * p.second + p.second] = 1.0f;
            }

            auto feed = [&](DWORD sourceChannel, DWORD targetChannel, float multiplier)
            {
                float* source = matrix.data() + 18 * ChannelMap.at(sourceChannel);
                float* target = matrix.data() + 18 * ChannelMap.at(targetChannel);
                for (int i = 0; i < 18; i++)
                    target[i] += source[i] * multiplier;
            };

            auto clear = [&](DWORD targetChannel)
            {
                float* target = matrix.data() + 18 * ChannelMap.at(targetChannel);
                for (int i = 0; i < 18; i++)
                    target[i] = 0.0f;
            };

            // Mix side
            {
                if (!(outputMask & SPEAKER_SIDE_LEFT))
                {
                    feed(SPEAKER_SIDE_LEFT, SPEAKER_BACK_LEFT, 1.0f);
                    clear(SPEAKER_SIDE_LEFT);
                }

                if (!(outputMask & SPEAKER_SIDE_RIGHT))
                {
                    feed(SPEAKER_SIDE_RIGHT, SPEAKER_BACK_RIGHT, 1.0f);
                    clear(SPEAKER_SIDE_RIGHT);
                }
            }

            // Mix back
            {
                if (!(outputMask & SPEAKER_BACK_CENTER))
                {
                    feed(SPEAKER_BACK_CENTER, SPEAKER_BACK_LEFT, 1.0f);
                    feed(SPEAKER_BACK_CENTER, SPEAKER_BACK_RIGHT, 1.0f);
                    clear(SPEAKER_BACK_CENTER);
                }

                if (!(outputMask & SPEAKER_BACK_LEFT))
                {
                    if (outputMask & SPEAKER_BACK_CENTER)
                    {
                        feed(SPEAKER_BACK_LEFT, SPEAKER_BACK_CENTER, 0.7071f);
                    }
                    else if (outputMask & SPEAKER_SIDE_LEFT)
                    {
                        feed(SPEAKER_BACK_LEFT, SPEAKER_SIDE_LEFT, 1.0f);
                    }
                    else
                    {
                        feed(SPEAKER_BACK_LEFT, SPEAKER_FRONT_LEFT, 0.7071f);
                    }

                    clear(SPEAKER_BACK_LEFT);
                }

                if (!(outputMask & SPEAKER_BACK_RIGHT))
                {
                    if (outputMask & SPEAKER_BACK_CENTER)
                    {
                        feed(SPEAKER_BACK_RIGHT, SPEAKER_BACK_CENTER, 0.7071f);
                    }
                    else if (outputMask & SPEAKER_SIDE_RIGHT)
                    {
                        feed(SPEAKER_BACK_RIGHT, SPEAKER_SIDE_RIGHT, 1.0f);
                    }
                    else
                    {
                        feed(SPEAKER_BACK_RIGHT, SPEAKER_FRONT_RIGHT, 0.7071f);
                    }

                    clear(SPEAKER_BACK_RIGHT);
                }
            }

            // Mix front
            {
                if (!(outputMask & SPEAKER_FRONT_CENTER))
                {
                    feed(SPEAKER_FRONT_CENTER, SPEAKER_FRONT_LEFT, 0.7071f);
                    feed(SPEAKER_FRONT_CENTER, SPEAKER_FRONT_RIGHT, 0.7071f);
                    clear(SPEAKER_FRONT_CENTER);
                }

                if (!(outputMask & SPEAKER_FRONT_LEFT) && (outputMask & SPEAKER_FRONT_CENTER))
                {
                    feed(SPEAKER_FRONT_LEFT, SPEAKER_FRONT_CENTER, 0.7071f);
                    clear(SPEAKER_FRONT_LEFT);
                }

                if (!(outputMask & SPEAKER_FRONT_RIGHT) && (outputMask & SPEAKER_FRONT_CENTER))
                {
                    feed(SPEAKER_FRONT_RIGHT, SPEAKER_FRONT_CENTER, 0.7071f);
                    clear(SPEAKER_FRONT_RIGHT);
                }
            }

            return matrix;
        }

        std::unique_ptr<float[]> BuildMatrix(size_t inputChannels, DWORD inputMask,
                                             size_t outputChannels, DWORD outputMask)
        {
            const auto fullMatrix = BuildFullMatrix(inputMask, outputMask);
            auto matrix = std::make_unique<float[]>(inputChannels * outputChannels);
            ZeroMemory(matrix.get(), sizeof(float) * inputChannels * outputChannels);

            size_t y = 0;
            for (auto& yp : ChannelMap)
            {
                if (outputMask & yp.first)
                {
                    size_t x = 0;
                    for (auto& xp : ChannelMap)
                    {
                        if (inputMask & xp.first)
                        {
                            matrix[y * inputChannels + x] = fullMatrix[yp.second * 18 + xp.second];

                            if (++x == inputChannels)
                                break;
                        }
                    }

                    if (++y == outputChannels)
                        break;
                }
            }

            return matrix;
        }

        template<size_t InputChannels, size_t OutputChannels>
        void Mix(const float* inputData, float* outputData, const float* matrix, size_t frames)
        {
            for (size_t frame = 0; frame < frames; frame++)
            {
                for (size_t y = 0; y < OutputChannels; y++)
                {
                    float d = 0.0f;

                    for (size_t x = 0; x < InputChannels; x++)
                    {
                        d += inputData[frame * InputChannels + x] * matrix[y * InputChannels + x];
                    }

                    outputData[frame * OutputChannels + y] = d;
                }
            }
        }

        void Mix(size_t inputChannels, const float* inputData, size_t outputChannels, float* outputData,
                 const float* matrix, size_t frames)
        {
            for (size_t frame = 0; frame < frames; frame++)
            {
                for (size_t y = 0; y < outputChannels; y++)
                {
                    float d = 0.0f;

                    for (size_t x = 0; x < inputChannels; x++)
                    {
                        d += inputData[frame * inputChannels + x] * matrix[y * inputChannels + x];
                    }

                    outputData[frame * outputChannels + y] = d;
                }
            }
        }
    }

    void DspMatrix::Initialize(uint32_t inputChannels, DWORD inputMask,
                               uint32_t outputChannels, DWORD outputMask)
    {
        m_matrix = nullptr;

        if (inputChannels != outputChannels || inputMask != outputMask)
            m_matrix = BuildMatrix(inputChannels, inputMask, outputChannels, outputMask);

        m_inputChannels = inputChannels;
        m_outputChannels = outputChannels;
    }

    void DspMatrix::Process(DspChunk& chunk)
    {
        if (m_matrix && !chunk.IsEmpty())
        {
            DspChunk::ToFloat(chunk);

            assert(chunk.GetFormat() == DspFormat::Float);
            assert(chunk.GetChannelCount() == m_inputChannels);

            DspChunk output(DspFormat::Float, m_outputChannels, chunk.GetFrameCount(), chunk.GetRate());

            const float* inputData = (const float*)chunk.GetConstData();
            float* outputData = (float*)output.GetData();

            if (m_inputChannels == 6 && m_outputChannels == 2)
            {
                Mix<6, 2>(inputData, outputData, m_matrix.get(), chunk.GetFrameCount());
            }
            else if (m_inputChannels == 7 && m_outputChannels == 2)
            {
                Mix<7, 2>(inputData, outputData, m_matrix.get(), chunk.GetFrameCount());
            }
            else if (m_inputChannels == 8 && m_outputChannels == 2)
            {
                Mix<8, 2>(inputData, outputData, m_matrix.get(), chunk.GetFrameCount());
            }
            else
            {
                Mix(m_inputChannels, inputData, m_outputChannels, outputData, m_matrix.get(), chunk.GetFrameCount());
            }

            chunk = std::move(output);
        }
    }

    void DspMatrix::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }

    DWORD DspMatrix::GetDefaultChannelMask(uint32_t channels)
    {
        switch (channels)
        {
            case 1:
                return KSAUDIO_SPEAKER_MONO;

            case 2:
                return KSAUDIO_SPEAKER_STEREO;

            case 3:
                return KSAUDIO_SPEAKER_STEREO | SPEAKER_FRONT_CENTER;

            case 4:
                return KSAUDIO_SPEAKER_SURROUND;

            case 5:
                return KSAUDIO_SPEAKER_QUAD | SPEAKER_FRONT_CENTER;

            case 6:
                return KSAUDIO_SPEAKER_5POINT1;

            case 7:
                return KSAUDIO_SPEAKER_5POINT1 | SPEAKER_BACK_CENTER;

            case 8:
                return KSAUDIO_SPEAKER_7POINT1;

            default:
                return 0;
        }
    }
}
