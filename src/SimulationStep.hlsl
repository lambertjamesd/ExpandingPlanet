
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
	uint particleCount;
	uint currentBatch;
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
	return oldIndex[index] & 0x78888888;
}

uint WriteToIndex(uint index, uint pointIndex)
{
	uint result;
	InterlockedExchange(newIndex[index], pointIndex, result);

	if ((currentBatch & 0x2) != (result & 0x80000000) >> 30)
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
	float4 pos = oldPoint[pointIndex].pos;
	uint lastIndex = PositionToIndex(pos.xyz);

	// Update position

	uint currentIndex = PositionToIndex(pos.xyz);

	if (DTid.x < particleCount)
	{
		uint prevIndexCell = oldPoint[pointIndex].prevIndexCell;
		if (currentIndex != prevIndexCell)
		{
			ClearIndex(prevIndexCell, pointIndex);
		}

		newPoint[pointIndex].pos = pos;
		newPoint[pointIndex].nextPoint = WriteToIndex(currentIndex, pointIndex);
		newPoint[pointIndex].prevIndexCell = lastIndex;
	}
}