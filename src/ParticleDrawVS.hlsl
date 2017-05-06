
#include "ParticleDraw.hlsl"

//
// Vertex shader for drawing the point-sprite particles.
//
VSParticleDrawOut VSParticleDraw(VSParticleIn input)
{
	VSParticleDrawOut output;

	output.pos = g_bufPosVelo[input.id].pos.xyz;

	float spectrumInput = input.id / 200.0f;

	output.color = float4(0.7, 0.7, 0.7, 1.0f);// g_spectrum.SampleLevel(g_sampler, float2(spectrumInput, 0), 0);

	return output;
}