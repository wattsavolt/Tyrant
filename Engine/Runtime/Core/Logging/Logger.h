#pragma once

#include "Base/base.h"
#include "String/StringTypes.h"

namespace tyr
{
	enum class LogLevel
	{
		Fatal,
		Error,
		Warning,
		Info	
	};

	enum class LogCategory : uint
	{
		Unspecified,
		General,
		FileSystem
	};

	class TYR_CORE_EXPORT Logger 
	{
	public:
		Logger& operator=(const Logger&) = delete;
		Logger(const Logger&) = delete;

		void LogMessage(const String& msg, LogLevel logLevel, LogCategory logCatergory = LogCategory::Unspecified);

		void LogErrorAndTrace(const String& msg, const String& stackTrace, LogLevel logLevel);

		void LogError(LogLevel logLevel, const String& function, const String& file, uint line, const char* desc, ...);

		static Logger& Instance();

	private:
		Logger() = default;
		void LogToConsole(const String& msg, const char* catergory);
	};
	
}