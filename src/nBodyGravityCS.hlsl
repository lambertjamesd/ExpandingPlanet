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


static float softeningSquared	= 0.0012500000f * 0.0012500000f;
static float g_fG				= 6.67300e-11f * 10000.0f;
static float g_fParticleMass	= g_fG * 10000.0f * 10000.0f;

#define blocksize 128
groupshared float4 sharedPos[blocksize];

//
// Body to body interaction, acceleration of the particle at position 
// bi is updated.
//
void bodyBodyInteraction(inout float3 ai, float4 bj, float4 bi, float mass, int particles) 
{
	float3 r = bj.xyz - bi.xyz;

	float distSqr = dot(r, r);
	distSqr += softeningSquared;

	float invDist = 1.0f / sqrt(distSqr);
	float invDistCube =  invDist * invDist * invDist;
	
	float s = mass * invDistCube * particles;

	ai += r * s;
}

cbuffer cbCS : register(b0)
{
	uint4   g_param;	// param[0] = MAX_PARTICLES;
						// param[1] = dimx;
	float4  g_paramf;	// paramf[0] = 0.1f;
						// paramf[1] = 1; 
};

StructuredBuffer<PosVelo> oldPosVelo	: register(t0);	// SRV
RWStructuredBuffer<PosVelo> newPosVelo	: register(u0);	// UAV

[numthreads(blocksize, 1, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	// Each thread of the CS updates one of the particles.
	float4 pos = oldPosVelo[DTid.x].pos;

	//float3 pointDir = normalize(pos.xyz);

	//pos.xyz -= pointDir * 0.1;		//deltaTime;

	//float innerRadius = 500;

	//if (dot(pos.xyz, pointDir) < innerRadius)
	//{
	//	pos.xyz = pointDir * innerRadius;
	//}

	if (DTid.x < g_param.x)
	{
		newPosVelo[DTid.x].pos = pos;
	}
}
