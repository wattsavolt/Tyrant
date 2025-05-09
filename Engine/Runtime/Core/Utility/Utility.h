#pragma once

#include "Logging/Logger.h"
#include "Platform/Platform.h"

namespace tyr
{

	#define TYR_VALUE(arg) #arg
	#define TYR_TO_LITERAL(arg) TYR_VALUE(arg)

	#define TYR_CONCAT(first, second) first second

	#if defined(_DEBUG) || defined(TYR_ENABLE_PROFILING)
	#define TYR_LOG(level, desc, ...)                                                                \
			Logger::Instance().LogError(level, __PRETTY_FUNCTION__, __FILE__, __LINE__, desc, __VA_ARGS__);     \
			if (level == LogLevel::Fatal)                                                          \
				Platform::Exit(false); 
	#else
	#	define TYR_LOG(desc, level)
	#endif

	#define TYR_LOG_FATAL(desc, ...) TYR_LOG(LogLevel::Fatal, desc, __VA_ARGS__)                           																																			        
	#define TYR_LOG_ERROR(desc, ...) TYR_LOG(LogLevel::Error, desc, __VA_ARGS__) 
	#define TYR_LOG_WARNING(desc, ...) TYR_LOG(LogLevel::Warning, desc, __VA_ARGS__) 
	#define TYR_LOG_INFO(desc, ...) TYR_LOG(LogLevel::Info, desc, __VA_ARGS__) 

	#define TYR_ALERT(msg) Platform::ShowAlertMessage(msg);

	// Assert Message
	#if TYR_DEBUG 
	#   define TYR_ASSERT_MSG(v, m) { if ( !(v) ) { TYR_LOG_WARNING(m); } }
	#else
	#	define TYR_ASSERT(v)
	#endif

	template <typename T>
	constexpr T CompileTimePower(T base, int exponent)
	{
		return (exponent == 0) ? 1 : base * CompileTimePower<T>(base, exponent - 1);
	}

	/// Class providing general utility functions 
	class TYR_CORE_EXPORT Utility
	{
	public:

		static constexpr unsigned char ReverseByte(unsigned char b)
		{
			b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
			b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
			b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
			return b;
		}

		static int NumDigits(int value);

		static bool HasFlag(uint flags, uint flag)
		{
			return (flags & flag) == flag;
		};
	};
}
