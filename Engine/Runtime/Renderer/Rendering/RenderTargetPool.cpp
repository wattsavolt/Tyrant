#include "RenderTargetPool.h"
#include "RenderRegistry.h"
#include "RenderAPI/Device.h"
#include "RenderResource/Texture.h"
#include "RenderResource/TextureDesc.h"

namespace tyr
{
	namespace
	{
		constexpr uint c_ReservedFreeTargets = 64;
	}

	RenderTargetPool::RenderTargetPool(RenderRegistry& registry, Device& device)
		: m_Registry(registry)
		, m_Device(device)
	{
		m_FreeTargets.Reserve(c_ReservedFreeTargets);
	}

	TextureHandle RenderTargetPool::Acquire(const TextureDesc& desc, bool& outCreated)
	{
		// Newest first, since those are the likeliest to be asked for again.
		for (uint i = m_FreeTargets.Size(); i-- > 0;)
		{
			const TextureHandle handle = m_FreeTargets[i].handle;
			const Texture& texture = m_Registry.GetTexture(handle);
			if (texture.info == desc.info && texture.usage == desc.usage && texture.sampleCount == desc.sampleCount)
			{
				// Erased in place to keep the oldest-first order.
				m_FreeTargets.Erase(i);
				outCreated = false;
				return handle;
			}
		}

		outCreated = true;
		return m_Registry.CreateTexture(desc);
	}

	void RenderTargetPool::Release(TextureHandle handle, uint64 frameNumber)
	{
		m_FreeTargets.Add({ handle, frameNumber });
	}

	void RenderTargetPool::Trim(uint64 frameNumber)
	{
		if (m_FreeTargets.IsEmpty())
		{
			return;
		}

		size_t usage;
		size_t budget;
		m_Device.GetDeviceMemoryBudget(usage, budget);
		const size_t threshold = static_cast<size_t>(static_cast<double>(budget) * c_BudgetThreshold);

		uint freeCount = 0;
		for (; freeCount < m_FreeTargets.Size(); ++freeCount)
		{
			const FreeTarget& target = m_FreeTargets[freeCount];
			const bool tooOld = frameNumber - target.releasedFrame > c_MaxIdleFrames;
			if (!tooOld && usage <= threshold)
			{
				break;
			}

			// Deleting gives the memory back straight away, since the GPU finished with it before
			// it was released here.
			Texture& texture = m_Registry.GetTexture(target.handle);
			const size_t size = m_Device.GetImageAllocationSize(texture.image);
			usage = usage > size ? usage - size : 0;
			m_Registry.DeleteTexture(target.handle);
		}

		if (freeCount > 0)
		{
			m_FreeTargets.EraseFromFront(freeCount);
		}
	}

	void RenderTargetPool::Clear()
	{
		for (const FreeTarget& target : m_FreeTargets)
		{
			m_Registry.DeleteTexture(target.handle);
		}
		m_FreeTargets.Clear();
	}
}
