#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Matrix4.h"
#include "RenderBase/RenderHandles.h"

namespace tyr
{
	static constexpr uint c_InvalidRenderIndex = UINT32_MAX;

	struct MeshInstanceInfo
	{
		Matrix4 transform;
		MeshHandle mesh;
		MaterialHandle material;
	};

	struct SkeletalMeshInstanceInfo
	{
		Matrix4 transform;
		MeshHandle mesh;
		SkeletonHandle skeleton;
		MaterialHandle material;
	};

	struct DirectionalLightInfo
	{
		Vector3 direction;
		Vector3 colour;
		float intensity;
		bool castsShadow;
	};

	struct PointLightInfo
	{
		Vector3 position;
		Vector3 attenuation;
		Vector3 colour;
		float intensity;
		float range;
		bool castsShadow;
	};

	struct SpotLightInfo
	{
		Vector3 position;
		Vector3 direction;
		Vector3 colour;
		Vector3 attenuation;
		float intensity;
		float range;
		float cone;
		bool castsShadow;
	};

	struct MeshInstanceDesc
	{
		MeshInstanceInfo info;
	};

	struct SkeletalMeshInstanceDesc
	{
		SkeletalMeshInstanceInfo info;
	};

	struct DirectionalLightDesc
	{
		DirectionalLightInfo info;
	};

	struct PointLightDesc
	{
		PointLightInfo info;
	};

	struct SpotLightDesc
	{
		SpotLightInfo info;
	};
}