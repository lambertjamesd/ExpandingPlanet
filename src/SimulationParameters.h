#pragma once

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class SimulationParameters
{
	struct SimulationData
	{
		float particleRadius;
		float maxTime;
		float coreRadius;
		float currentTime;
		float particleVelocity;
		float solidPressure;
		float cellWidth;
		UINT32 indexSize;
	};

	ComPtr<ID3D12Resource> m_constantBufferCS;
public:
	SimulationParameters();
	~SimulationParameters();

	ID3D12Resource* GetConstantBuffer();
};

