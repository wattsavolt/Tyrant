#ifndef COLOUR_UTILITY_HLSli
#define COLOUR_UTILITY_HLSli

// Rec. 709 luminance of linear colour.
float Luminance(float3 colour)
{
	return dot(colour, float3(0.2126f, 0.7152f, 0.0722f));
}

// Gran Turismo's tone curve (Uchimura 2017), per channel. A toe, a straight middle that keeps
// mid tones as they are, and a shoulder rolling off to peak: 1 for SDR, or the display's
// brightest over paper white for HDR, so both look the same below the shoulder.
float3 TonemapUchimura(float3 x, float peak)
{
	const float contrast = 1.0f;
	const float linearStart = 0.22f;
	const float linearLength = 0.4f;
	const float blackTightness = 1.33f;

	const float l0 = ((peak - linearStart) * linearLength) / contrast;
	const float s0 = linearStart + l0;
	const float s1 = linearStart + contrast * l0;
	const float c2 = (contrast * peak) / (peak - s1);
	const float cp = -c2 / peak;

	const float3 toeWeight = 1.0f - smoothstep(0.0f, linearStart, x);
	const float3 shoulderWeight = step(s0, x);
	const float3 linearWeight = 1.0f - toeWeight - shoulderWeight;

	const float3 toe = linearStart * pow(max(x, 0.0f) / linearStart, blackTightness);
	const float3 shoulder = peak - (peak - s1) * exp(cp * (x - s0));
	const float3 middle = linearStart + contrast * (x - linearStart);
	return toe * toeWeight + middle * linearWeight + shoulder * shoulderWeight;
}

float3 SRGBToLinear(float3 c)
{
	const float3 low = c / 12.92f;
	const float3 high = pow((max(c, 0.0f) + 0.055f) / 1.055f, 2.4f);
	return lerp(low, high, step(0.04045f, c));
}

float3 LinearToSRGB(float3 c)
{
	const float3 low = c * 12.92f;
	const float3 high = 1.055f * pow(max(c, 0.0f), 1.0f / 2.4f) - 0.055f;
	return lerp(low, high, step(0.0031308f, c));
}

// Linear Rec. 709 colour to linear Rec. 2020, the gamut HDR10 is in.
float3 Rec709ToRec2020(float3 c)
{
	const float3x3 m =
	{
		0.6274040f, 0.3292820f, 0.0433136f,
		0.0690970f, 0.9195400f, 0.0113612f,
		0.0163916f, 0.0880132f, 0.8955950f
	};
	return mul(m, c);
}

// The SMPTE ST 2084 (PQ) curve, from linear light where 1 is 10000 nits.
float3 EncodePQ(float3 c)
{
	const float m1 = 0.1593017578125f;
	const float m2 = 78.84375f;
	const float c1 = 0.8359375f;
	const float c2 = 18.8515625f;
	const float c3 = 18.6875f;
	const float3 p = pow(max(c, 0.0f), m1);
	return pow((c1 + c2 * p) / (1.0f + c3 * p), m2);
}

// Noise between 0 and 1 that changes from pixel to pixel without visible patterns (Jimenez 2014).
float InterleavedGradientNoise(float2 pixel)
{
	return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

#endif
