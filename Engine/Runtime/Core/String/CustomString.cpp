

#include "CustomString.h"
#include "Memory/Memory.h"
#include <cstring>

namespace tyr
{
	CString::CString()
	{
		CreateEmpty();
	}

	CString::CString(const char* data)
	{
		Copy(data);
	}

	CString::CString(const CString& val)
	{
		m_Size = val.m_Size;
		m_Data = new char[m_Size];
		strcpy_s(m_Data, m_Size, val.m_Data);
	}

	CString::CString(CString&& val) noexcept
	{
		m_Data = val.m_Data;
		m_Size = val.m_Size;
		val.m_Data = nullptr;
		val.m_Size = 0;
	}

	CString::~CString()
	{
		Free(m_Data);
	}

	CString& CString::operator=(const CString& val)
	{
		m_Size = val.m_Size;
		m_Data = new char[m_Size];
		strcpy_s(m_Data, val.m_Size, val.m_Data);
		return *this;
	}

	CString& CString::operator=(const char* data)
	{
		Copy(data);
		return *this;
	}

	bool CString::operator==(const CString& val)
	{
		// 0 if they are equal
		return strcmp(m_Data, val.m_Data) == 0;
	}

	bool CString::operator==(const char* data)
	{
		if (data == nullptr)
		{
			return false;
		}
		return strcmp(m_Data, data) == 0;
	}

	void CString::Copy(const char* data)
	{
		if (data == nullptr)
		{
			CreateEmpty();
		}
		else
		{
			m_Size = strlen(data) + 1;

			if (m_Data)
			{
				Free(m_Data);
			}
			m_Data = new char[m_Size];
			strcpy_s(m_Data, m_Size, data);
		}
	}

	void CString::CreateEmpty()
	{
		m_Size = 1;
		m_Data = new char[m_Size];
		m_Data[0] = '\0';
	}

	size_t CString::Size() const
	{
		return m_Size;
	}

}