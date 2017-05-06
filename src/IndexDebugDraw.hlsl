
#include "Common.hlsl"

struct IndexDebugDrawVSIn
{
	uint id : SV_VERTEXID;
};

struct IndexDebugDrawVSOut
{
	uint id : BLENDINDICES0;
};

struct IndexDebugDrawGSOut
{
	float4 color		: COLOR;
	float4 pos			: SV_POSITION;
};

struct IndexDebugDrawPSIn
{
	float4 color		: COLOR;
};

cbuffer cb0 : register(b0)
{
	row_major float4x4 g_mWorldViewProj;
	row_major float4x4 g_mInvView;
};

cbuffer cbImmutable
{
	static uint Nil = ~0;
}

StructuredBuffer<PosVelo> g_bufPosVelo : register(t0);
StructuredBuffer<uint> g_spacialIndex : register(t1);