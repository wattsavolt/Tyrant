#pragma once

#include "RenderAPI/RenderAPITypes.h"
#include "RenderAPI/Image.h"
#include "Math/Vector3I.h"
#include "RenderResource.h"
#include "TextureDesc.h"

namespace tyr
{
	struct Texture : public RenderResource
	{
		ImageHandle image;
		ImageViewHandle imageView;
		SamplerHandle sampler;
		TextureInfo info;
		ImageLayout imageLayout;
	};
}
