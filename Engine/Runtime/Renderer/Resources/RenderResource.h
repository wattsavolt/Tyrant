#pragma once

#include "Base/Base.h"
#include "RendererMacros.h"
#include "RenderAPI/RenderAPITypes.h"
#include <atomic>

namespace tyr
{
	class Device;
	
	typedef uint64 ResourceID;

	/// Base class for a graphics resource.
	class TYR_RENDERER_EXPORT RenderResource
	{
	public:
		RenderResource(Device& device);
		virtual ~RenderResource();

		uint64 GetID() const { return m_ID; }

		// Updatelater to support one per buffer view
		BarrierAccess GetAccessState(uint index = 0) const { return m_AccessState; }

	protected:
		Device& m_Device;
		ResourceID m_ID;
		BarrierAccess m_AccessState;

	private:
		friend class RenderGraph;
		static std::atomic<ResourceID> s_NextID;
	};
}
