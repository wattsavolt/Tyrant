#ifndef MESH_PS_HLSli
#define MESH_PS_HLSli

#include "Common.hlsli"
#include "LightUtility.hlsli"

cbuffer SceneInfoCBuffer : register(b0)
{
	float4x4 ViewProj;
	float3 camPos;
	float ambient;
};

cbuffer SpotLightCBuffer : register(b1)
{
	SpotLight spotLight;
};

cbuffer MaterialCBuffer : register(b2)
{
	Material material;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	//input.normal = normalize(input.normal);

	//// The ambient light affecting the material should be calculated in the renderer before the material is drawn.

	//// Create a float3 of the input position. Easier to use.
	//float3 pixPos = float3(input.worldPos.x, input.worldPos.y, input.worldPos.z);

	//// Calculate the direction from the pixel to the camera.
	//float3 toEyeVec = normalize(camPos - pixPos);

	//float3 colour = float3(0.0f, 0.0f, 0.0f);

	//// These variables are reassigned every time a light computational function is called.
	//float3 diffuse;
	//float3 specular;

	////for (int i = 0; i < pointLightCount; i++)
	////{
	//	//ComputePointLightEffect(pointLights[i], material, pixPos, input.normal, toEyeVec, diffuse, specular);
	//	//colour += saturate(diffuse + specular);
	////}

	//ComputeSpotLightEffect(spotLight, material, pixPos, input.normal, toEyeVec, diffuse, specular);
	//colour += saturate(diffuse + specular);

	//// Add ambient light value
	//colour.x += ambient;
	//colour.y += ambient;
	//colour.z += ambient;

	//// Make sure the values are between 0 and 1
	//colour = saturate(colour);

	//// Return Final Colour
	//return float4(colour.x, colour.y, colour.z, 1.0f);
	// Sample the BC7 texture
	float3 linearColor = float3(1,0,0);// textures[0].Sample(sampler, uv).rgb;

	// No need to convert to sRGB manually as the swapchain format is sRGB so the linear colour is automatically converted to sRGB.
    return float4(linearColor, 1.0f);
}

#endif

