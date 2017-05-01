#include "IndexDebugDraw.hlsl"

//
// Vertex shader for drawing the point-sprite particles.
//
IndexDebugDrawVSOut IndexDebugDrawVS(IndexDebugDrawVSIn input)
{
	IndexDebugDrawVSOut output;

	output.id = input.id;

	return output;
}