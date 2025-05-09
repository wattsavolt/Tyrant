#pragma once

#include <Base/Base.h>

namespace tyr
{
	class ImageLoader final
	{
	public:
		static uchar* LoadFile(const char* fileName);
	};
}