#pragma once

#include "DXSample.h"

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
		UINT32 particleCount;
		UINT32 currentBatch;
	};

	ID3D12Device& m_device;
	SimulationData* m_data;
	ComPtr<ID3D12Resource> m_constantBufferCS;
public:
	SimulationParameters(
		ID3D12Device& device, 
		ID3D12GraphicsCommandList& commandList, 
		float particleRadius, 
		UINT32 particleCount
	);
	~SimulationParameters();

	void PostUpdate();
	ID3D12Resource* GetConstantBuffer();
};

