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
		// Set only when srcBuffer's data came from a resource upload allocation (see
		// RenderAllocationManager::RequestResourceUploadAllocation) - default-invalid otherwise
		// (e.g. frame upload requests, which reclaim in bulk and don't need this). Once this
		// request's copy is actually submitted, Renderer::DrainSubmissionCompletions signals this
		// id with the submission's timeline value so ResourceUploadAllocator can reclaim it once
		// the GPU catches up - see RenderAllocationManager::SignalResourceUpload.
		Handle resourceId;

		bool SameResources(const BufferUploadRequest& other) const noexcept
		{
			// Assumes both requests are valid
			return srcBuffer == other.srcBuffer &&
				dstBuffer == other.dstBuffer;
		}

		// For sorting
		bool operator<(const BufferUploadRequest& other) const noexcept
		{
			if (srcBuffer != other.srcBuffer)
				return srcBuffer.h.index < other.srcBuffer.h.index;
			return dstBuffer.h.index < other.dstBuffer.h.index;
		}
	};

	struct TextureUploadRequest
	{
		RenderBufferHandle srcBuffer;
		size_t srcOffset = 0;
		TextureHandle dstTexture;
		uint highestMip;
		uint mipCount;
		// Same purpose as BufferUploadRequest::resourceId - see its comment.
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