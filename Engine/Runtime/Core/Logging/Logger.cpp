#include "Logger.h"
#include "String/StringUtil.h"
#include "Threading/ThreadTypes.h"
#include <cstdio>
#include <iostream>

namespace tyr
{
	void Logger::LogMessage(const String& msg, LogLevel level, LogCategory category)
	{
		AddToHistory(msg, level);

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

	void Logger::AddToHistory(const String& msg, LogLevel level)
	{
		LockGuard lock(m_HistoryMutex);
		LogEntry& entry = m_History[m_NextSequence % c_HistorySize];
		entry.sequence = m_NextSequence++;
		entry.level = level;
		snprintf(entry.text, sizeof(entry.text), "%s", msg.c_str());
	}

	uint Logger::GetRecentEntries(uint64 afterSequence, LogEntry* outEntries, uint maxCount)
	{
		LockGuard lock(m_HistoryMutex);
		const uint64 oldestKept = m_NextSequence > c_HistorySize ? m_NextSequence - c_HistorySize : 1;
		uint count = 0;
		for (uint64 sequence = afterSequence + 1 > oldestKept ? afterSequence + 1 : oldestKept; sequence < m_NextSequence && count < maxCount; ++sequence)
		{
			outEntries[count++] = m_History[sequence % c_HistorySize];
		}
		return count;
	}

	Logger& Logger::Instance()
	{
		static Logger logger;
		return logger;
	}
}