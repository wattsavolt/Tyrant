#include "ImageCompressor.h"
#include "Core.h"
#include "nvtt.h"
#include "AssetSystem/TextureAsset.h"
#include "AssetSystem/AssetUtil.h"
#include "ImageUtil.h"
#include "Rendering/RenderConstants.h"

namespace tyr
{
    static Array<uint8>& GetThreadLocalBuffer()
    {
        static TYR_THREADLOCAL Array<uint8> buffer;
        return buffer;
    }

    // Custom MemoryOutputHandler uses the thread-local static buffer. Data is not gpu ready
    class MemoryOutputHandler : public nvtt::OutputHandler
    {
    public:
        MemoryOutputHandler()
        {
            m_Data = &GetThreadLocalBuffer();
            m_Data->Clear();
        }

        void beginImage(int size, int width, int height, int depth, int face, int miplevel) override
        {
            m_Data->Reserve(m_Data->Size() + size);
        }

        bool writeData(const void* data, int size) override
        {
            const uint8* bytes = reinterpret_cast<const uint8*>(data);
            m_Data->Insert(m_Data->Size(), bytes, static_cast<uint>(size));
            return true;
        }

        void endImage() override {}

        const void* getData() const { return m_Data->Data(); }
        size_t getDataSize() const { return m_Data->Size(); }

    private:
        Array<uint8>* m_Data = nullptr;
    };

    // Important Note: Good enough for textures up to 16K × 16K megatextures but if bigger textures than this are needed, endImage should be changed to write the current data to a file
    // to save RAM and Array has capacity limit of UINT2_MAX
    class GpuReadyMemoryOutputHandler : public nvtt::OutputHandler
    {
    public:
        static constexpr uint c_MaxRowAlignment = RenderConstants::c_RowPitchAlignment;

        GpuReadyMemoryOutputHandler(uint blockSize = 16, uint rowAlignment = RenderConstants::c_RowPitchAlignment, uint mipAlignment = RenderConstants::c_UploadAlignment)
            : m_BlockSize(blockSize)
            , m_RowAlignment(rowAlignment)
            , m_MipAlignment(mipAlignment)
            , m_BlockRows(0)
            , m_RowBytes(0)
            , m_AlignedRowBytes(0)
            , m_CurrentOffset(0)
            , m_MipOffset(0)
            , m_WriteIndex(0)
            , m_RowCursor(0)
            , m_CurrentRow(0)
        {
            m_Data = &GetThreadLocalBuffer();
            m_Data->Clear();
        }

        void beginImage(int /*size*/, int width, int height, int /*depth*/, int /*face*/, int /*miplevel*/) override
        {
            const uint blocksX = (width + 3) / 4;
            m_BlockRows = (height + 3) / 4;

            m_RowBytes = blocksX * m_BlockSize;
            m_AlignedRowBytes = MemoryUtil::Align(m_RowBytes, m_RowAlignment);

            m_MipOffset = MemoryUtil::Align<size_t>(m_CurrentOffset, m_MipAlignment);

            const uint mipSize = m_AlignedRowBytes * m_BlockRows;

            m_Data->Reserve(m_MipOffset + mipSize);

            m_WriteIndex = m_MipOffset;
            m_RowCursor = 0;
            m_CurrentRow = 0;
        }

        bool writeData(const void* data, int size) override
        {
            const uint8* src = reinterpret_cast<const uint8*>(data);
            uint remaining = size;

            uint8 pad[c_MaxRowAlignment] = {};

            while (remaining > 0)
            {
                const uint spaceInRow = m_RowBytes - m_RowCursor;
                const uint toCopy = std::min(spaceInRow, remaining);

                m_Data->Insert((uint)m_WriteIndex, src, toCopy);

                src += toCopy;
                remaining -= toCopy;

                m_WriteIndex += toCopy;
                m_RowCursor += toCopy;

                if (m_RowCursor == m_RowBytes)
                {
                    // Pad row
                    const uint padBytes = m_AlignedRowBytes - m_RowBytes;
                    if (padBytes > 0)
                    {
                        m_Data->Insert((uint)m_WriteIndex, pad, padBytes);
                        m_WriteIndex += padBytes;
                    }

                    m_RowCursor = 0;
                    ++m_CurrentRow;

                    if (m_CurrentRow == m_BlockRows)
                        break;
                }
            }

            return true;
        }

        void endImage() override
        {
            m_CurrentOffset = m_WriteIndex;
        }

        const void* getData() const { return m_Data->Data(); }
        size_t getDataSize() const { return m_CurrentOffset; }

    private:
        Array<uint8>* m_Data = nullptr;

        uint m_BlockSize;
        uint m_RowAlignment;
        uint m_MipAlignment;

        uint m_BlockRows;
        uint m_RowBytes;
        uint m_AlignedRowBytes;

        size_t m_CurrentOffset;
        size_t m_MipOffset;
        size_t m_WriteIndex;

        uint m_RowCursor;
        uint m_CurrentRow;
    };

    nvtt::InputFormat ToNvttInputFormat(ImageCompressionInputFormat format)
    {
        switch (format)
        {
        case ImageCompressionInputFormat::RGBA_8U:  return nvtt::InputFormat_BGRA_8UB;
        case ImageCompressionInputFormat::RGBA_16F: return nvtt::InputFormat_RGBA_16F;
        case ImageCompressionInputFormat::RGBA_32F: return nvtt::InputFormat_RGBA_32F;
        default: TYR_ASSERT(false);
        }
        return nvtt::InputFormat_BGRA_8UB;
    }

    nvtt::Format ToNvttOutputFormat(ImageCompressionOutputFormat format)
    {
        switch (format)
        {
        case ImageCompressionOutputFormat::BC3: return nvtt::Format_BC3;
        case ImageCompressionOutputFormat::BC5: return nvtt::Format_BC5;
        case ImageCompressionOutputFormat::BC7: return nvtt::Format_BC7;
        default: TYR_ASSERT(false);
        }
        return nvtt::Format_BC7;
    }

    PixelFormat ToOutputPixelFormat(ImageCompressionOutputFormat format, bool isSRGB = true)
    {
        switch (format)
        {
        case ImageCompressionOutputFormat::BC3: return isSRGB ? PF_BC3_SRGB : PF_BC3_UNORM;
        case ImageCompressionOutputFormat::BC5: return PF_BC5_UNORM;
        case ImageCompressionOutputFormat::BC7: return isSRGB ? PF_BC7_SRGB : PF_BC7_UNORM;
        default: TYR_ASSERT(false);
        }
        return PF_BC3_SRGB;
    }

    static void SerializeCompressedImage(const TextureInfo& textureInfo, const char* filePath)
    {
        TextureHeader metadata;
  
        const Array<uint8>& buffer = GetThreadLocalBuffer();
        metadata.dataSize = buffer.Size();

        metadata.info = textureInfo;

        char absFilePath[TYR_MAX_PATH_TOTAL_SIZE];
        AssetUtil::CreateFullPath(absFilePath, filePath);

        Serializer::Instance().SerializeToFile<TextureHeader>(absFilePath, metadata);
        FileStream::WriteFile(absFilePath, buffer.Data(), metadata.dataSize, false);
    }

    bool ImageCompressor::CompressImage2D(const Image2DCompressionDesc& desc)
    {
        TYR_ASSERT(desc.mipCount > 0);

        const nvtt::InputFormat inputFormat = ToNvttInputFormat(desc.inputFormat);
        if (inputFormat == nvtt::InputFormat_BGRA_8UB)
        {
            // Swap the R and G
            const uint texelCount = desc.width * desc.height;
            uint8* image = static_cast<uint8*>(desc.image);
            ImageUtil::SwapChannels<uint8>(image, texelCount, 4, 0, 2);
        }

        nvtt::Surface surface;
        if (!surface.setImage(ToNvttInputFormat(desc.inputFormat),
            static_cast<int>(desc.width), static_cast<int>(desc.height), 1, desc.image))
        {
            return false;
        }

        nvtt::Context context;
        nvtt::OutputOptions outputOptions;
        nvtt::CompressionOptions compressionOptions;

        outputOptions.setOutputHeader(false);

        compressionOptions.setFormat(ToNvttOutputFormat(desc.outputFormat));

        GpuReadyMemoryOutputHandler outputHandler;
        outputOptions.setOutputHandler(&outputHandler);

        for (uint8 mip = 0; mip < desc.mipCount; ++mip)
        {
            const bool success = context.compress(surface, 0, mip, compressionOptions, outputOptions);
            if (!success)
            {
                return false;
            }

            if (mip + 1 < desc.mipCount)
            {
                if (!surface.buildNextMipmap(nvtt::MipmapFilter_Box))
                {
                    break;
                }
            }
        }

        TextureInfo textureInfo;
        textureInfo.width = desc.width;
        textureInfo.height = desc.height;
        textureInfo.depth = 1;
        textureInfo.mipCount = desc.mipCount;
        textureInfo.type = ImageType::Image2D;
        textureInfo.format = ToOutputPixelFormat(desc.outputFormat, desc.isSRGB);

        SerializeCompressedImage(textureInfo, desc.outputFilePath);

        return true;
    }

    bool ImageCompressor::CompressCubemap(const CubemapCompressionDesc& desc)
    {
        nvtt::CubeSurface cubeSurface;
        if (!cubeSurface.load(desc.inputFilePath, desc.mip))
        {
            return false;
        }

        nvtt::Context context;
        nvtt::OutputOptions outputOptions;
        nvtt::CompressionOptions compressionOptions;

        outputOptions.setOutputHeader(false);

        GpuReadyMemoryOutputHandler outputHandler;
        outputOptions.setOutputHandler(&outputHandler);

        compressionOptions.setFormat(ToNvttOutputFormat(desc.outputFormat));
        const bool success = context.compress(cubeSurface, desc.mip, compressionOptions, outputOptions);
        if (!success)
        {
            return false;
        }

        const nvtt::Surface& surface = cubeSurface.face(0);
        TextureInfo textureInfo;
        textureInfo.width = surface.width();
        textureInfo.height = surface.height();
        textureInfo.depth = 1;
        textureInfo.mipCount = 1;
        textureInfo.type = ImageType::Cubemap;
        textureInfo.format = ToOutputPixelFormat(desc.outputFormat, desc.isSRGB);

        SerializeCompressedImage(textureInfo, desc.outputFilePath);

        return true;
    }
} 

	