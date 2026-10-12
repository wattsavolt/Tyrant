#pragma once

#include "Base/base.h"
#include "String/StringTypes.h"
#include "Threading/ThreadTypes.h"

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

	// A recent message, kept for tools such as the editor's MCP server to read back.
	struct LogEntry
	{
		static constexpr uint c_MaxTextSize = 512;
		// Counts up from 1 across every message, so a reader can ask for only newer ones.
		uint64 sequence;
		LogLevel level;
		// Cut short to fit.
		char text[c_MaxTextSize];
	};

	class TYR_CORE_API Logger 
	{
	public:
		Logger& operator=(const Logger&) = delete;
		Logger(const Logger&) = delete;

		void LogMessage(const String& msg, LogLevel logLevel, LogCategory logCatergory = LogCategory::Unspecified);

		void LogErrorAndTrace(const String& msg, const String& stackTrace, LogLevel logLevel);

		void LogError(LogLevel logLevel, const String& function, const String& file, uint line, const char* desc, ...);

		static Logger& Instance();

		// Copies up to maxCount of the kept messages newer than afterSequence, oldest first, and
		// returns how many. Only the most recent c_HistorySize are kept.
		uint GetRecentEntries(uint64 afterSequence, LogEntry* outEntries, uint maxCount);

		static constexpr uint c_HistorySize = 256;

	private:
		Logger() = default;
		void LogToConsole(const String& msg, const char* catergory);
		void AddToHistory(const String& msg, LogLevel level);

		LogEntry m_History[c_HistorySize];
		uint64 m_NextSequence = 1;
		Mutex m_HistoryMutex;
	};
	
}