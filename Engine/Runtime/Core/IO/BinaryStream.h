#pragma once

#include "Base/base.h"
#include "Base/INonCopyable.h"
#include "String/StringTypes.h"

namespace tyr
{
	class TYR_CORE_EXPORT BinaryStream : private INonCopyable
	{
	public:
		enum class Operation
		{
			Read = 1,
			Write = 2
		};

		enum class Type
		{
			File = 1,
			BufferedFile = 2
		};
	public:
		BinaryStream(Operation op = Operation::Read);
		virtual ~BinaryStream();
	
		const char* GetName() const { return m_Name; }

		void SetName(const char* name) { m_Name = name; }

		Operation GetOperation() const { return m_Operation; }

		size_t GeSize() const { return m_Size; }

		virtual void WriteString(const String& string, StringEncoding encoding = StringEncoding::UTF8);

		virtual void WriteString(const WString& string, StringEncoding encoding = StringEncoding::UTF8);

		virtual String ReadAsString();

		virtual WString ReadAsWString();

		virtual size_t Write(const void* buffer, size_t count) { return 0; }

		virtual size_t Read(void* buffer, size_t count) = 0;

		virtual Type GetStreamType() const = 0;

		virtual void Skip(size_t count) = 0;

		virtual void Seek(size_t pos) = 0;

		virtual size_t GetOffset() const = 0;

		virtual bool IsEOF() const = 0;

		virtual void Close() = 0;

	protected:
		const char* m_Name;
		size_t m_Size;
		Operation m_Operation;
	};
	
}