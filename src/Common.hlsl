
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

#define MAX_POINTS_PER_CELL 64