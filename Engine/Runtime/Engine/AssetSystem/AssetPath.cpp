#include "AssetPath.h"
#include "Memory/Memory.h"
#include "Reflection/Reflection.h"
#include <cstring>

namespace tyr
{
	TYR_REFL_CLASS_START(AssetPath, 0);
		TYR_REFL_ARRAY_FIELD(&AssetPath::m_Size, &AssetPath::m_Data, "Path", true, true, true);
	TYR_REFL_CLASS_END();

	AssetPath::AssetPath()
	{
		CreateEmpty();
	}

	AssetPath::AssetPath(const char* data)
	{
		CreateEmpty();
		Copy(data);
	}

	AssetPath::AssetPath(const AssetPath& other)
		: AssetPath(other.m_Data)
	{
	}

	AssetPath::~AssetPath()
	{

	}

	AssetPath& AssetPath::operator=(const char* data)
	{
		if (m_Data != data)
		{
			Copy(data);
		}
		return *this;
	}

	AssetPath& AssetPath::operator=(const AssetPath& other)
	{
		*this = other.m_Data;
		return *this;
	}

	bool AssetPath::operator==(const AssetPath& val)
	{
		// 0 if they are equal
		return strcmp(m_Data, val.m_Data) == 0;
	}

	bool AssetPath::operator==(const char* data)
	{
		if (data == nullptr)
		{
			return false;
		}
		return strcmp(m_Data, data) == 0;
	}

	void AssetPath::Copy(const char* data)
	{
		m_Size = strlen(data) + 1;
		TYR_ASSERT(m_Size <= c_MaxAssetPathDataSize);
		strcpy_s(m_Data, m_Size, data);	
	}

	void AssetPath::CreateEmpty()
	{
		m_Size = 1;
		m_Data[0] = '\0';
	}
}