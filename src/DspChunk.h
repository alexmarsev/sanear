#pragma once

#include "DspFormat.h"

namespace SaneAudioRenderer
{
    class DspChunk final
    {
    public:

        static void ToFormat(DspFormat format, DspChunk& chunk);
        static void ToFloat(DspChunk& chunk) { ToFormat(DspFormat::Float, chunk); }

        DspChunk();
        DspChunk(DspFormat format, uint32_t channels, size_t frames, uint32_t rate);
        DspChunk(IMediaSample* pSample, const AM_SAMPLE2_PROPERTIES& sampleProps, const WAVEFORMATEX& sampleFormat);
        DspChunk(DspChunk&& other);
        DspChunk& operator=(DspChunk&& other);

        bool IsEmpty()             const { return m_dataSize == 0; }

        DspFormat GetFormat()      const { return m_format; }
        uint32_t GetFormatSize()   const { return DspFormatSize(m_format); }
        uint32_t GetChannelCount() const { return m_channels; }
        uint32_t GetFrameSize()    const { return GetFormatSize() * m_channels; }
        uint32_t GetRate()         const { return m_rate; }

        size_t GetSize()           const { return m_dataSize; }
        size_t GetSampleCount()    const { assert(GetFormatSize() != 0); return m_dataSize / GetFormatSize(); }
        size_t GetFrameCount()     const { assert(m_channels != 0); return GetSampleCount() / m_channels; }

        const char* GetConstData() const { return m_delayedCopy ? m_constData : m_data.get(); }

        char* GetData();
        void Shrink(size_t toFrames);

    private:

        void Allocate();
        void InvokeDelayedCopy();

        IMediaSamplePtr m_mediaSample;

        DspFormat m_format;
        uint32_t m_channels;
        uint32_t m_rate;

        size_t m_dataSize;
        const char* m_constData;
        bool m_delayedCopy;
        std::unique_ptr<char[], AlignedFreeDeleter> m_data;
    };
}