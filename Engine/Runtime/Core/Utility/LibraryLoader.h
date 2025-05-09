/// Copyright (c) 2023 Aidan Clear 
#pragma once

#include "INonCopyable.h"
#include "Base/Base.h"

namespace tyr
{
	/// Class loads a dynamic library into memory, manages the pointer and frees on destruction.
	class TYR_CORE_EXPORT LibraryLoader final : INonCopyable
	{
	public:
		LibraryLoader();
		~LibraryLoader();

		bool Load(const char* name, bool addFileExtension = true);
		void Unload();
		void* GetProcessAddress(const char* function) const;
		bool IsLoaded() { return m_Handle; }

	private:
		Handle m_Handle;
	};
}
