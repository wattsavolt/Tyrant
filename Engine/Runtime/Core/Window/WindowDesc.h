#pragma once

#include "Core.h"

namespace tyr
{
	struct WindowDesc
	{
		int showFlag = 1;
		int width = 1920;
		int height = 1080;
		uint16 iconResourceId = 0;
		const char* name;
	};
}