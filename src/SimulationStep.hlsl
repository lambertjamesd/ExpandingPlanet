
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
	uint result = oldIndex[index];

	if (result == Nil)
	{
		return Nil;
	}
	else
	{
		return result & 0x7FFFFFFF;
	}
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

void Overlap(float3 target, float3 other, inout float3 moveAmount)
{
	float3 offset = target - other;

	if (dot(offset, offset) < 4 * particleRadius * particleRadius)
	{
		float3 normal = normalize(offset);
		moveAmount += normal * 0.5 * (2 * particleRadius - dot(normal, offset));
	}
}

void OverlapPointsInCell(float3 startPos, uint cellIndex, inout float3 moveAmount)
{
	uint current = GetFirstPoint(cellIndex);
	uint i = 0;

	while (current != Nil && i < MAX_POINTS_PER_CELL)
	{
		float3 other = oldPoint[current].pos;
		if (any(other != startPos))
		{
			Overlap(startPos, other, moveAmount);
		}
		current = oldPoint[current].nextPoint;
		++i;
	}
}

void OverlapPoints(float3 startPos, inout float3 moveAmount)
{
	uint3 minCell = PostionToCellPosition(startPos - 2 * particleRadius);
	uint3 maxCell = PostionToCellPosition(startPos + 2 * particleRadius);

	uint3 cell;
	uint i = 0;

	for (cell.z = minCell.z; cell.z <= maxCell.z && i < 27; ++cell.z)
	{
		for (cell.y = minCell.y; cell.y <= maxCell.y && i < 27; ++cell.y)
		{
			for (cell.x = minCell.x; cell.x <= maxCell.x && i < 27; ++cell.x)
			{
				uint cellIndex = CellPositionToIndex(cell);
				OverlapPointsInCell(startPos, cellIndex, moveAmount);
				++i;
			}
		}
	}

	if (dot(moveAmount, moveAmount) > particleRadius * particleRadius * 4.0f)
	{
		moveAmount = normalize(moveAmount) * particleRadius * 2;
	}
}

[numthreads(blocksize, 1, 1)]
void SimulationStep(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint pointIndex = DTid.x;
	float3 pos = oldPoint[pointIndex].pos.xyz;
	uint lastIndex = PositionToIndex(pos);
	float3 moveAmount = -normalize(pos) * 0.1;

	OverlapPoints(pos, moveAmount);

	pos += moveAmount;

	if (dot(pos, pos) < coreRadius * coreRadius)
	{
		pos = normalize(pos) * coreRadius;
	}

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