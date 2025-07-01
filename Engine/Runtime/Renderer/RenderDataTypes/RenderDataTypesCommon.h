#pragma once

#include "RendererMacros.h"
#include "Core.h"

namespace tyr
{
	typedef uint64 RenderableID;

	struct RenderDataAccessor
	{
		// Container is a buffer or array of textures
		uint containerIndex = 0;
		uint elementIndex = 0;

		bool operator== (const RenderDataAccessor& accessor)
		{
			return containerIndex == accessor.containerIndex && elementIndex == accessor.elementIndex;
		}

		bool operator< (const RenderDataAccessor& accessor)
		{
			if (containerIndex < accessor.containerIndex)
			{
				return true;
			}
			if (containerIndex == accessor.containerIndex)
			{
				return elementIndex < accessor.elementIndex;
			}
			return false;
		}

		uint64 GetID() const
		{
			return (((uint64)containerIndex) << 32) | ((uint64)elementIndex);
		}
	};

}

namespace std
{
	template <>
	struct hash<tyr::RenderDataAccessor>
	{
		size_t operator()(const tyr::RenderDataAccessor& accessor) const noexcept
		{
			return std::hash<tyr::uint64>{}(accessor.GetID());
		}
	};
}
