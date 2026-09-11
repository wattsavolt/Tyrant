
#pragma once

#include "RenderAPITypes.h"

namespace tyr
{
	class TYR_GRAPHICS_API GraphicsUtility
	{
	public:
		static float CalculateAspectRatio(const ViewArea& viewArea, uint targetWidth, uint targetHeight);
		static void CreateRenderArea(const ViewArea& viewArea, uint targetWidth, uint targetHeight, GraphicsRect& renderArea);
	};
}
		