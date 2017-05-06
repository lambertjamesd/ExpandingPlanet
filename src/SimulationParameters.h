#pragma once

#include "DXSample.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class SimulationParameters
{
public:
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

	float m_startOuterRadius;
	float m_minInnerRadius;
	float m_maxInnerRadius;
	float m_timeStep;
private:
	ID3D12Device& m_device;
	SimulationData* m_data;
	ComPtr<ID3D12Resource> m_constantBufferCS;
	
	float ParticleToSimRadius(float particleRadius, UINT32 particleCount);
	float RadiusToVolume(float radius);
	float VolumeToRadius(float volume);
public:

	SimulationParameters(
		ID3D12Device& device, 
		ID3D12GraphicsCommandList& commandList, 
		float particleRadius, 
		UINT32 particleCount,
		float percentHollow,
		float expandRatio,
		float maxTime,
		float timestep
	);
	~SimulationParameters();

	void PostUpdate();
	ID3D12Resource* GetConstantBuffer();
	SimulationData& GetData();
	UINT32 IndexCellCount();

	float GetStartRadius();
};

