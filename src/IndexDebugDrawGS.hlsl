#include "IndexDebugDraw.hlsl"

[maxvertexcount(MAX_POINTS_PER_CELL)]
void IndexDebugDrawGS(point IndexDebugDrawVSOut input[1], inout LineStream<IndexDebugDrawGSOut> lineStream)
{
	IndexDebugDrawGSOut output;

	uint indexId = input[0].id;
	uint current = g_spacialIndex[indexId] & 0x7FFFFFFF;
	uint i = 0;

	if (current == 0x7FFFFFFF)
	{
		current = Nil;
	}

	output.color = saturate(float4(0.5, 0.25, 0.125 , 1) * indexId);


	while (current != Nil && i < MAX_POINTS_PER_CELL)
	{
		output.pos = mul(float4(g_bufPosVelo[current].pos.xyz, 1), g_mWorldViewProj);
		lineStream.Append(output);

		output.pos += float4(10, 0, 0, 0);
		lineStream.Append(output);

		++i;
		current = g_bufPosVelo[current].nextPoint;
	}
	lineStream.RestartStrip();
}