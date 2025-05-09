
#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"

namespace tyr
{
	
	// TODO: Implement
	struct AccelerationStructureDesc
	{
		int placeholder;
	};

	/// Class repesenting an acceleration structure 
	class TYR_GRAPHICS_EXPORT AccelerationStructure
	{
	public:
		AccelerationStructure(const AccelerationStructureDesc& desc);
		virtual ~AccelerationStructure() = default;
		
		const AccelerationStructureDesc& GetDesc() const { return m_Desc; }
	

	protected:
		AccelerationStructureDesc m_Desc;
	};
}
