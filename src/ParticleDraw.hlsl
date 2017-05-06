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

#include "Common.hlsl"

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

cbuffer cb0 : register(b1)
{
	row_major float4x4 g_mWorldViewProj;
	row_major float4x4 g_mInvView;
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