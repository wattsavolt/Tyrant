#pragma once

#include "Core.h"
#include "Math/Vector3.h"

namespace tyr
{
	// One particle, drawn as a square or streak facing the camera. Matches Particle.hlsli byte for byte.
	struct ParticleInstance
	{
		Vector3 position;
		// Half the square's width, in metres.
		float size;
		// Metres per second, and seconds of that travel it's drawn stretched over, as a streak
		// behind it. A stretch of 0 draws a square.
		Vector3 velocity;
		float stretch;
		// Linear colour and opacity, 8 bits each, R in the lowest byte.
		uint colour;
		// Radians, turning the square about the line to the camera.
		float rotation;
		// The flipbook frame shown: its whole part is the frame, and its fraction blends into the next.
		float frame;
		// The flipbook texture's bindless index, or c_NoParticleTexture for a soft round blob.
		uint textureIndex;
		// The flipbook's columns in the low byte and rows in the next.
		uint flipbook;
		// Scales the colour, for glowing particles brighter than white.
		float brightness;
		// 0 covers what's behind it by its opacity, and 1 adds its light to it.
		float additive;
		// How far in front of a surface it starts fading, so it doesn't cut a hard line through it.
		float softness;
	};

	constexpr uint c_NoParticleTexture = 0xFFFFFFFFu;
}
