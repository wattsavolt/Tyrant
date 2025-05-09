/// Copyright (c) 2023 Aidan Clear 

#include "LibraryLoader.h"
#include "Platform/Platform.h"

namespace tyr
{
	LibraryLoader::LibraryLoader()
		: m_Handle(nullptr)
	{
		
	}

	LibraryLoader::~LibraryLoader()
	{
		Unload();
	}

	bool LibraryLoader::Load(const char* name, bool addFileExtension)
	{	
		Unload();
		m_Handle = Platform::OpenLibrary(name, addFileExtension);
		return m_Handle != nullptr;
	}

	void LibraryLoader::Unload()
	{
		if (m_Handle)
		{
			Platform::CloseLibrary(m_Handle);
			m_Handle = nullptr;
		}
	}

	void* LibraryLoader::GetProcessAddress(const char* function) const
	{
		if (!m_Handle)
		{
			return nullptr;
		}
		return Platform::GetProcessAddress(m_Handle, function);
	}
}
