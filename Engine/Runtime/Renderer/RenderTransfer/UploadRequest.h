#pragma once

#include "RenderBase/RenderHandles.h"

namespace tyr
{
	struct BufferUploadRequest
	{
		RenderBufferHandle srcBuffer;
		size_t srcOffset = 0;
		RenderBufferHandle dstBuffer;
		size_t dstOffset = 0;
		size_t size = 0;
		// Set only when srcBuffer's data came from a resource upload allocation - default-invalid
		// otherwise (e.g. frame upload requests, which reclaim in bulk and don't need this). Lets
		// that allocation be reclaimed once the GPU catches up to this request's submission.
		Handle resourceId;

		bool SameResources(const BufferUploadRequest& other) const noexcept
		{
			// Assumes both requests are valid
			return srcBuffer == other.srcBuffer &&
				dstBuffer == other.dstBuffer;
		}

		// For sorting - groups every request touching the same (srcBuffer, dstBuffer) pair
		// adjacently, so they can be batched into a single vkCmdCopyBuffer call. Compares full
		// handles (index and generation) on both buffers for a valid strict weak ordering even
		// across pool-slot reuse.
		bool operator<(const BufferUploadRequest& other) const noexcept
		{
			if (srcBuffer.h.index != other.srcBuffer.h.index)
				return srcBuffer.h.index < other.srcBuffer.h.index;
			if (srcBuffer.h.generation != other.srcBuffer.h.generation)
				return srcBuffer.h.generation < other.srcBuffer.h.generation;
			if (dstBuffer.h.index != other.dstBuffer.h.index)
				return dstBuffer.h.index < other.dstBuffer.h.index;
			return dstBuffer.h.generation < other.dstBuffer.h.generation;
		}
	};

	// What this upload's destination texture is used for - lets a pass tell whether it's the
	// sole intended consumer of a given upload, e.g. GUIPass only needs to declare reads for
	// GUI-type uploads.
	enum class TextureUploadRequestType : uint8
	{
		Material,
		GUI
	};

	struct TextureUploadRequest
	{
		RenderBufferHandle srcBuffer;
		size_t srcOffset = 0;
		TextureHandle dstTexture;
		uint highestMip;
		uint mipCount;
		TextureUploadRequestType type = TextureUploadRequestType::Material;
		// Set only when srcBuffer's data came from a resource upload allocation, so it can be
		// reclaimed once the GPU catches up to this request's submission.
		Handle resourceId;

		bool SameResources(const TextureUploadRequest& other) const noexcept
		{
			// Assumes both requests are valid
			return srcBuffer == other.srcBuffer &&
				dstTexture == other.dstTexture;
		}

		bool operator<(const TextureUploadRequest& other) const noexcept
		{
			if (srcBuffer.h.index != other.srcBuffer.h.index)
				return srcBuffer.h.index < other.srcBuffer.h.index;
			return dstTexture.h.index < other.dstTexture.h.index;
		}
	};

	struct FileToBufferUploadRequest
	{
		FileHandle srcFile;
		size_t srcOffset;
		Handle dstBuffer;
		size_t dstOffset;
		size_t size;
	};

	struct FileToTextureUploadRequest
	{
		FileHandle srcFile;
		size_t srcOffset;
		Handle dstBuffer;
		size_t dstOffset;
		size_t size;
	};
}