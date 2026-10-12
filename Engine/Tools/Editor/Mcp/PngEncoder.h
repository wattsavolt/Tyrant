#pragma once

#include "Core.h"

namespace tyr
{
	// Compresses 8-bit images to PNG, keeping its working buffers between images.
	class PngEncoder final
	{
	public:
		// Appends the PNG file's bytes to out. pixels are rows top to bottom, each width * channels
		// bytes, where channels is 3 for RGB or 4 for RGBA.
		void Encode(const uint8* pixels, uint width, uint height, uint channels, Array<uint8>& out);

	private:
		// Each row with whichever PNG filter leaves the smallest differences, its type byte first.
		void FilterRows(const uint8* pixels, uint width, uint height, uint channels);
		// A zlib stream of fixed-Huffman deflate data, appended to out.
		void Deflate(const uint8* data, uint size, Array<uint8>& out);

		Array<uint8> m_Filtered;
		Array<int> m_HashHeads;
		Array<int> m_HashChain;
	};
}
