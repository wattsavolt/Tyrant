#pragma once

#include "Core.h"
#include <bit>

namespace tyr
{
	// A CPU copy of a GPU buffer of fixed slots, each with a dirty bit. A slot changed any number
	// of times in a frame is uploaded once with its latest record. Main thread only.
	template<typename T, uint N>
	class RenderRecordTable final
	{
	public:
		RenderRecordTable()
		{
			m_Records.Resize(N);
		}

		void Set(uint index, const T& record)
		{
			TYR_ASSERT(index < N);
			m_Records[index] = record;
			m_DirtyMask[index >> 6] |= static_cast<uint64>(1) << (index & 63);
		}

		// For a slot being freed, so its record isn't uploaded.
		void ClearDirty(uint index)
		{
			TYR_ASSERT(index < N);
			m_DirtyMask[index >> 6] &= ~(static_cast<uint64>(1) << (index & 63));
		}

		T& GetRecord(uint index)
		{
			return m_Records[index];
		}

		const T* GetRecords() const
		{
			return m_Records.Data();
		}

		// Calls upload(first, count) for each run of dirty slots, in slot order, then clears every
		// dirty bit.
		template<typename UploadFunction>
		void Flush(UploadFunction&& upload)
		{
			uint index = 0;
			while (index < N)
			{
				const uint first = FindNext(index, false);
				if (first >= N)
				{
					break;
				}
				const uint end = FindNext(first, true);
				upload(first, end - first);
				index = end;
			}
			memset(m_DirtyMask, 0, sizeof(m_DirtyMask));
		}

	private:
		static constexpr uint c_WordCount = (N + 63) / 64;

		// The first slot at or after index that's dirty, or clean when clean is set. N if none.
		uint FindNext(uint index, bool clean) const
		{
			uint word = index >> 6;
			const uint64 flip = clean ? ~static_cast<uint64>(0) : 0;
			uint64 bits = (m_DirtyMask[word] ^ flip) & (~static_cast<uint64>(0) << (index & 63));
			while (bits == 0)
			{
				if (++word == c_WordCount)
				{
					return N;
				}
				bits = m_DirtyMask[word] ^ flip;
			}
			const uint found = word * 64 + static_cast<uint>(std::countr_zero(bits));
			return found < N ? found : N;
		}

		Array<T> m_Records;
		uint64 m_DirtyMask[c_WordCount] = {};
	};
}
