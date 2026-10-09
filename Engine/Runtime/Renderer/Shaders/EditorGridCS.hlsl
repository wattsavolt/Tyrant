#ifndef EDITOR_GRID_CS_HLSli
#define EDITOR_GRID_CS_HLSli

#include "Common.hlsli"

TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_EDITOR_GRID_OUTPUT, 0) RWTexture2D<float4> outputImages[TYR_BUFFERED_FRAME_COUNT] : register(u24);

// Matches EditorGridPushConstants in Renderer.cpp byte for byte.
struct PushConstants
{
	// Without TAA jitter, since the grid is drawn after TAA.
	float4x4 invViewProj;
	uint sourceIndex;
	uint depthIndex;
	uint width;
	uint height;
	uint renderFrameIndex;
	uint majorLineEvery;
	float cellSize;
	// Zero just copies the image, for when only debug lines are drawn over it.
	uint gridEnabled;
};
TYR_VK_PUSH_CONSTANT PushConstants g_PushConstants;

// Line widths are fractions of a cell (minor, axis) or of a major cell (major).
static const float c_MinorLineWidth = 0.01f;
static const float c_MajorLineWidth = 0.0025f;
static const float c_AxisLineWidth = 0.025f;
// Linear colours, since the image being drawn over is linear.
static const float4 c_MinorLineColour = float4(0.25f, 0.25f, 0.25f, 0.45f);
static const float4 c_MajorLineColour = float4(0.4f, 0.4f, 0.4f, 0.7f);
static const float4 c_XAxisColour = float4(0.8f, 0.1f, 0.1f, 0.9f);
static const float4 c_ZAxisColour = float4(0.1f, 0.25f, 0.9f, 0.9f);
// The grid fades out between these distances from the camera, in major cells.
static const float c_FadeStartMajorCells = 10.0f;
static const float c_FadeEndMajorCells = 20.0f;

float3 Unproject(float2 ndc, float deviceDepth)
{
	const float4 position = mul(float4(ndc, deviceDepth, 1.0f), g_PushConstants.invViewProj);
	return position.xyz / position.w;
}

// The viewport is flipped, so NDC y is +1 at the top row.
float2 PixelToNdc(float2 pixel)
{
	const float2 uv = pixel / float2(g_PushConstants.width, g_PushConstants.height);
	return float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
}

// Where the camera ray through the pixel position hits the y = 0 plane. Returns false when the
// ray points away from it.
bool IntersectGround(float2 pixel, out float3 rayOrigin, out float3 hit)
{
	const float2 ndc = PixelToNdc(pixel);
	// Reverse-Z, so 1 is the near plane and 0 the far plane.
	rayOrigin = Unproject(ndc, 1.0f);
	const float3 rayDirection = Unproject(ndc, 0.0f) - rayOrigin;
	hit = rayOrigin;
	if (abs(rayDirection.y) < 1e-6f)
	{
		return false;
	}

	const float t = -rayOrigin.y / rayDirection.y;
	hit = rayOrigin + rayDirection * t;
	return t > 0.0f;
}

// Ben Golus's "pristine grid" - anti-aliased lines that fade to their average coverage rather
// than shimmering once they get thinner than a pixel. uv is in cells and uvDeriv is how much it
// changes across one pixel.
float PristineGrid(float2 uv, float2 uvDeriv, float lineWidth)
{
	const float2 drawWidth = clamp(lineWidth, uvDeriv, 0.5f);
	const float2 lineAA = max(uvDeriv, 0.000001f) * 1.5f;
	const float2 gridUV = 1.0f - abs(frac(uv) * 2.0f - 1.0f);
	float2 grid = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
	grid *= saturate(lineWidth / drawWidth);
	grid = lerp(grid, lineWidth, saturate(uvDeriv * 2.0f - 1.0f));
	return lerp(grid.x, 1.0f, grid.y);
}

// A single line along coordinate 0, the same way as PristineGrid.
float AxisLine(float coord, float coordDeriv, float lineWidth)
{
	const float drawWidth = max(lineWidth, coordDeriv);
	const float lineAA = max(coordDeriv, 0.000001f) * 1.5f;
	const float coverage = smoothstep(drawWidth + lineAA, drawWidth - lineAA, abs(coord));
	return coverage * saturate(lineWidth / drawWidth);
}

// Blends a line over what's below it.
void BlendLine(inout float4 result, float4 lineColour, float coverage)
{
	const float alpha = lineColour.a * coverage;
	result.rgb = lerp(result.rgb, lineColour.rgb, alpha);
	result.a = alpha + result.a * (1.0f - alpha);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= g_PushConstants.width || dispatchThreadId.y >= g_PushConstants.height)
	{
		return;
	}

	const int3 pixelCoord = int3(dispatchThreadId.xy, 0);
	const float3 source = textures[g_PushConstants.sourceIndex].Load(pixelCoord).rgb;
	float3 output = source;

	const float2 pixel = float2(dispatchThreadId.xy) + 0.5f;
	float3 rayOrigin;
	float3 hit;
	if (g_PushConstants.gridEnabled != 0 && IntersectGround(pixel, rayOrigin, hit))
	{
		// Hidden behind anything the scene drew closer to the camera. The small bias keeps the
		// grid on top of surfaces lying exactly on the plane.
		const float gridDistance = distance(rayOrigin, hit);
		const float sceneDepth = textures[g_PushConstants.depthIndex].Load(pixelCoord).r;
		const bool occluded = sceneDepth > 0.0f &&
			distance(rayOrigin, Unproject(PixelToNdc(pixel), sceneDepth)) < gridDistance * 0.999f;

		if (!occluded)
		{
			const float cellSize = g_PushConstants.cellSize;
			const float majorCellSize = cellSize * g_PushConstants.majorLineEvery;

			// Exact per-pixel derivatives from where the neighbouring pixels hit the plane. A
			// neighbour past the horizon counts as a huge change, fading the lines out there.
			float3 neighbourOrigin;
			float3 hitRight;
			float3 hitDown;
			const bool hasRight = IntersectGround(pixel + float2(1.0f, 0.0f), neighbourOrigin, hitRight);
			const bool hasDown = IntersectGround(pixel + float2(0.0f, 1.0f), neighbourOrigin, hitDown);
			const float2 ddxWorld = hasRight ? hitRight.xz - hit.xz : float2(1e6f, 1e6f);
			const float2 ddyWorld = hasDown ? hitDown.xz - hit.xz : float2(1e6f, 1e6f);
			const float2 worldDeriv = float2(length(float2(ddxWorld.x, ddyWorld.x)), length(float2(ddxWorld.y, ddyWorld.y)));

			float4 grid = float4(0.0f, 0.0f, 0.0f, 0.0f);
			BlendLine(grid, c_MinorLineColour, PristineGrid(hit.xz / cellSize, worldDeriv / cellSize, c_MinorLineWidth));
			BlendLine(grid, c_MajorLineColour, PristineGrid(hit.xz / majorCellSize, worldDeriv / majorCellSize, c_MajorLineWidth));
			// The X axis runs along z = 0 and the Z axis along x = 0.
			BlendLine(grid, c_XAxisColour, AxisLine(hit.z / cellSize, worldDeriv.y / cellSize, c_AxisLineWidth));
			BlendLine(grid, c_ZAxisColour, AxisLine(hit.x / cellSize, worldDeriv.x / cellSize, c_AxisLineWidth));

			const float horizontalDistance = distance(rayOrigin.xz, hit.xz);
			const float fade = 1.0f - smoothstep(c_FadeStartMajorCells * majorCellSize, c_FadeEndMajorCells * majorCellSize, horizontalDistance);

			// grid.rgb is already the lines blended over black, weighted by grid.a.
			const float alpha = grid.a * fade;
			const float3 lineColour = grid.a > 0.0f ? grid.rgb / grid.a : float3(0.0f, 0.0f, 0.0f);
			output = lerp(source, lineColour, alpha);
		}
	}

	outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(output, 1.0f);
}

#endif
