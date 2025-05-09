#include "Logger.h"
#include "String/StringUtil.h"
#include "Threading/Threading.h"
#include <iostream>

namespace tyr
{
	void Logger::LogMessage(const String& msg, LogLevel level, LogCategory category)
	{
		// TODO: Log out to a file here
		switch (level)
		{
		case LogLevel::Fatal:
			LogToConsole(msg.c_str(), "FATAL");
			break;
		case LogLevel::Error:
			LogToConsole(msg.c_str(), "ERROR");
			break;
		case LogLevel::Warning:
			LogToConsole(msg.c_str(), "WARNING");
			break;
		case LogLevel::Info:
			LogToConsole(msg.c_str(), "INFO");
			break;
		}	
	}

	void Logger::LogErrorAndTrace(const String& msg, const String& stackTrace, LogLevel logLevel)
	{
		StringStream errorMsg;
		if (logLevel == LogLevel::Fatal)
		{
			errorMsg << "A fatal error occurred and the application must be terminated!" << std::endl;
		}
		errorMsg << msg;

		if (stackTrace != "")
		{
			errorMsg << "\n\nStack trace: \n";
			errorMsg << stackTrace;
		}

		LogMessage(errorMsg.str(), logLevel);
	}

	void Logger::LogError(LogLevel logLevel, const String& function, const String& file, uint line, const char* desc, ...)
	{
		va_list args;
		char buffer[4096];

		va_start(args, desc);
		vsprintf_s(buffer, desc, args);
		va_end(args);

		String descStr = buffer;

		StringStream msg;
		msg << "  - Description: " << descStr << std::endl;
		msg << "  - Function: " << function << std::endl;
		msg << "  - File: "     << file << ":" << line;

		// TODO:: Get stack trace from Platform
		LogErrorAndTrace(msg.str(), "", logLevel);
	}

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS && TYR_COMPILER == TYR_COMPILER_MSVC
#include <windows.h>
	void Logger::LogToConsole(const String& msg, const char* catergory)
	{
		static Mutex mutex;

		Lock lock(mutex);
		OutputDebugString("[");
		OutputDebugString(catergory);
		OutputDebugString("] ");
		OutputDebugString(msg.c_str());
		OutputDebugString("\n");

		// Default output if running without debugger.
		std::cout << "[" << catergory << "] " << msg << std::endl;
	}
#else
	void Logger::LogToConsole(const String& msg, const char* catergory)
	{
		std::cout << "[" << catergory << "] " << msg << std::endl;
	}
#endif

	Logger& Logger::Instance()
	{
		static Logger logger;
		return logger;
	}
}