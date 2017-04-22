
#define blocksize 128

struct PosVelo
{
	float4 pos;
	uint nextPoint;
	uint prevIndexCell;
	uint2 padding;
};

cbuffer cbCS : register(b0)
{
	float particleRadius;
	float maxTime;
	float coreRadius;
	float currentTime;
	float particleVelocity;
	float solidPressure;
	float cellWidth;
	uint indexSize;
};

cbuffer cbImmutable
{
	static uint Nil = ~0u;
}

StructuredBuffer<uint> oldIndex : register(t0);
StructuredBuffer<PosVelo> oldPoint : register(t1);
RWStructuredBuffer<uint> newIndex : register(u0);
RWStructuredBuffer<PosVelo> newPoint : register(u1);

uint CellPositionToIndex(uint3 cellPosition)
{
	return cellPosition.x + (cellPosition.y + cellPosition.z * indexSize) * indexSize;
}

uint PositionToIndex(float3 position)
{
	float3 origin = -(indexSize * cellWidth * 0.5);
	return CellPositionToIndex(clamp((int3)floor((position - origin) / cellWidth), 0u, indexSize));
}

float3 GetPosition(uint index)
{
	return oldPoint[index].pos.xyz;
}

uint GetNext(uint current)
{
	return oldPoint[current].nextPoint;
}

[numthreads(blocksize, 1, 1)]
void ReIndexMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{

}