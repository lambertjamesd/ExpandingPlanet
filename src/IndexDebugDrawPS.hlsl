#include "IndexDebugDraw.hlsl"
//
// PS for drawing particles. Use the texture coordinates to generate a 
// radial gradient representing the particle.
//
float4 IndexDebugDrawPS(IndexDebugDrawPSIn input) : SV_Target
{
	return input.color;
}
