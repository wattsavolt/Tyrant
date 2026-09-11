#include "EcsChunkPool.h"

namespace tyr
{
	EcsChunkPool& EcsChunkPool::Instance()
	{
		static EcsChunkPool instance;
		return instance;
	}
}
