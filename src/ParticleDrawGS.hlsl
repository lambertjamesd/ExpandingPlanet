
#include "ParticleDraw.hlsl"

//
// GS for rendering point sprite particles.  Takes a point and turns 
// it into 2 triangles.
//
[maxvertexcount(4)]
void GSParticleDraw(point VSParticleDrawOut input[1], inout TriangleStream<GSParticleDrawOut> SpriteStream)
{
	GSParticleDrawOut output;

	output.worldNormal = normalize(input[0].pos);

	// Emit two new triangles.
	for (int i = 0; i < 4; i++)
	{
		float3 position = g_positions[i] * particleRadius;
		position = mul(position, (float3x3)g_mInvView) + input[0].pos;
		output.pos = mul(float4(position, 1.0), g_mWorldViewProj);

		output.color = input[0].color;
		output.tex = g_texcoords[i];
		SpriteStream.Append(output);
	}
	SpriteStream.RestartStrip();
}