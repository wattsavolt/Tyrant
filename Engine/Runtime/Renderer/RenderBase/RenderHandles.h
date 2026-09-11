#pragma once

#include "Core.h"
#include "RendererMacros.h"

namespace tyr
{
	TYR_CREATE_HANDLE_TYPE(RenderWindowHandle);
	TYR_CREATE_HANDLE_TYPE(RenderBufferHandle);
	TYR_CREATE_HANDLE_TYPE(TextureHandle);
	TYR_CREATE_HANDLE_TYPE(MaterialHandle);
	TYR_CREATE_HANDLE_TYPE(MeshHandle);
	TYR_CREATE_HANDLE_TYPE(SkeletonHandle);
	TYR_CREATE_HANDLE_TYPE(SkeletalMeshHandle);
	TYR_CREATE_HANDLE_TYPE(MeshInstanceHandle);
	TYR_CREATE_HANDLE_TYPE(SkeletalMeshInstanceHandle);
	TYR_CREATE_HANDLE_TYPE(DirLightHandle);
	TYR_CREATE_HANDLE_TYPE(PointLightHandle);
	TYR_CREATE_HANDLE_TYPE(SpotLightHandle);
}