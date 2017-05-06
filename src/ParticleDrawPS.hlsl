
#include "ParticleDraw.hlsl"

//
// PS for drawing particles. Use the texture coordinates to generate a 
// radial gradient representing the particle.
//
float4 PSParticleDraw(PSParticleDrawIn input) : SV_Target
{
	float3 ambientColor = float3(0.2, 0.3, 0.5);
	float3 normal = g_texture.Sample(g_sampler, input.tex).xyz;
	if (dot(normal, normal) < 0.5f) {
		discard;
	}

	float ambientFactor = saturate(dot(normal, input.worldNormal));
	float diffuseFactor = saturate(dot(normal, float3(0.7, 0, 0.7)));
	float3 finalColor = input.color.rgb * (ambientColor * ambientFactor + diffuseFactor);
	return float4(finalColor, 1);
}
