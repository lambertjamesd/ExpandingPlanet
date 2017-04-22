//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

struct PosVelo
{
	float4 pos;
	uint nextPoint;
	uint3 padding;
};


struct VSParticleIn
{
	float4 color	: COLOR;
	uint id			: SV_VERTEXID;
};

struct VSParticleDrawOut
{
	float3 pos			: POSITION;
	float4 color		: COLOR;
};

struct GSParticleDrawOut
{
	float2 tex			: TEXCOORD0;
	float4 color		: COLOR;
	float3 worldNormal  : TEXCOORD1;
	float4 pos			: SV_POSITION;
};

struct PSParticleDrawIn
{
	float2 tex			: TEXCOORD0;
	float4 color		: COLOR;
	float3 worldNormal  : TEXCOORD1;
};

StructuredBuffer<PosVelo> g_bufPosVelo	: register(t0);
Texture2D g_texture : register(t1);
Texture2D g_spectrum : register(t2);

SamplerState g_sampler : register(s0);

cbuffer cb0
{
	row_major float4x4 g_mWorldViewProj;
	row_major float4x4 g_mInvView;
};

cbuffer cb1
{
	static float g_fParticleRad = 10.0f;
};

cbuffer cbImmutable
{
	static float3 g_positions[4] =
	{
		float3(-1, 1, 0),
		float3(1, 1, 0),
		float3(-1, -1, 0),
		float3(1, -1, 0),
	};
	
	static float2 g_texcoords[4] =
	{ 
		float2(0, 0), 
		float2(1, 0),
		float2(0, 1),
		float2(1, 1),
	};
};

//
// Vertex shader for drawing the point-sprite particles.
//
VSParticleDrawOut VSParticleDraw(VSParticleIn input)
{
	VSParticleDrawOut output;
	
	output.pos = g_bufPosVelo[input.id].pos.xyz;

	float spectrumInput = input.id / 10000.0f;
		
	output.color = g_spectrum.SampleLevel(g_sampler, float2(spectrumInput, 0), 0);

	return output;
}

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
		float3 position = g_positions[i] * g_fParticleRad;
		position = mul(position, (float3x3)g_mInvView) + input[0].pos;
		output.pos = mul(float4(position,1.0), g_mWorldViewProj);

		output.color = input[0].color;
		output.tex = g_texcoords[i];
		SpriteStream.Append(output);
	}
	SpriteStream.RestartStrip();
}

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
