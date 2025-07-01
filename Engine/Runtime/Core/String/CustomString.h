#pragma once

#include "Base/base.h"

namespace tyr
{
	class TYR_CORE_EXPORT CString final
	{
	public:
		CString();
		CString(const char* data);
		CString(const CString& val);
		CString(CString&& val) noexcept;
		~CString();
		CString& operator=(const CString& val);
		CString& operator=(const char* data);
		bool operator==(const CString& val);
		bool operator==(const char* data);

		size_t Size() const { return m_Size - 1; }

	private:
		void Copy(const char* data);
		void CreateEmpty();

		char* m_Data;
		size_t m_Size;
	};
	
}