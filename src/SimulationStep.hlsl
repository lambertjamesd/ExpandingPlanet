
#define blocksize 128

#include "Common.hlsl"

cbuffer cbImmutable
{
	static uint Nil = ~0u;
}

StructuredBuffer<PosVelo> oldPoint : register(t0);
StructuredBuffer<uint> oldIndex : register(t1);
RWStructuredBuffer<PosVelo> newPoint : register(u0);
RWStructuredBuffer<uint> newIndex : register(u1);

uint CellPositionToIndex(uint3 cellPosition)
{
	return cellPosition.x + (cellPosition.y + cellPosition.z * indexSize) * indexSize;
}

uint3 PostionToCellPosition(float3 position)
{
	float3 origin = -(indexSize * cellWidth * 0.5);
	return clamp((int3)floor((position - origin) / cellWidth), 0u, indexSize);
}

uint PositionToIndex(float3 position)
{
	return CellPositionToIndex(PostionToCellPosition(position));
}

float3 GetPosition(uint index)
{
	return oldPoint[index].pos.xyz;
}

uint GetNext(uint current)
{
	return oldPoint[current].nextPoint;
}

uint GetFirstPoint(uint index)
{
	return oldIndex[index] & 0x7FFFFFFF;
}

uint WriteToIndex(uint index, uint pointIndex)
{
	uint result;
	InterlockedExchange(newIndex[index], pointIndex | ((currentBatch & 0x2) << 30), result);

	if (((currentBatch & 0x2) == 0) != ((result & 0x80000000) == 0))
	{
		result = Nil;
	}

	return result;
}

void ClearIndex(uint index, uint pointIndex)
{
	// Clear the previous location only if nothing else has been written to it
	InterlockedCompareStore(newIndex[index], pointIndex | ((~currentBatch) & 0x2) << 30, Nil);
}

[numthreads(blocksize, 1, 1)]
void SimulationStep(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint pointIndex = DTid.x;
	float3 pos = oldPoint[pointIndex].pos.xyz;
	uint lastIndex = PositionToIndex(pos);

	float cosVal;
	float sinVal;

	sincos(3.14 / 60000, sinVal, cosVal);

	float3x3 rotationMatrix = {
		cosVal, -sinVal, 0,
		sinVal, cosVal, 0,
		0, 0, 1
	};

	pos = mul(pos, rotationMatrix);

	uint currentIndex = PositionToIndex(pos);

	if (DTid.x < particleCount)
	{
		uint prevIndexCell = oldPoint[pointIndex].prevIndexCell;
		if (currentIndex != prevIndexCell)
		{
			ClearIndex(prevIndexCell, pointIndex);
		}
		 
		newPoint[pointIndex].pos = float4(pos, 1);
		newPoint[pointIndex].nextPoint = WriteToIndex(currentIndex, pointIndex);
		newPoint[pointIndex].prevIndexCell = lastIndex;
	}
}