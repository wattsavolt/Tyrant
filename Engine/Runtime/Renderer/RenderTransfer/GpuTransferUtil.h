#pragma once

#include "RendererMacros.h"
#include "Core.h"

namespace tyr
{
	struct BufferUploadRequest;
	struct TextureUploadRequest;
	struct FileToBufferUploadRequest;
	struct FileToTextureUploadRequest;
	class CommandList;

	/// Class that provides utility functions for transfers from file / upload buffer to GPU buffer / texture.
	class TYR_RENDERER_API GpuTransferUtil final
	{
	public:
		static constexpr uint c_MaxUploadRegionsPerBuffer = 256;

		// Issues one copy command for requests that have same source buffer and destination buffer
		static void UploadToBuffers(CommandList& commandList, BufferUploadRequest* requests, uint count);
		// Expects only one request references the same texture 
		static void UploadToTextures(CommandList& commandList, const TextureUploadRequest* requests, uint count);
		static void FileToBuffer(CommandList& commandList, const FileToBufferUploadRequest& request);
		static void FileToTexture(CommandList& commandList, const FileToTextureUploadRequest& request);
	};
}
