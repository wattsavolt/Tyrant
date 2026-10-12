#include "PngEncoder.h"
#include <cstdlib>
#include <cstring>

namespace tyr
{
	namespace
	{
		constexpr uint c_WindowSize = 32768;
		constexpr uint c_MinMatch = 3;
		constexpr uint c_MaxMatch = 258;
		constexpr uint c_HashBits = 15;
		// How many earlier matches are tried at each position - more compresses better but slower.
		constexpr uint c_MaxChainLength = 32;

		const uint16 c_LengthBases[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
		const uint8 c_LengthExtraBits[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
		const uint16 c_DistanceBases[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
		const uint8 c_DistanceExtraBits[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

		uint ReverseBits(uint code, uint bitCount)
		{
			uint reversed = 0;
			for (uint i = 0; i < bitCount; ++i)
			{
				reversed = (reversed << 1) | ((code >> i) & 1);
			}
			return reversed;
		}

		// Deflate's bits go out least significant first, except Huffman codes, which are reversed.
		class BitWriter
		{
		public:
			explicit BitWriter(Array<uint8>& out) : m_Out(out) {}

			void Write(uint bits, uint bitCount)
			{
				m_Bits |= (uint64)bits << m_BitCount;
				m_BitCount += bitCount;
				while (m_BitCount >= 8)
				{
					m_Out.Add(static_cast<uint8>(m_Bits));
					m_Bits >>= 8;
					m_BitCount -= 8;
				}
			}

			void WriteCode(uint code, uint bitCount) { Write(ReverseBits(code, bitCount), bitCount); }

			void Flush()
			{
				if (m_BitCount > 0)
				{
					m_Out.Add(static_cast<uint8>(m_Bits));
				}
				m_Bits = 0;
				m_BitCount = 0;
			}

		private:
			Array<uint8>& m_Out;
			uint64 m_Bits = 0;
			uint m_BitCount = 0;
		};

		// The fixed Huffman code for a literal or length symbol (0-287).
		void WriteLiteralOrLength(BitWriter& writer, uint symbol)
		{
			if (symbol < 144)
			{
				writer.WriteCode(0x30 + symbol, 8);
			}
			else if (symbol < 256)
			{
				writer.WriteCode(0x190 + symbol - 144, 9);
			}
			else if (symbol < 280)
			{
				writer.WriteCode(symbol - 256, 7);
			}
			else
			{
				writer.WriteCode(0xC0 + symbol - 280, 8);
			}
		}

		void WriteMatch(BitWriter& writer, uint length, uint distance)
		{
			uint lengthCode = 28;
			while (c_LengthBases[lengthCode] > length)
			{
				--lengthCode;
			}
			WriteLiteralOrLength(writer, 257 + lengthCode);
			writer.Write(length - c_LengthBases[lengthCode], c_LengthExtraBits[lengthCode]);

			uint distanceCode = 29;
			while (c_DistanceBases[distanceCode] > distance)
			{
				--distanceCode;
			}
			writer.WriteCode(distanceCode, 5);
			writer.Write(distance - c_DistanceBases[distanceCode], c_DistanceExtraBits[distanceCode]);
		}

		uint Hash(const uint8* data)
		{
			const uint value = (uint)data[0] << 16 | (uint)data[1] << 8 | data[2];
			return (value * 2654435761u) >> (32 - c_HashBits);
		}

		uint Crc32(const uint8* data, size_t size, uint crc = 0)
		{
			static uint s_Table[256];
			static bool s_TableBuilt = false;
			if (!s_TableBuilt)
			{
				for (uint i = 0; i < 256; ++i)
				{
					uint c = i;
					for (uint k = 0; k < 8; ++k)
					{
						c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
					}
					s_Table[i] = c;
				}
				s_TableBuilt = true;
			}

			crc = ~crc;
			for (size_t i = 0; i < size; ++i)
			{
				crc = s_Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
			}
			return ~crc;
		}

		void AppendBigEndian(Array<uint8>& out, uint value)
		{
			out.Add(static_cast<uint8>(value >> 24));
			out.Add(static_cast<uint8>(value >> 16));
			out.Add(static_cast<uint8>(value >> 8));
			out.Add(static_cast<uint8>(value));
		}

		// Appends a chunk's length, type, data and CRC. The data is written by writeData.
		template<typename WriteData>
		void AppendChunk(Array<uint8>& out, const char* type, WriteData writeData)
		{
			const uint lengthOffset = out.Size();
			AppendBigEndian(out, 0);
			const uint typeOffset = out.Size();
			out.Add(static_cast<uint8>(type[0]));
			out.Add(static_cast<uint8>(type[1]));
			out.Add(static_cast<uint8>(type[2]));
			out.Add(static_cast<uint8>(type[3]));
			writeData();

			const uint dataSize = out.Size() - typeOffset - 4;
			out[lengthOffset] = static_cast<uint8>(dataSize >> 24);
			out[lengthOffset + 1] = static_cast<uint8>(dataSize >> 16);
			out[lengthOffset + 2] = static_cast<uint8>(dataSize >> 8);
			out[lengthOffset + 3] = static_cast<uint8>(dataSize);
			AppendBigEndian(out, Crc32(out.Data() + typeOffset, dataSize + 4));
		}

		uint8 Paeth(int left, int up, int upLeft)
		{
			const int estimate = left + up - upLeft;
			const int toLeft = abs(estimate - left);
			const int toUp = abs(estimate - up);
			const int toUpLeft = abs(estimate - upLeft);
			return static_cast<uint8>(toLeft <= toUp && toLeft <= toUpLeft ? left : (toUp <= toUpLeft ? up : upLeft));
		}
	}

	void PngEncoder::FilterRows(const uint8* pixels, uint width, uint height, uint channels)
	{
		const uint rowSize = width * channels;
		m_Filtered.Resize(height * (rowSize + 1));

		for (uint y = 0; y < height; ++y)
		{
			const uint8* row = pixels + (size_t)y * rowSize;
			const uint8* above = y > 0 ? row - rowSize : nullptr;
			uint8* out = m_Filtered.Data() + (size_t)y * (rowSize + 1);

			// Filters 0 to 4: none, left, up, average and Paeth. The one with the smallest sum of
			// differences usually compresses best.
			uint bestFilter = 0;
			uint bestCost = ~0u;
			for (uint filter = 0; filter < 5; ++filter)
			{
				uint cost = 0;
				for (uint i = 0; i < rowSize; ++i)
				{
					const int left = i >= channels ? row[i - channels] : 0;
					const int up = above ? above[i] : 0;
					const int upLeft = above && i >= channels ? above[i - channels] : 0;
					const int predicted = filter == 0 ? 0 : filter == 1 ? left : filter == 2 ? up : filter == 3 ? (left + up) / 2 : Paeth(left, up, upLeft);
					cost += (uint)abs(static_cast<int8>(static_cast<uint8>(row[i] - predicted)));
				}
				if (cost < bestCost)
				{
					bestCost = cost;
					bestFilter = filter;
				}
			}

			out[0] = static_cast<uint8>(bestFilter);
			for (uint i = 0; i < rowSize; ++i)
			{
				const int left = i >= channels ? row[i - channels] : 0;
				const int up = above ? above[i] : 0;
				const int upLeft = above && i >= channels ? above[i - channels] : 0;
				const int predicted = bestFilter == 0 ? 0 : bestFilter == 1 ? left : bestFilter == 2 ? up : bestFilter == 3 ? (left + up) / 2 : Paeth(left, up, upLeft);
				out[i + 1] = static_cast<uint8>(row[i] - predicted);
			}
		}
	}

	void PngEncoder::Deflate(const uint8* data, uint size, Array<uint8>& out)
	{
		// zlib header: deflate with a 32 KB window, no preset dictionary.
		out.Add(0x78);
		out.Add(0x01);

		m_HashHeads.Resize(1u << c_HashBits);
		m_HashChain.Resize(c_WindowSize);
		for (int& head : m_HashHeads)
		{
			head = -1;
		}

		BitWriter writer(out);
		// One final block of fixed Huffman codes.
		writer.Write(1, 1);
		writer.Write(1, 2);

		uint position = 0;
		while (position < size)
		{
			uint bestLength = 0;
			uint bestDistance = 0;
			if (position + c_MinMatch <= size)
			{
				const uint hash = Hash(data + position);
				const uint maxLength = size - position < c_MaxMatch ? size - position : c_MaxMatch;
				int candidate = m_HashHeads[hash];
				for (uint chain = 0; candidate >= 0 && chain < c_MaxChainLength; ++chain)
				{
					const uint distance = position - (uint)candidate;
					if (distance > c_WindowSize - 1)
					{
						break;
					}
					uint length = 0;
					while (length < maxLength && data[candidate + length] == data[position + length])
					{
						++length;
					}
					if (length > bestLength)
					{
						bestLength = length;
						bestDistance = distance;
						if (length == maxLength)
						{
							break;
						}
					}
					candidate = m_HashChain[candidate % c_WindowSize];
				}
			}

			// Every position passed over goes into the hash chains, so later matches can find it.
			const uint advance = bestLength >= c_MinMatch ? bestLength : 1;
			for (uint i = 0; i < advance && position + i + c_MinMatch <= size; ++i)
			{
				const uint hash = Hash(data + position + i);
				m_HashChain[(position + i) % c_WindowSize] = m_HashHeads[hash];
				m_HashHeads[hash] = (int)(position + i);
			}

			if (bestLength >= c_MinMatch)
			{
				WriteMatch(writer, bestLength, bestDistance);
			}
			else
			{
				WriteLiteralOrLength(writer, data[position]);
			}
			position += advance;
		}
		WriteLiteralOrLength(writer, 256);
		writer.Flush();

		// Adler-32 of the uncompressed data.
		uint a = 1;
		uint b = 0;
		for (uint i = 0; i < size; ++i)
		{
			a = (a + data[i]) % 65521;
			b = (b + a) % 65521;
		}
		AppendBigEndian(out, (b << 16) | a);
	}

	void PngEncoder::Encode(const uint8* pixels, uint width, uint height, uint channels, Array<uint8>& out)
	{
		TYR_ASSERT(channels == 3 || channels == 4);
		static const uint8 c_Signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
		for (uint8 byte : c_Signature)
		{
			out.Add(byte);
		}

		AppendChunk(out, "IHDR", [&]()
		{
			AppendBigEndian(out, width);
			AppendBigEndian(out, height);
			out.Add(8);
			// Colour type 2 is RGB and 6 is RGBA.
			out.Add(channels == 4 ? 6 : 2);
			out.Add(0);
			out.Add(0);
			out.Add(0);
		});

		FilterRows(pixels, width, height, channels);
		AppendChunk(out, "IDAT", [&]() { Deflate(m_Filtered.Data(), m_Filtered.Size(), out); });
		AppendChunk(out, "IEND", []() {});
	}
}
