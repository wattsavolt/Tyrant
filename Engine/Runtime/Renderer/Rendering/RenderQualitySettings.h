#pragma once

#include "Core.h"

namespace tyr
{
	enum class QualityLevel : uint8
	{
		Low,
		Medium,
		High,
		Ultra
	};

	// Concrete numeric settings every pass actually reads. A quality level only ever picks
	// these defaults (see ResolveQualitySettings below) - passes never branch on the level
	// itself, so a field can be tuned later without re-plumbing anything.
	struct RenderQualitySettings
	{
		QualityLevel level = QualityLevel::Medium;
		uint shadowRaysPerPixel = 1;
		uint maxShadowCastingLocalLights = 0;
		uint denoiserSpatialRadius = 1;
		bool taaEnabled = true;
		float taaHistoryBlendWeight = 0.9f;
		// Not read anywhere yet - reserved for when distance-based LOD selection exists.
		float lodBias = 0.0f;
	};

	inline RenderQualitySettings ResolveQualitySettings(QualityLevel level)
	{
		RenderQualitySettings settings;
		settings.level = level;

		switch (level)
		{
		case QualityLevel::Low:
			settings.shadowRaysPerPixel = 1;
			settings.maxShadowCastingLocalLights = 0;
			settings.denoiserSpatialRadius = 1;
			settings.taaEnabled = true;
			settings.taaHistoryBlendWeight = 0.85f;
			settings.lodBias = 1.0f;
			break;
		case QualityLevel::Medium:
			settings.shadowRaysPerPixel = 1;
			settings.maxShadowCastingLocalLights = 4;
			settings.denoiserSpatialRadius = 1;
			settings.taaEnabled = true;
			settings.taaHistoryBlendWeight = 0.9f;
			settings.lodBias = 0.0f;
			break;
		case QualityLevel::High:
			settings.shadowRaysPerPixel = 2;
			settings.maxShadowCastingLocalLights = 8;
			settings.denoiserSpatialRadius = 2;
			settings.taaEnabled = true;
			settings.taaHistoryBlendWeight = 0.92f;
			settings.lodBias = 0.0f;
			break;
		case QualityLevel::Ultra:
			settings.shadowRaysPerPixel = 4;
			settings.maxShadowCastingLocalLights = 16;
			settings.denoiserSpatialRadius = 2;
			settings.taaEnabled = true;
			settings.taaHistoryBlendWeight = 0.95f;
			settings.lodBias = -1.0f;
			break;
		default:
			TYR_ASSERT(false);
			break;
		}

		return settings;
	}
}
