#pragma once

#include <string>
#include "Memory/FrameAllocation.h"

namespace tyr
{
	typedef std::basic_string<char, std::char_traits<char>, StdAllocator<char, FrameAllocator>> FrameString;

	typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, StdAllocator<wchar_t, FrameAllocator>> FrameWString;
}